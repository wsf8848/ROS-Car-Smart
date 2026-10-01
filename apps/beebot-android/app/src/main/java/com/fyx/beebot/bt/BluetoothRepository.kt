package com.fyx.beebot.bt

import android.annotation.SuppressLint
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothSocket
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import com.fyx.beebot.util.Haptics
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import java.util.UUID
import java.util.concurrent.Executors

/**
 * 经典蓝牙 SPP 收发管理（单例，串口助手数据源）。
 *
 * 连接/读取在独立线程；所有 Compose 快照状态的写入都 post 回主线程，保证界面安全刷新。
 * ⚠️ HC-05/HC-06 需先在系统蓝牙设置里配对（PIN 一般 1234/0000），这里列出已配对设备直接连。
 */
object BluetoothRepository {

    private val SPP_UUID: UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB")
    private const val MAX_FRAMES = 500
    private const val PREFS_NAME = "beebot_prefs"
    private const val KEY_LAST_DEVICE_ADDRESS = "bt_last_device_address"
    private const val KEY_LAST_DEVICE_NAME = "bt_last_device_name"

    var connected by mutableStateOf(false); private set
    var connecting by mutableStateOf(false); private set
    var deviceName by mutableStateOf(""); private set
    var statusMsg by mutableStateOf("未连接"); private set

    val frames = mutableStateListOf<BtFrame>()   // 最新在前
    var rxBytes by mutableStateOf(0L); private set
    var txBytes by mutableStateOf(0L); private set
    var rxIntervalMs by mutableStateOf(-1); private set   // 相邻两帧接收间隔

    val paired = mutableStateListOf<BtDevice>()

    /** 原始接收字节回调（供阿克曼遥测解析等；每次收到数据在主线程回调）。 */
    var rxListener: ((ByteArray) -> Unit)? = null

    /** 高频二进制遥测回调：直接在蓝牙读取线程触发，避免摇杆刷新阻塞主线程时误判超时。 */
    @Volatile var backgroundRxListener: ((ByteArray, Long) -> Unit)? = null

    /** 控制页关闭原始帧列表记录，避免高频二进制遥测持续触发列表重组。 */
    @Volatile var frameLoggingEnabled = true

    private val main = Handler(Looper.getMainLooper())
    private val writeLock = Any()
    private val writer = Executors.newSingleThreadExecutor { task ->
        Thread(task, "BeeBot-BluetoothWriter").apply { isDaemon = true }
    }
    private var socket: BluetoothSocket? = null
    private var reader: Thread? = null
    private var lastRxTime = 0L
    @Volatile private var running = false

    @SuppressLint("MissingPermission")
    fun refreshPaired(context: Context) {
        try {
            val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
            if (adapter == null || !adapter.isEnabled) {
                statusMsg = "蓝牙未开启"
                return
            }
            paired.clear()
            adapter.bondedDevices?.forEach { paired.add(BtDevice(it.name ?: "未知设备", it.address)) }
            if (!connected && !connecting) statusMsg = "未连接"
            if (paired.isEmpty()) addSys("无已配对设备，请先在系统蓝牙里配对 HC-05")
        } catch (e: Exception) {
            addSys("读取配对设备失败: ${e.message}")
        }
    }

