package com.fyx.beebot.input

import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.fyx.beebot.util.Haptics
import kotlin.math.abs

/**
 * 手柄输入采集层（单一数据源）。
 *
 * 职责：把 Android 底层的 [MotionEvent]（摇杆/扳机模拟量）与 [KeyEvent]（按键）
 * 归一化成业务模型 [GamepadState]，并维护一份实时信号日志。
 *
 * 使用 Compose 快照状态（mutableStateOf / mutableStateListOf），
 * 界面读取即自动订阅，输入变化即自动刷新，无需手写回调。
 *
 * 输入事件都在主线程分发，读写无并发问题。
 */
object GamepadRepository {

    var state by mutableStateOf(GamepadState())
        private set

    /** 实时信号日志，最新的在最前面，最多保留 [MAX_LOG] 条。 */
    val events = mutableStateListOf<InputEvent>()

    private const val MAX_LOG = 200
    private const val AXIS_LOG_INTERVAL_MS = 120L
    private var lastAxisLogTime = 0L

    /** 启动时扫描已连接的手柄设备（掌机内置手柄）。 */
    fun refreshConnectedDevices() {
        var found: InputDevice? = null
        for (id in InputDevice.getDeviceIds()) {
            val dev = InputDevice.getDevice(id) ?: continue
            val src = dev.sources
            val isGamepad = (src and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD) ||
                    (src and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)
            if (isGamepad && !dev.isVirtual) {
                found = dev
                break
            }
        }
        if (found != null) {
            state = state.copy(connected = true, deviceName = found.name, seq = state.seq + 1)
            addEvent(InputEvent.Type.SYSTEM, "检测到手柄：${found.name}")
        } else {
            state = state.copy(connected = false, seq = state.seq + 1)
            addEvent(InputEvent.Type.SYSTEM, "未检测到手柄设备")
        }
    }

    /** 处理摇杆/扳机模拟量事件。返回是否已消费。 */
    fun processMotionEvent(event: MotionEvent): Boolean {
        val isJoystick = event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK
        if (!isJoystick || event.action != MotionEvent.ACTION_MOVE) return false

        val dev = event.device
        val src = event.source

        val lx = centeredAxis(event, dev, MotionEvent.AXIS_X, src)
        val ly = centeredAxis(event, dev, MotionEvent.AXIS_Y, src)
        // 右摇杆：Android 主流映射为 Z(横) / RZ(纵)，部分设备用 RX / RY
        val rx = centeredAxis(event, dev, pickAxis(dev, src, MotionEvent.AXIS_Z, MotionEvent.AXIS_RX), src)
        val ry = centeredAxis(event, dev, pickAxis(dev, src, MotionEvent.AXIS_RZ, MotionEvent.AXIS_RY), src)
        // 扳机：优先 LTRIGGER/RTRIGGER，回退 BRAKE/GAS
        val lt = triggerAxis(event, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE)
        val rt = triggerAxis(event, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS)
        val hx = event.getAxisValue(MotionEvent.AXIS_HAT_X)
        val hy = event.getAxisValue(MotionEvent.AXIS_HAT_Y)

        val name = dev?.name ?: state.deviceName
        state = state.copy(
            leftX = lx, leftY = ly, rightX = rx, rightY = ry,
            leftTrigger = lt, rightTrigger = rt, hatX = hx, hatY = hy,
            connected = true, deviceName = name, seq = state.seq + 1
        )

        val now = event.eventTime
        if (now - lastAxisLogTime >= AXIS_LOG_INTERVAL_MS &&
            (abs(lx) > 0.02f || abs(ly) > 0.02f || abs(rx) > 0.02f || abs(ry) > 0.02f ||
                    lt > 0.02f || rt > 0.02f || abs(hx) > 0.5f || abs(hy) > 0.5f)
        ) {
            lastAxisLogTime = now
            addEvent(
                InputEvent.Type.AXIS,
                "LX=%+.2f LY=%+.2f RX=%+.2f RY=%+.2f L2=%.2f R2=%.2f".format(lx, ly, rx, ry, lt, rt)
            )
        }
        return true
    }

    /** 处理按键事件。返回是否已消费。 */
    fun processKeyEvent(event: KeyEvent): Boolean {
        val btn = GamepadButton.fromKeyCode(event.keyCode) ?: return false
        when (event.action) {
            KeyEvent.ACTION_DOWN -> {
                if (event.repeatCount == 0 && !state.buttons.contains(btn)) {
                    state = state.copy(
                        buttons = state.buttons + btn,
                        connected = true,
                        deviceName = event.device?.name ?: state.deviceName,
                        seq = state.seq + 1
                    )
                    addEvent(InputEvent.Type.KEY_DOWN, "${btn.label} (键码 ${event.keyCode})")
                    Haptics.tick() // 实体手柄键按下轻震
                }
            }
            KeyEvent.ACTION_UP -> {
                if (state.buttons.contains(btn)) {
                    state = state.copy(buttons = state.buttons - btn, seq = state.seq + 1)
                    addEvent(InputEvent.Type.KEY_UP, "${btn.label} (键码 ${event.keyCode})")
                }
            }
        }
        return true
    }

    fun clearLog() = events.clear()

    // ---- 内部工具 ----

    private fun addEvent(type: InputEvent.Type, message: String) {
        events.add(0, InputEvent(System.currentTimeMillis(), type, message))
        while (events.size > MAX_LOG) events.removeAt(events.size - 1)
    }

    /** 选择设备实际支持的轴（存在 [primary] 用之，否则用 [fallback]）。 */
    private fun pickAxis(device: InputDevice?, source: Int, primary: Int, fallback: Int): Int {
        if (device?.getMotionRange(primary, source) != null) return primary
        if (device?.getMotionRange(fallback, source) != null) return fallback
        return primary
    }

    /** 读取带死区处理的居中轴值（摇杆）。 */
    private fun centeredAxis(event: MotionEvent, device: InputDevice?, axis: Int, source: Int): Float {
        val range = device?.getMotionRange(axis, source)
        val value = event.getAxisValue(axis)
        val flat = range?.flat ?: 0.05f
        return if (abs(value) > flat) value else 0f
    }

    /** 读取扳机轴值（0~1），带轻微死区。 */
    private fun triggerAxis(event: MotionEvent, primary: Int, fallback: Int): Float {
        var v = event.getAxisValue(primary)
        if (v == 0f) v = event.getAxisValue(fallback)
        return if (v > 0.03f) v.coerceIn(0f, 1f) else 0f
    }
}
