package com.fyx.beebot.mqtt

import android.content.Context
import android.content.SharedPreferences
import android.os.Handler
import android.os.Looper
import android.provider.Settings
import android.util.Log
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.fyx.beebot.BuildConfig
import com.fyx.beebot.bt.SmartCarTelemetry
import org.eclipse.paho.client.mqttv3.IMqttActionListener
import org.eclipse.paho.client.mqttv3.IMqttDeliveryToken
import org.eclipse.paho.client.mqttv3.IMqttToken
import org.eclipse.paho.client.mqttv3.MqttAsyncClient
import org.eclipse.paho.client.mqttv3.MqttCallbackExtended
import org.eclipse.paho.client.mqttv3.MqttConnectOptions
import org.eclipse.paho.client.mqttv3.MqttMessage
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence
import org.json.JSONObject

enum class MqttConnectionState(val label: String) {
    DISCONNECTED("MQTT 未连接"), CONNECTING("MQTT 连接中"), CONNECTED("MQTT 已连接"),
    CONFIG_MISSING("MQTT 配置缺失"), ERROR("MQTT 连接异常")
}

/** 公网 MQTT 连接与车辆状态单一数据源。所有 Compose 状态写入都回到主线程。 */
object RemoteVehicleRepository {
    private const val TAG = "BeeBotMqtt"

    var connectionState by mutableStateOf(MqttConnectionState.DISCONNECTED); private set
    var vehicleReportedOnline by mutableStateOf(false); private set
    var lastVehicleMessageAt by mutableStateOf(0L); private set
    var mqttLatencyMs by mutableStateOf<Int?>(null); private set
    var lastLatencyUpdatedAt by mutableStateOf(0L); private set
    var pitch by mutableStateOf<Float?>(null); private set
    var roll by mutableStateOf<Float?>(null); private set
    var yaw by mutableStateOf<Float?>(null); private set
    var lastControlCommand by mutableStateOf<Int?>(null); private set
    var lastControlSequence by mutableStateOf<Int?>(null); private set
    var acceptedControlCount by mutableStateOf(0L); private set
    var rejectedControlCount by mutableStateOf(0L); private set
    var uartFrameCount by mutableStateOf(0L); private set
    var uartErrorCount by mutableStateOf(0L); private set
    var telemetryMessageCount by mutableStateOf(0L); private set
    var telemetrySubscribed by mutableStateOf(false); private set
    var lastPublishError by mutableStateOf(""); private set
    var lastError by mutableStateOf(""); private set

    private val main = Handler(Looper.getMainLooper())
    private var client: MqttAsyncClient? = null
    private var connectOptions: MqttConnectOptions? = null
    private var initialized = false
    private var connecting = false
    private var nextControlSequence = 0
    private var controlPreferences: SharedPreferences? = null
    private val publishedAtBySequence = LongArray(256)
    private val retryConnect = Runnable { connectInitial() }

    val brokerConnected: Boolean get() = connectionState == MqttConnectionState.CONNECTED

    fun initialize(context: Context) {
        if (initialized) return
        initialized = true
        if (BuildConfig.MQTT_WSS_URL.isBlank() || BuildConfig.MQTT_USERNAME.isBlank() || BuildConfig.MQTT_PASSWORD.isBlank()) {
            connectionState = MqttConnectionState.CONFIG_MISSING
            lastError = "受控 MQTT 配置未注入"
            return
        }
        controlPreferences = context.applicationContext
            .getSharedPreferences("mqtt_control_state", Context.MODE_PRIVATE)
        nextControlSequence = controlPreferences
            ?.getInt("next_control_sequence", 0)
            ?.and(0xFF) ?: 0
        val androidId = Settings.Secure.getString(context.contentResolver, Settings.Secure.ANDROID_ID)
            ?.replace(Regex("[^A-Za-z0-9]"), "")?.takeLast(16) ?: "unknown"
        val mqttClient = MqttAsyncClient(BuildConfig.MQTT_WSS_URL, "controller-$androidId", MemoryPersistence())
        client = mqttClient
        mqttClient.setCallback(object : MqttCallbackExtended {
            override fun connectComplete(reconnect: Boolean, serverURI: String?) {
                updateMain {
                    connecting = false
                    connectionState = MqttConnectionState.CONNECTED
                    lastError = ""
                }
                main.removeCallbacks(retryConnect)
                subscribeAll()
            }

            override fun connectionLost(cause: Throwable?) {
                Log.w(TAG, "MQTT connection lost: ${cause.describeChain()}", cause)
                updateMain {
                    connectionState = MqttConnectionState.DISCONNECTED
                    vehicleReportedOnline = false
                    mqttLatencyMs = null
                    lastLatencyUpdatedAt = 0L
                    lastError = cause.describeChain()
                }
            }

            override fun messageArrived(topic: String, message: MqttMessage) {
                val payload = message.payload ?: return
                when (topic) {
                    BuildConfig.MQTT_STATUS_TOPIC -> parseStatus(payload)
                    BuildConfig.MQTT_TELEMETRY_TOPIC -> parseTelemetry(payload)
                }
            }

            override fun deliveryComplete(token: IMqttDeliveryToken?) = Unit
        })
        connectOptions = MqttConnectOptions().apply {
            isCleanSession = true
            isAutomaticReconnect = true
            keepAliveInterval = 30
            connectionTimeout = 12
            userName = BuildConfig.MQTT_USERNAME
            password = BuildConfig.MQTT_PASSWORD.toCharArray()
        }
        connectInitial()
    }

