package com.fyx.beebot.speech

/** 第一版离线助手支持的固定业务意图。 */
enum class VoiceIntent {
    DEVICE_BATTERY,
    CHARGING_STATUS,
    BATTERY_TEMPERATURE,
    DATE_TIME,
    NETWORK_STATUS,
    VEHICLE_ONLINE,
    VEHICLE_BATTERY,
    COMMUNICATION_LATENCY,
    RADAR_DISTANCE,
    VEHICLE_MODE,
    APP_VERSION,
    HELP,
    UNKNOWN
}

enum class RadarDirection(val label: String) {
    FRONT("前方"), REAR("后方"), LEFT("左侧"), RIGHT("右侧"), ALL("四向")
}

data class DeviceStatusSnapshot(
    val deviceBatteryPercent: Int? = null,
    val charging: Boolean? = null,
    val batteryTemperatureC: Float? = null,
    val dateTimeText: String,
    val networkDescription: String,
    val appVersion: String,
    val vehicleOnline: Boolean,
    val vehicleBatteryPercent: Int? = null,
    val vehicleVoltageV: Float? = null,
    val communicationLatencyMs: Int? = null,
    val frontDistanceCm: Int? = null,
    val rearDistanceCm: Int? = null,
    val leftDistanceCm: Int? = null,
    val rightDistanceCm: Int? = null,
    val vehicleMode: String? = null
)

data class ParsedVoiceIntent(
    val intent: VoiceIntent,
    val radarDirection: RadarDirection = RadarDirection.ALL
)

data class VoiceAssistantReply(val intent: VoiceIntent, val text: String)

/**
 * 只做确定性关键词匹配，不调用大模型、不访问网络。
 * 顺序就是消歧优先级：车辆专属词必须先于默认的掌机电量。
 */
object VoiceIntentParser {
    private val vehicleWords = listOf("车辆", "小车", "车子", "车端", "机器人")
    private val deviceWords = listOf("掌机", "手柄", "遥控器", "本机", "设备")
    private val batteryWords = listOf("电量", "电池", "多少电", "剩余电", "还有电")

    fun parse(rawText: String): ParsedVoiceIntent {
        val text = normalize(rawText)
        if (text.isBlank()) return ParsedVoiceIntent(VoiceIntent.UNKNOWN)
        val vehicle = text.containsAny(vehicleWords)
        val device = text.containsAny(deviceWords)

        return when {
            text.containsAny("帮助", "能问什么", "可以问什么", "支持什么", "你会什么") ->
                ParsedVoiceIntent(VoiceIntent.HELP)
            text.containsAny("版本", "版本号") -> ParsedVoiceIntent(VoiceIntent.APP_VERSION)
            text.containsAny("网络", "wifi", "无线网", "联网", "网线", "以太网") ->
                ParsedVoiceIntent(VoiceIntent.NETWORK_STATUS)
            text.containsAny("延迟", "时延", "通信速度", "通讯速度") ->
                ParsedVoiceIntent(VoiceIntent.COMMUNICATION_LATENCY)
            text.containsAny("雷达", "距离", "障碍物") -> ParsedVoiceIntent(
                VoiceIntent.RADAR_DISTANCE,
                when {
                    text.containsAny("前面", "前方", "前边") -> RadarDirection.FRONT
                    text.containsAny("后面", "后方", "后边") -> RadarDirection.REAR
                    text.containsAny("左边", "左侧", "左方") -> RadarDirection.LEFT
                    text.containsAny("右边", "右侧", "右方") -> RadarDirection.RIGHT
                    else -> RadarDirection.ALL
                }
            )
            vehicle && text.containsAny("模式", "状态模式") -> ParsedVoiceIntent(VoiceIntent.VEHICLE_MODE)
            vehicle && text.containsAny(batteryWords) -> ParsedVoiceIntent(VoiceIntent.VEHICLE_BATTERY)
            vehicle && text.containsAny("在线", "离线", "连接", "连上", "掉线", "在不在") ->
                ParsedVoiceIntent(VoiceIntent.VEHICLE_ONLINE)
            text.containsAny("充电", "充上电", "插着电") -> ParsedVoiceIntent(VoiceIntent.CHARGING_STATUS)
            text.containsAny("温度", "发热", "多热") && (device || text.contains("电池")) ->
                ParsedVoiceIntent(VoiceIntent.BATTERY_TEMPERATURE)
            (device || !vehicle) && text.containsAny(batteryWords) -> ParsedVoiceIntent(VoiceIntent.DEVICE_BATTERY)
            text.containsAny("几点", "时间", "日期", "几号", "星期") -> ParsedVoiceIntent(VoiceIntent.DATE_TIME)
            else -> ParsedVoiceIntent(VoiceIntent.UNKNOWN)
        }
    }

    private fun normalize(text: String): String = text.lowercase()
        .replace(Regex("[\\s，。！？、,.!?：:；;\"'（）()]+"), "")