    @SuppressLint("MissingPermission")
    fun connect(context: Context, device: BtDevice) {
        if (connecting || connected) return
        connecting = true
        statusMsg = "连接中…"
        addSys("正在连接 ${device.name} …")
        Thread {
            try {
                val adapter = (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager).adapter
                adapter?.cancelDiscovery()
                val dev = adapter?.getRemoteDevice(device.address)
                val sock = dev!!.createRfcommSocketToServiceRecord(SPP_UUID)
                sock.connect()
                socket = sock
                main.post {
                    connected = true; connecting = false
                    deviceName = device.name; statusMsg = "已连接 ${device.name}"
                    context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE).edit()
                        .putString(KEY_LAST_DEVICE_ADDRESS, device.address)
                        .putString(KEY_LAST_DEVICE_NAME, device.name)
                        .apply()
                    addSys("已连接 ${device.name}")
                    Haptics.heavy() // 连接成功强震
                }
                readLoop(sock)
            } catch (e: Exception) {
                try { socket?.close() } catch (_: Exception) {}
                socket = null
                main.post {
                    connecting = false; connected = false; statusMsg = "连接失败"
                    addSys("连接失败: ${e.message}")
                }
            }
        }.start()
    }

    /** 自动连接最近一次成功连接的配对设备；没有历史记录时保持未连接并提示用户。 */
    @SuppressLint("MissingPermission")
    fun connectLastDevice(context: Context): Boolean {
        if (connected || connecting) return true
        refreshPaired(context)
        val prefs = context.applicationContext.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val address = prefs.getString(KEY_LAST_DEVICE_ADDRESS, null)
        if (address.isNullOrBlank()) {
            statusMsg = "无历史蓝牙设备，请先在蓝牙调试页连接"
            addSys(statusMsg)
            return false
        }
        val savedName = prefs.getString(KEY_LAST_DEVICE_NAME, null)
        val device = paired.firstOrNull { it.address.equals(address, ignoreCase = true) }
        if (device == null) {
            statusMsg = "历史蓝牙设备未配对"
            addSys("${savedName ?: address} 已不在系统配对列表")
            return false
        }
        connect(context.applicationContext, device)
        return true
    }

    private fun readLoop(sock: BluetoothSocket) {
        running = true
        reader = Thread {
            val buf = ByteArray(1024)
            val ins = try { sock.inputStream } catch (e: Exception) { null } ?: return@Thread
            while (running) {
                try {
                    val n = ins.read(buf)
                    if (n > 0) {
                        val data = buf.copyOf(n)
                        val now = SystemClock.elapsedRealtime()
                        val interval = if (lastRxTime > 0) (now - lastRxTime).toInt() else -1
                        lastRxTime = now
                        backgroundRxListener?.invoke(data, now)
                        main.post {
                            rxBytes += n
                            if (interval >= 0) rxIntervalMs = interval
                            if (frameLoggingEnabled) addFrame(BtFrame(System.currentTimeMillis(), BtDir.RX, data))
                            rxListener?.invoke(data)
                        }
                    }
                } catch (e: Exception) {
                    main.post {
                        if (connected) { connected = false; statusMsg = "连接断开"; addSys("连接断开") }
                    }
                    break
                }
            }
        }.also { it.start() }
    }

    fun send(bytes: ByteArray) {
        val out = try { socket?.outputStream } catch (e: Exception) { null }
        if (out == null || !connected) { statusMsg = "未连接，无法发送"; return }
        writer.execute {
            try {
                synchronized(writeLock) { out.write(bytes); out.flush() }
                main.post { txBytes += bytes.size; addFrame(BtFrame(System.currentTimeMillis(), BtDir.TX, bytes)) }
            } catch (e: Exception) {
                main.post { statusMsg = "发送失败: ${e.message}" }
            }
        }
    }

    /** 发送控制字节（不写入收发日志，避免高频遥控刷屏；仅累计 TX 统计）。 */
    fun sendRaw(bytes: ByteArray) {
        val out = try { socket?.outputStream } catch (e: Exception) { null }
        if (out == null || !connected) return
        writer.execute {
            try {
                synchronized(writeLock) { out.write(bytes); out.flush() }
                main.post { txBytes += bytes.size }
            } catch (e: Exception) { }
        }
    }

    fun disconnect() {
        running = false
        try { socket?.close() } catch (_: Exception) {}
        socket = null
        connected = false; connecting = false; statusMsg = "未连接"
        rxIntervalMs = -1; lastRxTime = 0
        backgroundRxListener = null
        frameLoggingEnabled = true
        addSys("已断开")
    }

    fun clearLog() {
        frames.clear(); rxBytes = 0; txBytes = 0; rxIntervalMs = -1
    }

    private fun addFrame(f: BtFrame) {
        frames.add(0, f)
        while (frames.size > MAX_FRAMES) frames.removeAt(frames.size - 1)
    }

    private fun addSys(msg: String) = addFrame(BtFrame(System.currentTimeMillis(), BtDir.SYS, sysMsg = msg))
}