    /** Paho 自动重连只覆盖“曾经连接成功后掉线”；首次握手失败需由应用主动重试。 */
    private fun connectInitial() {
        val mqttClient = client ?: return
        val options = connectOptions ?: return
        if (!initialized || mqttClient.isConnected || connecting) return
        connecting = true
        connectionState = MqttConnectionState.CONNECTING
        mqttClient.connect(options, null, object : IMqttActionListener {
            override fun onSuccess(asyncActionToken: IMqttToken?) = Unit
            override fun onFailure(asyncActionToken: IMqttToken?, exception: Throwable?) {
                Log.e(TAG, "Initial MQTT connection failed: ${exception.describeChain()}", exception)
                updateMain {
                    connecting = false
                    connectionState = MqttConnectionState.ERROR
                    lastError = exception.describeChain().ifBlank { "连接失败" }
                    main.removeCallbacks(retryConnect)
                    main.postDelayed(retryConnect, 5_000L)
                }
            }
        })
    }

    private fun subscribeAll() {
        val mqttClient = client ?: return
        if (!mqttClient.isConnected) return
        subscribeTopic(BuildConfig.MQTT_TELEMETRY_TOPIC) { telemetrySubscribed = true }
        subscribeTopic(BuildConfig.MQTT_STATUS_TOPIC)
    }

    private fun subscribeTopic(topic: String, onSuccess: () -> Unit = {}) {
        val mqttClient = client ?: return
        mqttClient.subscribe(topic, 0, null, object : IMqttActionListener {
            override fun onSuccess(asyncActionToken: IMqttToken?) = updateMain(onSuccess)
            override fun onFailure(asyncActionToken: IMqttToken?, exception: Throwable?) = updateMain {
                lastError = "订阅失败 $topic: ${exception?.message.orEmpty()}"
            }
        })
    }

    private fun parseStatus(payload: ByteArray) {
        val text = payload.toString(Charsets.UTF_8).trim()
        var online = text.equals("online", true)
        var latency: Int? = null
        var acknowledgedSequence: Int? = null
        var acknowledgedCommand: Int? = null
        var controlRx: Long? = null
        var controlErrors: Long? = null
        var uartFrames: Long? = null
        var uartErrors: Long? = null
        runCatching {
            val json = JSONObject(text)
            online = json.optBoolean("online", json.optString("status").equals("online", true))
            latency = when {
                json.has("rtt_ms") -> json.optInt("rtt_ms")
                json.has("latency_ms") -> json.optInt("latency_ms")
                else -> null
            }
            if (json.has("last_seq")) acknowledgedSequence = json.optInt("last_seq") and 0xFF
            if (json.has("last_cmd")) acknowledgedCommand = json.optInt("last_cmd") and 0xFF
            if (json.has("control_rx")) controlRx = json.optLong("control_rx")
            if (json.has("control_errors")) controlErrors = json.optLong("control_errors")
            if (json.has("uart_frames")) uartFrames = json.optLong("uart_frames")
            if (json.has("uart_errors")) uartErrors = json.optLong("uart_errors")
        }
        updateMain {
            vehicleReportedOnline = online
            lastVehicleMessageAt = System.currentTimeMillis()
            if (latency != null && latency!! >= 0) {
                mqttLatencyMs = latency
                lastLatencyUpdatedAt = System.currentTimeMillis()
            }
            acknowledgedSequence?.let { sequence ->
                lastControlSequence = sequence
                val sentAt = publishedAtBySequence[sequence]
                Log.i(TAG, "Control status seq=$sequence pendingAt=$sentAt")
                if (sentAt > 0L) {
                    val measuredLatency = (System.currentTimeMillis() - sentAt)
                        .coerceAtLeast(0L)
                        .coerceAtMost(Int.MAX_VALUE.toLong())
                        .toInt()
                    mqttLatencyMs = measuredLatency
                    lastLatencyUpdatedAt = System.currentTimeMillis()
                    publishedAtBySequence[sequence] = 0L
                    Log.d(TAG, "Control ACK seq=$sequence latency=${measuredLatency}ms")
                }
            }
            acknowledgedCommand?.let { lastControlCommand = it }
            controlRx?.let { acceptedControlCount = it }
            controlErrors?.let { rejectedControlCount = it }
            uartFrames?.let { uartFrameCount = it }
            uartErrors?.let { uartErrorCount = it }
        }
    }

