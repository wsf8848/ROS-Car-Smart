package com.fyx.beebot.bt

/** 已配对的蓝牙设备。 */
data class BtDevice(val name: String, val address: String)

/** 一帧数据的方向。 */
enum class BtDir { RX, TX, SYS }

/** 收发日志中的一帧（原始字节按当前编码在界面上格式化显示）。 */
data class BtFrame(
    val timestamp: Long,
    val dir: BtDir,
    val data: ByteArray = ByteArray(0),
    val sysMsg: String? = null
)

/** 收发编码模式。 */
enum class BtEncoding { HEX, UTF8 }

/** 波特率候选（SPP 下仅 App 内保存/显示，不改写模块）。 */
val BAUD_RATES = listOf(1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200)

/** HEX / UTF-8 编解码。 */
object BtCodec {
    fun format(bytes: ByteArray, enc: BtEncoding): String = when (enc) {
        BtEncoding.HEX -> bytes.joinToString(" ") { "%02X".format(it) }
        BtEncoding.UTF8 -> String(bytes, Charsets.UTF_8)
    }

    /** 解析发送框文本为字节；HEX 非法时返回 null。 */
    fun parse(text: String, enc: BtEncoding): ByteArray? = when (enc) {
        BtEncoding.UTF8 -> text.toByteArray(Charsets.UTF_8)
        BtEncoding.HEX -> {
            val clean = text.replace(Regex("[^0-9a-fA-F]"), "")
            if (clean.isEmpty() || clean.length % 2 != 0) null
            else ByteArray(clean.length / 2) { clean.substring(it * 2, it * 2 + 2).toInt(16).toByte() }
        }
    }
}
