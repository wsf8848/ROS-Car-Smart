package com.fyx.beebot.speech

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.os.BatteryManager
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.fyx.beebot.BuildConfig
import com.fyx.beebot.bt.SmartCarTelemetry
import com.fyx.beebot.mqtt.RemoteVehicleRepository
import com.fyx.beebot.util.VoicePrompts
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

enum class ConversationRole { USER, ASSISTANT }

data class VoiceConversationMessage(
    val id: Long,
    val role: ConversationRole,
    val speakerName: String,
    val text: String
)

/** 把最终识别句、真实状态查询和 TTS 串成完全离线的确定性对话闭环。 */
object VoiceAssistantController {
    val messages = mutableStateListOf<VoiceConversationMessage>()

    var active by mutableStateOf(false)
        private set
    var speaking by mutableStateOf(false)
        private set
    var lastIntent by mutableStateOf<VoiceIntent?>(null)
        private set

    private val main = Handler(Looper.getMainLooper())
    private var appContext: Context? = null
    private var nextMessageId = 1L

    fun attach(context: Context) {
        appContext = context.applicationContext
        VoicePrompts.initialize(context)
        SpeechRecognitionController.finalSegmentListener = ::onFinalSegment
    }

    fun start(context: Context) {
        attach(context)
        if (speaking) return
        active = true
        SpeechRecognitionController.start(context)
    }

    fun pause() {
        active = false
        SpeechRecognitionController.stop()
    }

    fun detach() {
        active = false
        speaking = false
        main.removeCallbacksAndMessages(null)
        SpeechRecognitionController.finalSegmentListener = null
        SpeechRecognitionController.stop()
        appContext = null
    }

    fun clear() {
        messages.clear()
        lastIntent = null
        SpeechRecognitionController.clearText()
    }

    fun transcript(): String = messages.joinToString("\n") { message ->
        "${message.speakerName}：${message.text}"
    }

    private fun onFinalSegment(speakerName: String?, text: String) {
        if (!active || speaking || text.isBlank()) return
        val context = appContext ?: return
        SpeechRecognitionController.stop()
        appendMessage(ConversationRole.USER, speakerName?.takeIf { it.isNotBlank() } ?: "未知说话人", text)

        val reply = VoiceAssistantEngine.reply(text, createSnapshot(context))
        lastIntent = reply.intent
        appendMessage(ConversationRole.ASSISTANT, "小蜜蜂助手", reply.text)
        speaking = true
        VoicePrompts.speak(reply.text) {
            speaking = false
            if (active) {
                main.postDelayed({
                    appContext?.takeIf { active }?.let(SpeechRecognitionController::start)
                }, 350L)
            }
        }
    }

    private fun appendMessage(role: ConversationRole, speakerName: String, text: String) {
        if (messages.size >= 60) messages.removeRange(0, 2.coerceAtMost(messages.size))
        messages += VoiceConversationMessage(nextMessageId++, role, speakerName, text)
    }

    @Suppress("DEPRECATION")
    private fun createSnapshot(context: Context): DeviceStatusSnapshot {
        val battery = context.registerReceiver(null, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val level = battery?.getIntExtra(BatteryManager.EXTRA_LEVEL, -1) ?: -1
        val scale = battery?.getIntExtra(BatteryManager.EXTRA_SCALE, -1) ?: -1
        val devicePercent = if (level >= 0 && scale > 0) (level * 100 / scale).coerceIn(0, 100) else null
        val status = battery?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        val charging = when (status) {
            BatteryManager.BATTERY_STATUS_CHARGING, BatteryManager.BATTERY_STATUS_FULL -> true
            BatteryManager.BATTERY_STATUS_DISCHARGING, BatteryManager.BATTERY_STATUS_NOT_CHARGING -> false
            else -> null
        }
        val temperatureTenths = battery?.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, Int.MIN_VALUE)
            ?: Int.MIN_VALUE
        val temperature = temperatureTenths.takeIf { it != Int.MIN_VALUE }?.div(10f)

        val vehicleOnline = RemoteVehicleRepository.isVehicleOnline()
        val powerFresh = SmartCarTelemetry.powerAgeMs().let { it in 0..SmartCarTelemetry.TELEMETRY_TIMEOUT_MS }
        val radarFresh = SmartCarTelemetry.radarAgeMs().let { it in 0..SmartCarTelemetry.TELEMETRY_TIMEOUT_MS }

        return DeviceStatusSnapshot(
            deviceBatteryPercent = devicePercent,
            charging = charging,
            batteryTemperatureC = temperature,
            dateTimeText = SimpleDateFormat("yyyy年M月d日EEEEH点m分", Locale.CHINA).format(Date()),
            networkDescription = networkDescription(context),
            appVersion = BuildConfig.VERSION_NAME,
            vehicleOnline = vehicleOnline,
            vehicleBatteryPercent = SmartCarTelemetry.powerPercent.takeIf { vehicleOnline && powerFresh },
            vehicleVoltageV = SmartCarTelemetry.powerVoltageDeciVolt
                ?.takeIf { vehicleOnline && powerFresh }?.div(10f),
            communicationLatencyMs = RemoteVehicleRepository.freshLatencyMs().takeIf { vehicleOnline },
            frontDistanceCm = SmartCarTelemetry.frontDistanceCm.takeIf { vehicleOnline && radarFresh },
            rearDistanceCm = SmartCarTelemetry.rearDistanceCm.takeIf { vehicleOnline && radarFresh },
            leftDistanceCm = SmartCarTelemetry.leftDistanceCm.takeIf { vehicleOnline && radarFresh },
            rightDistanceCm = SmartCarTelemetry.rightDistanceCm.takeIf { vehicleOnline && radarFresh },
            vehicleMode = SmartCarTelemetry.carStatusText().takeIf { vehicleOnline && radarFresh }
        )
    }

    private fun networkDescription(context: Context): String {
        val manager = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val network = manager.activeNetwork ?: return "掌机当前没有可用网络连接。"
        val capabilities = manager.getNetworkCapabilities(network)
            ?: return "掌机检测到网络连接，但暂时无法读取网络类型。"
        val type = when {
            capabilities.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) -> "无线网络"
            capabilities.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET) -> "有线网络"
            capabilities.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR) -> "移动网络"
            else -> "网络"
        }
        return if (capabilities.hasCapability(NetworkCapabilities.NET_CAPABILITY_VALIDATED)) {
            "掌机当前通过${type}连接，网络可用。"
        } else {
            "掌机当前已连接${type}，但系统尚未确认互联网可用。"
        }
    }
}