    private fun String.containsAny(words: Iterable<String>): Boolean = words.any(::contains)
    private fun String.containsAny(vararg words: String): Boolean = words.any(::contains)
}

/** 把已解析意图与某一时刻的真实状态快照组合成固定回答。 */
object VoiceAssistantEngine {
    fun reply(question: String, snapshot: DeviceStatusSnapshot): VoiceAssistantReply {
        val parsed = VoiceIntentParser.parse(question)
        val text = when (parsed.intent) {
            VoiceIntent.DEVICE_BATTERY -> snapshot.deviceBatteryPercent?.let { percent ->
                val chargingText = when (snapshot.charging) {
                    true -> "，正在充电"
                    false -> "，当前未充电"
                    null -> ""
                }
                "掌机当前电量为百分之${percent}$chargingText。"
            } ?: "暂时无法读取掌机电量。"

            VoiceIntent.CHARGING_STATUS -> when (snapshot.charging) {
                true -> "掌机当前正在充电。"
                false -> "掌机当前没有充电。"
                null -> "暂时无法读取掌机充电状态。"
            }

            VoiceIntent.BATTERY_TEMPERATURE -> snapshot.batteryTemperatureC?.let {
                "掌机电池当前温度为${formatOneDecimal(it)}摄氏度。"
            } ?: "暂时无法读取掌机电池温度。"

            VoiceIntent.DATE_TIME -> "现在是${snapshot.dateTimeText}。"
            VoiceIntent.NETWORK_STATUS -> snapshot.networkDescription
            VoiceIntent.VEHICLE_ONLINE -> if (snapshot.vehicleOnline) {
                "车辆当前在线。"
            } else {
                "车辆当前离线，或者最近五秒没有收到车辆消息。"
            }

            VoiceIntent.VEHICLE_BATTERY -> when {
                !snapshot.vehicleOnline -> "车辆当前离线，无法获取最新电量。"
                snapshot.vehicleBatteryPercent == null -> "暂时没有收到车辆最新电量数据。"
                else -> buildString {
                    append("车辆当前电量为百分之${snapshot.vehicleBatteryPercent}")
                    snapshot.vehicleVoltageV?.let { append("，电压${formatOneDecimal(it)}伏") }
                    append("。")
                }
            }

            VoiceIntent.COMMUNICATION_LATENCY -> when {
                !snapshot.vehicleOnline -> "车辆当前离线，无法测量通信延迟。"
                snapshot.communicationLatencyMs == null -> "车辆在线，通信延迟正在刷新。"
                else -> "车辆当前通信延迟为${snapshot.communicationLatencyMs}毫秒。"
            }

            VoiceIntent.RADAR_DISTANCE -> radarReply(parsed.radarDirection, snapshot)
            VoiceIntent.VEHICLE_MODE -> when {
                !snapshot.vehicleOnline -> "车辆当前离线，无法确认最新车辆模式。"
                snapshot.vehicleMode == null -> "暂时没有收到车辆最新模式数据。"
                else -> "车辆当前处于${snapshot.vehicleMode}模式。"
            }

            VoiceIntent.APP_VERSION -> "当前小蜜蜂智控系统版本是${snapshot.appVersion}。"
            VoiceIntent.HELP, VoiceIntent.UNKNOWN ->
                "我可以离线查询掌机电量、充电状态、电池温度、时间、网络、车辆在线状态、车辆电量、通信延迟、雷达距离、车辆模式和应用版本。"
        }
        return VoiceAssistantReply(parsed.intent, text)
    }

    private fun radarReply(direction: RadarDirection, snapshot: DeviceStatusSnapshot): String {
        if (!snapshot.vehicleOnline) return "车辆当前离线，无法获取最新雷达距离。"
        val value = when (direction) {
            RadarDirection.FRONT -> snapshot.frontDistanceCm
            RadarDirection.REAR -> snapshot.rearDistanceCm
            RadarDirection.LEFT -> snapshot.leftDistanceCm
            RadarDirection.RIGHT -> snapshot.rightDistanceCm
            RadarDirection.ALL -> null
        }
        if (direction != RadarDirection.ALL) {
            return value?.let { "${direction.label}障碍物距离${it}厘米。" }
                ?: "暂时没有收到${direction.label}最新雷达数据。"
        }
        val values = listOf(
            "前方" to snapshot.frontDistanceCm,
            "后方" to snapshot.rearDistanceCm,
            "左侧" to snapshot.leftDistanceCm,
            "右侧" to snapshot.rightDistanceCm
        )
        if (values.all { it.second == null }) return "暂时没有收到车辆最新雷达数据。"
        return values.joinToString("，", postfix = "。") { (label, distance) ->
            distance?.let { "${label}${it}厘米" } ?: "${label}暂无数据"
        }
    }

    private fun formatOneDecimal(value: Float): String = String.format(java.util.Locale.CHINA, "%.1f", value)
}
