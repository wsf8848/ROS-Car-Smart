package com.fyx.beebot.mqtt

import kotlin.math.abs

/** ESP32-C3 `55 43 01` 远程控制协议的纯 Kotlin 编解码规则。 */
object RemoteControlProtocol {
    const val FRAME_LENGTH = 20
    const val STOP = 0x49
    const val MODE_MANUAL = 0x5B
    const val MODE_AVOIDANCE = 0x5C
    const val MODE_AUTONOMOUS = 0x5D
    const val HORN_ON = 0x5E
    const val HORN_OFF = 0x5F
    const val LIGHT_OFF = 0x60
    const val LIGHT_WATER = 0x61
    const val LIGHT_FLASH = 0x62
    const val SPEED_DOWN = 0x6A
    const val SPEED_UP = 0x6B
    const val DEADZONE = 0.25f

    fun encode(sequence: Int, command: Int): ByteArray {
        require(command in 0x41..0x49 || command in 0x5B..0x5F || command in 0x60..0x62 || command in 0x6A..0x6B) {
            "Command 0x${command.toString(16)} is not accepted by the ESP32-C3 bridge"
        }
        return ByteArray(FRAME_LENGTH).apply {
            this[0] = 0x55
            this[1] = 0x43
            this[2] = 0x01
            this[3] = sequence.toByte()
            this[4] = command.toByte()
            this[18] = crc8Atm(this, 18).toByte()
            this[19] = 0x5B
        }
    }

    /** 左摇杆上为负值；右摇杆右为正值。 */
    fun driveCommand(leftY: Float, rightX: Float): Int {
        val forward = leftY < -DEADZONE
        val backward = leftY > DEADZONE
        val left = rightX < -DEADZONE
        val right = rightX > DEADZONE
        return when {
            forward && left -> 0x48
            forward && right -> 0x42
            forward -> 0x41
            backward && left -> 0x46
            backward && right -> 0x44
            backward -> 0x45
            left -> 0x47
            right -> 0x43
            else -> STOP
        }
    }

    fun hasDriveInput(leftY: Float, rightX: Float): Boolean =
        abs(leftY) > DEADZONE || abs(rightX) > DEADZONE

    fun crc8Atm(bytes: ByteArray, length: Int = bytes.size): Int {
        var crc = 0
        for (index in 0 until length) {
            crc = crc xor (bytes[index].toInt() and 0xFF)
            repeat(8) {
                crc = if (crc and 0x80 != 0) ((crc shl 1) xor 0x07) and 0xFF else (crc shl 1) and 0xFF
            }
        }
        return crc
    }
}