    private fun parseTelemetry(payload: ByteArray) {
        val now = System.currentTimeMillis()
        updateMain { telemetryMessageCount++ }
        if (payload.firstOrNull()?.toInt()?.and(0xFF) == 0x55) {
            SmartCarTelemetry.feedRx(payload)
            updateMain { vehicleReportedOnline = true; lastVehicleMessageAt = now }
            return
        }
        val text = payload.toString(Charsets.UTF_8).trim()
        runCatching {
            val json = JSONObject(text)
            updateMain {
                pitch = json.optDouble("pitch", Double.NaN).takeIf { !it.isNaN() }?.toFloat()
                roll = json.optDouble("roll", Double.NaN).takeIf { !it.isNaN() }?.toFloat()
                yaw = json.optDouble("yaw", Double.NaN).takeIf { !it.isNaN() }?.toFloat()
                vehicleReportedOnline = true
                lastVehicleMessageAt = now
            }
        }.onFailure { error -> updateMain { lastError = "遥测格式错误: ${error.message.orEmpty()}" } }
    }

    fun isVehicleOnline(now: Long = System.currentTimeMillis()): Boolean =
        brokerConnected && vehicleReportedOnline && now - lastVehicleMessageAt <= 5_000L

    /** 延迟超过有效窗口后不再显示旧值；主控台时钟会持续触发该结果刷新。 */
    fun freshLatencyMs(now: Long = System.currentTimeMillis(), maxAgeMs: Long = 5_000L): Int? =
        mqttLatencyMs?.takeIf { lastLatencyUpdatedAt > 0L && now - lastLatencyUpdatedAt in 0..maxAgeMs }

    /** 发布 ESP32-C3 实际接受的原始 20 字节 Payload；禁止 retain。 */
    @Synchronized
    fun publishControl(command: Int): Boolean {
        val mqttClient = client ?: return false
        if (!mqttClient.isConnected || !isVehicleOnline()) return false
        val sequence = nextControlSequence and 0xFF
        val frame = runCatching { RemoteControlProtocol.encode(sequence, command) }
            .getOrElse { error -> updateMain { lastPublishError = error.message.orEmpty() }; return false }
        publishedAtBySequence[sequence] = System.currentTimeMillis()
        return runCatching {
            mqttClient.publish(BuildConfig.MQTT_CONTROL_TOPIC, MqttMessage(frame).apply {
                qos = 0
                isRetained = false
            })
            nextControlSequence = (sequence + 1) and 0xFF
            controlPreferences?.edit()
                ?.putInt("next_control_sequence", nextControlSequence)
                ?.apply()
            updateMain { lastPublishError = "" }
            true
        }.getOrElse { error ->
            publishedAtBySequence[sequence] = 0L
            updateMain { lastPublishError = error.message ?: "控制发布失败" }
            false
        }
    }

    fun publishStop(repeat: Int = 3): Boolean {
        var sent = false
        repeat(repeat.coerceIn(1, 3)) { sent = publishControl(RemoteControlProtocol.STOP) || sent }
        return sent
    }

    fun disconnect() {
        publishStop()
        initialized = false
        connecting = false
        main.removeCallbacks(retryConnect)
        runCatching { client?.disconnectForcibly(500, 500, false) }
        runCatching { client?.close() }
        client = null
        connectOptions = null
        connectionState = MqttConnectionState.DISCONNECTED
        vehicleReportedOnline = false
        mqttLatencyMs = null
        lastLatencyUpdatedAt = 0L
        telemetrySubscribed = false
        publishedAtBySequence.fill(0L)
    }

    private fun updateMain(block: () -> Unit) {
        if (Looper.myLooper() == Looper.getMainLooper()) block() else main.post(block)
    }

    private fun Throwable?.describeChain(): String {
        if (this == null) return ""
        val parts = mutableListOf<String>()
        val visited = mutableSetOf<Throwable>()
        var current: Throwable? = this
        while (current != null && parts.size < 6) {
            val error = current
            if (!visited.add(error)) break
            val type = error.javaClass.simpleName.ifBlank { error.javaClass.name }
            val message = error.message?.trim().orEmpty()
            parts += if (message.isBlank()) type else "$type: $message"
            current = error.cause
        }
        return parts.joinToString(" -> ")
    }
}
