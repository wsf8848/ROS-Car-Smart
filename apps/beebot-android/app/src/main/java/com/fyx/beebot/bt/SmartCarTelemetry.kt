package com.fyx.beebot.bt

import android.os.SystemClock
import android.os.Handler
import android.os.Looper
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue

/** 解析智能小车通过蓝牙 SPP 连续发送的雷达、底盘和电源二进制遥测包。 */
object SmartCarTelemetry {

    const val TELEMETRY_TIMEOUT_MS = 1500L

    private const val HEADER = 0x55
    private const val RADAR_TYPE = 0x51
    private const val CHASSIS_TYPE = 0x52
    private const val POWER_TYPE = 0x53
    private const val TAIL = 0x5B

    var frontDistanceCm by mutableStateOf<Int?>(null); private set
    var rearDistanceCm by mutableStateOf<Int?>(null); private set
    var leftDistanceCm by mutableStateOf<Int?>(null); private set
    var rightDistanceCm by mutableStateOf<Int?>(null); private set
    var lidarStatus by mutableStateOf(0); private set
    var carStatus by mutableStateOf(0); private set

    var leftRearSpeed by mutableStateOf(0); private set
    var leftFrontSpeed by mutableStateOf(0); private set
    var rightFrontSpeed by mutableStateOf(0); private set
    var rightRearSpeed by mutableStateOf(0); private set
    var gyroscopeAngle by mutableStateOf(0); private set
    var powerVoltageDeciVolt by mutableStateOf<Int?>(null); private set
    var powerPercent by mutableStateOf<Int?>(null); private set

    var radarIntervalMs by mutableStateOf(-1); private set
    var chassisIntervalMs by mutableStateOf(-1); private set
    var parsedPacketCount by mutableStateOf(0L); private set

    private val main = Handler(Looper.getMainLooper())
    @Volatile private var lastRadarTime = 0L
    @Volatile private var lastChassisTime = 0L
    @Volatile private var lastPowerTime = 0L
    private val buffer = mutableListOf<Int>()

    fun feedRx(bytes: ByteArray, receivedAtMs: Long = SystemClock.elapsedRealtime()) {
        val updates = synchronized(buffer) {
            bytes.forEach { buffer.add(it.toInt() and 0xFF) }
            val parsed = parsePackets(receivedAtMs)
            if (buffer.size > 256) buffer.subList(0, buffer.size - 256).clear()
            parsed
        }
        if (updates.isEmpty()) return
        main.post {
            parsedPacketCount += updates.size
            updates.forEach { update ->
                when (update) {
                    is RadarUpdate -> applyRadarUpdate(update)
                    is ChassisUpdate -> applyChassisUpdate(update)
                    is PowerUpdate -> applyPowerUpdate(update)
                }
            }
        }
    }

    fun resetSession() {
        synchronized(buffer) { buffer.clear() }
        lastRadarTime = 0L
        lastChassisTime = 0L
        lastPowerTime = 0L
        radarIntervalMs = -1
        chassisIntervalMs = -1
        powerVoltageDeciVolt = null
        powerPercent = null
        parsedPacketCount = 0L
    }

    fun radarAgeMs(now: Long = SystemClock.elapsedRealtime()): Long =
        if (lastRadarTime == 0L) -1L else (now - lastRadarTime).coerceAtLeast(0L)

    fun chassisAgeMs(now: Long = SystemClock.elapsedRealtime()): Long =
        if (lastChassisTime == 0L) -1L else (now - lastChassisTime).coerceAtLeast(0L)

    fun powerAgeMs(now: Long = SystemClock.elapsedRealtime()): Long =
        if (lastPowerTime == 0L) -1L else (now - lastPowerTime).coerceAtLeast(0L)

    fun lidarStatusText(): String = when (lidarStatus) {
        1 -> "工作中"
        else -> "异常/等待"
    }

    fun carStatusText(): String = when (carStatus) {
        1 -> "智能避障"
        2 -> "自主导航"
        3 -> "蓝牙手动"
        else -> "待机"
    }

