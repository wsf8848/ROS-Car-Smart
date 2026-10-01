package com.fyx.beebot.speech

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test

class VoiceAssistantEngineTest {
    private val snapshot = DeviceStatusSnapshot(
        deviceBatteryPercent = 76,
        charging = true,
        batteryTemperatureC = 31.5f,
        dateTimeText = "2026年9月17日星期四17点20分",
        networkDescription = "掌机当前通过无线网络连接。",
        appVersion = "2.6.0",
        vehicleOnline = true,
        vehicleBatteryPercent = 63,
        vehicleVoltageV = 12.4f,
        communicationLatencyMs = 42,
        frontDistanceCm = 85,
        rearDistanceCm = 120,
        leftDistanceCm = 64,
        rightDistanceCm = 71,
        vehicleMode = "智能避障"
    )

    @Test fun unspecifiedBatteryDefaultsToHandheld() {
        val reply = VoiceAssistantEngine.reply("现在电量多少", snapshot)
        assertEquals(VoiceIntent.DEVICE_BATTERY, reply.intent)
        assertTrue(reply.text.contains("76"))
        assertTrue(reply.text.contains("正在充电"))
    }

    @Test fun vehicleBatteryHasPriorityOverDeviceBattery() {
        val reply = VoiceAssistantEngine.reply("车辆还剩多少电？", snapshot)
        assertEquals(VoiceIntent.VEHICLE_BATTERY, reply.intent)
        assertTrue(reply.text.contains("63"))
        assertTrue(reply.text.contains("12.4"))
    }

    @Test fun recognizesCommonDeviceAliases() {
        assertEquals(VoiceIntent.DEVICE_BATTERY, VoiceIntentParser.parse("遥控器还有多少电").intent)
        assertEquals(VoiceIntent.DEVICE_BATTERY, VoiceIntentParser.parse("手柄电池怎么样").intent)
        assertEquals(VoiceIntent.CHARGING_STATUS, VoiceIntentParser.parse("掌机在充电吗").intent)
    }

    @Test fun directionRadarReturnsOnlyRequestedDirection() {
        val reply = VoiceAssistantEngine.reply("前方障碍物距离多少", snapshot)
        assertEquals(VoiceIntent.RADAR_DISTANCE, reply.intent)
        assertEquals("前方障碍物距离85厘米。", reply.text)
    }

    @Test fun staleVehicleDataNeverFallsBackToOldValue() {
        val stale = snapshot.copy(vehicleBatteryPercent = null, vehicleVoltageV = null)
        val reply = VoiceAssistantEngine.reply("小车电量多少", stale)
        assertEquals("暂时没有收到车辆最新电量数据。", reply.text)
    }

    @Test fun offlineVehicleExplainsUnavailableState() {
        val reply = VoiceAssistantEngine.reply("车辆通信延迟多少", snapshot.copy(vehicleOnline = false))
        assertEquals(VoiceIntent.COMMUNICATION_LATENCY, reply.intent)
        assertTrue(reply.text.contains("离线"))
    }

    @Test fun unknownQuestionReturnsLocalHelp() {
        val reply = VoiceAssistantEngine.reply("给我讲个故事", snapshot)
        assertEquals(VoiceIntent.UNKNOWN, reply.intent)
        assertTrue(reply.text.contains("离线查询"))
    }
}
