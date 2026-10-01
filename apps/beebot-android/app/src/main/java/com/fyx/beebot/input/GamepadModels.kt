package com.fyx.beebot.input

import android.view.KeyEvent

/**
 * 手柄按键枚举 —— 将 Android KeyEvent 键码映射为业务可读的按键。
 * 后续接入机器狗 / 机器车时，控制协议直接引用这里的枚举，无需再关心底层键码。
 */
enum class GamepadButton(val keyCode: Int, val label: String) {
    A(KeyEvent.KEYCODE_BUTTON_A, "A"),
    B(KeyEvent.KEYCODE_BUTTON_B, "B"),
    X(KeyEvent.KEYCODE_BUTTON_X, "X"),
    Y(KeyEvent.KEYCODE_BUTTON_Y, "Y"),
    L1(KeyEvent.KEYCODE_BUTTON_L1, "L1"),
    R1(KeyEvent.KEYCODE_BUTTON_R1, "R1"),
    L2(KeyEvent.KEYCODE_BUTTON_L2, "L2"),
    R2(KeyEvent.KEYCODE_BUTTON_R2, "R2"),
    L3(KeyEvent.KEYCODE_BUTTON_THUMBL, "L3"),
    R3(KeyEvent.KEYCODE_BUTTON_THUMBR, "R3"),
    START(KeyEvent.KEYCODE_BUTTON_START, "START"),
    SELECT(KeyEvent.KEYCODE_BUTTON_SELECT, "SELECT"),
    MODE(KeyEvent.KEYCODE_BUTTON_MODE, "MODE"),
    DPAD_UP(KeyEvent.KEYCODE_DPAD_UP, "上"),
    DPAD_DOWN(KeyEvent.KEYCODE_DPAD_DOWN, "下"),
    DPAD_LEFT(KeyEvent.KEYCODE_DPAD_LEFT, "左"),
    DPAD_RIGHT(KeyEvent.KEYCODE_DPAD_RIGHT, "右");

    companion object {
        private val byCode = values().associateBy { it.keyCode }
        fun fromKeyCode(code: Int): GamepadButton? = byCode[code]
    }
}

/**
 * 手柄完整状态快照（不可变）。任何一次输入变化都会产生一个新的快照，
 * Compose 依据快照差异自动刷新界面。
 *
 * 摇杆分量取值范围 -1.0 ~ +1.0，扳机 0.0 ~ 1.0。
 */
data class GamepadState(
    val leftX: Float = 0f,
    val leftY: Float = 0f,
    val rightX: Float = 0f,
    val rightY: Float = 0f,
    val leftTrigger: Float = 0f,   // L2 模拟量
    val rightTrigger: Float = 0f,  // R2 模拟量
    val hatX: Float = 0f,          // 方向键横轴 -1/0/1
    val hatY: Float = 0f,          // 方向键纵轴 -1/0/1
    val buttons: Set<GamepadButton> = emptySet(),
    val deviceName: String = "",
    val connected: Boolean = false,
    val seq: Long = 0L             // 序号，保证每次更新都能触发刷新
) {
    fun isPressed(btn: GamepadButton): Boolean = when (btn) {
        GamepadButton.DPAD_UP -> buttons.contains(btn) || hatY < -0.5f
        GamepadButton.DPAD_DOWN -> buttons.contains(btn) || hatY > 0.5f
        GamepadButton.DPAD_LEFT -> buttons.contains(btn) || hatX < -0.5f
        GamepadButton.DPAD_RIGHT -> buttons.contains(btn) || hatX > 0.5f
        else -> buttons.contains(btn)
    }
}

/** 实时信号日志的一条记录。 */
data class InputEvent(
    val timestamp: Long,
    val type: Type,
    val message: String
) {
    enum class Type { KEY_DOWN, KEY_UP, AXIS, SYSTEM }
}