    private fun parsePackets(receivedAtMs: Long): List<TelemetryUpdate> {
        val updates = mutableListOf<TelemetryUpdate>()
        while (buffer.size >= 2) {
            val headerIndex = buffer.indexOf(HEADER)
            if (headerIndex < 0) {
                buffer.clear()
                break
            }
            if (headerIndex > 0) buffer.subList(0, headerIndex).clear()
            if (buffer.size < 2) break

            val packetLength = when (buffer[1]) {
                RADAR_TYPE -> 9
                CHASSIS_TYPE -> 8
                POWER_TYPE -> 5
                else -> {
                    buffer.removeAt(0)
                    continue
                }
            }
            if (buffer.size < packetLength) break
            if (buffer[packetLength - 1] != TAIL) {
                buffer.removeAt(0)
                continue
            }

            val packet = buffer.take(packetLength)
            buffer.subList(0, packetLength).clear()
            if (packet[1] == RADAR_TYPE) {
                val interval = if (lastRadarTime > 0L) (receivedAtMs - lastRadarTime).toInt() else -1
                lastRadarTime = receivedAtMs
                updates.add(
                    RadarUpdate(
                        decodeDistance(packet[2]), decodeDistance(packet[3]),
                        decodeDistance(packet[4]), decodeDistance(packet[5]),
                        packet[6], packet[7], interval
                    )
                )
            } else if (packet[1] == CHASSIS_TYPE) {
                val interval = if (lastChassisTime > 0L) (receivedAtMs - lastChassisTime).toInt() else -1
                lastChassisTime = receivedAtMs
                updates.add(ChassisUpdate(decodeSignedByte(packet[2]), decodeSignedByte(packet[3]), decodeSignedByte(packet[4]), decodeSignedByte(packet[5]), decodeSignedByte(packet[6]), interval))
            } else {
                val interval = if (lastPowerTime > 0L) (receivedAtMs - lastPowerTime).toInt() else -1
                lastPowerTime = receivedAtMs
                updates.add(
                    PowerUpdate(
                        decodePowerValue(packet[2]),
                        decodePowerValue(packet[3]),
                        interval
                    )
                )
            }
        }
        return updates
    }

    private fun applyRadarUpdate(update: RadarUpdate) {
        frontDistanceCm = update.frontDistanceCm
        rearDistanceCm = update.rearDistanceCm
        leftDistanceCm = update.leftDistanceCm
        rightDistanceCm = update.rightDistanceCm
        lidarStatus = update.lidarStatus
        carStatus = update.carStatus
        if (update.intervalMs >= 0) radarIntervalMs = update.intervalMs
    }

    private fun applyChassisUpdate(update: ChassisUpdate) {
        leftRearSpeed = update.leftRearSpeed
        leftFrontSpeed = update.leftFrontSpeed
        rightFrontSpeed = update.rightFrontSpeed
        rightRearSpeed = update.rightRearSpeed
        gyroscopeAngle = update.gyroscopeAngle
        if (update.intervalMs >= 0) chassisIntervalMs = update.intervalMs
    }

    private fun applyPowerUpdate(update: PowerUpdate) {
        powerVoltageDeciVolt = update.voltageDeciVolt
        powerPercent = update.percent
    }

    private fun decodeDistance(value: Int): Int? = if (value == 0xFF) null else value

    private fun decodePowerValue(value: Int): Int? = if (value == 0xFF) null else value

    private fun decodeSignedByte(value: Int): Int = value.toByte().toInt()

    private sealed interface TelemetryUpdate

    private data class RadarUpdate(
        val frontDistanceCm: Int?,
        val rearDistanceCm: Int?,
        val leftDistanceCm: Int?,
        val rightDistanceCm: Int?,
        val lidarStatus: Int,
        val carStatus: Int,
        val intervalMs: Int
    ) : TelemetryUpdate

    private data class ChassisUpdate(
        val leftRearSpeed: Int,
        val leftFrontSpeed: Int,
        val rightFrontSpeed: Int,
        val rightRearSpeed: Int,
        val gyroscopeAngle: Int,
        val intervalMs: Int
    ) : TelemetryUpdate

    private data class PowerUpdate(
        val voltageDeciVolt: Int?,
        val percent: Int?,
        val intervalMs: Int
    ) : TelemetryUpdate
}
