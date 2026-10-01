package com.fyx.beebot.util

import android.content.Context
import android.net.ConnectivityManager
import android.net.NetworkCapabilities
import android.net.wifi.WifiManager
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.ui.platform.LocalContext
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.net.InetAddress

/** WiFi 连接状态：名称、信号百分比、延迟(ms)。 */
data class WifiInfo(
    val connected: Boolean = false,
    val percent: Int = 0,
    val ssid: String = "",
    val latencyMs: Int = -1   // -1 = 未知/超时
)

/**
 * 每 2s 刷新的 WiFi 状态（名称 / 信号百分比 / 网关延迟）。
 *
 * ⚠️ SSID 名称受 Android 定位隐私保护；本掌机（Retroid 定制 ROM）更进一步在系统层
 * 屏蔽第三方 App 读取，即便授予定位权限仍返回 <unknown ssid>，此时回退显示“WiFi已连接”。
 * 在普通手机上可正常显示真实名称。信号强度与延迟不受限制。
 */
@Composable
fun rememberWifiStatus(): State<WifiInfo> {
    val context = LocalContext.current
    val state = remember { mutableStateOf(WifiInfo()) }
    LaunchedEffect(Unit) {
        while (true) {
            state.value = withContext(Dispatchers.IO) { readWifiStatus(context) }
            delay(2000)
        }
    }
    return state
}

private fun readWifiStatus(context: Context): WifiInfo {
    return try {
        val cm = context.getSystemService(Context.CONNECTIVITY_SERVICE) as ConnectivityManager
        val caps = cm.getNetworkCapabilities(cm.activeNetwork)
        val onWifi = caps?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        if (!onWifi) return WifiInfo(false, 0, "", -1)

        val wm = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as WifiManager
        @Suppress("DEPRECATION")
        val info = wm.connectionInfo
        val rssi = info?.rssi ?: -60
        val percent = ((rssi + 100) * 2).coerceIn(0, 100)

        @Suppress("DEPRECATION")
        val raw = info?.ssid?.trim('"') ?: ""
        val ssid = if (raw.isEmpty() || raw == "<unknown ssid>") "WiFi已连接" else raw

        // 延迟：ping 网关（无网关则 ping 阿里 DNS）
        @Suppress("DEPRECATION")
        val gw = wm.dhcpInfo?.gateway ?: 0
        val target = if (gw != 0) intToIp(gw) else "223.5.5.5"
        val latency = pingMs(target)

        WifiInfo(true, percent, ssid, latency)
    } catch (e: Exception) {
        WifiInfo(false, 0, "", -1)
    }
}

private fun pingMs(host: String): Int = try {
    val addr = InetAddress.getByName(host)
    val t0 = System.nanoTime()
    if (addr.isReachable(800)) ((System.nanoTime() - t0) / 1_000_000L).toInt().coerceAtLeast(0) else -1
} catch (e: Exception) {
    -1
}

private fun intToIp(i: Int): String =
    "${i and 0xff}.${i shr 8 and 0xff}.${i shr 16 and 0xff}.${i shr 24 and 0xff}"
