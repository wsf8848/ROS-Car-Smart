package com.fyx.beebot.ui

import android.content.Context
import android.webkit.JavascriptInterface
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import com.fyx.beebot.bt.AckermannController
import com.fyx.beebot.bt.BluetoothRepository
import com.fyx.beebot.input.GamepadRepository
import kotlinx.coroutines.delay
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Fullscreen
import androidx.compose.material.icons.filled.FullscreenExit
import androidx.compose.material.icons.filled.Refresh
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.zIndex
import androidx.compose.ui.window.Popup
import com.fyx.beebot.ui.theme.Hud

private const val PAGE = "file:///android_asset/webconsole/index.html"
private const val PREF = "beebot_prefs"
private const val KEY_IP = "ros_pi_ip"
private const val DEFAULT_IP = "192.168.0.100"

private fun urlFor(ip: String) = "$PAGE?host=${ip.ifBlank { DEFAULT_IP }}&embed=1"

// 全屏状态：网页轮询 BtBridge.fullscreen() 来决定摄像头是否铺满整屏（比 evaluateJavascript 可靠）
@Volatile private var rosWebFullscreen = false

/**
 * ROS 网页页：内嵌 WebView 加载打包在 assets 里的 ROS 控制台网页，
 * 网页经 WiFi 连树莓派的 rosbridge/摄像头流。顶部可改树莓派 IP、刷新、全屏。
 */
@Composable
fun RosWebScreen(fullscreen: Boolean, onToggleFullscreen: () -> Unit) {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences(PREF, Context.MODE_PRIVATE) }
    var ip by rememberSaveable { mutableStateOf(prefs.getString(KEY_IP, DEFAULT_IP) ?: DEFAULT_IP) }
    // 0=加载中 1=已加载 2=加载失败
    val status = remember { mutableStateOf(0) }
    val webViewRef = remember { mutableStateOf<WebView?>(null) }

    fun reload() {
        prefs.edit().putString(KEY_IP, ip).apply()
        status.value = 0
        webViewRef.value?.loadUrl(urlFor(ip))
    }

    // 网页/实体摇杆 → 蓝牙直连驱动：每 100ms 发一次（实体优先），维持底盘失控保护；离开页面自动停车
    LaunchedEffect(Unit) {
        while (true) { AckermannController.rosDriveTick(GamepadRepository.state); delay(100) }
    }
    DisposableEffect(Unit) { onDispose { AckermannController.webStop() } }

    // 全屏状态给网页（网页每 ~300ms 轮询 BtBridge.fullscreen() 决定摄像头是否铺满）
    LaunchedEffect(fullscreen, webViewRef.value) {
        rosWebFullscreen = fullscreen
        // 网页轮询仍是主机制；这里补一次即时同步，避免部分 WebView 首次进入全屏时要等到下一轮轮询。
        delay(120)
        webViewRef.value?.evaluateJavascript(
            "document.body.classList.toggle('camfull', ${if (fullscreen) "true" else "false"});window.scrollTo(0,0);",
            null
        )
    }

    Column(Modifier.fillMaxSize()) {
      if (!fullscreen) {
        // 顶部控制栏
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .background(Hud.Panel, RoundedCornerShape(10.dp))
                .border(1.dp, Hud.Border, RoundedCornerShape(10.dp))
                .padding(horizontal = 10.dp, vertical = 7.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("树莓派", color = Hud.TextSecondary, fontSize = 11.sp)
            Spacer(Modifier.width(8.dp))
            // 可编辑 IP
            Box(
                modifier = Modifier
                    .width(140.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(Hud.PanelDeep)
                    .border(1.dp, Hud.Border, RoundedCornerShape(6.dp))
                    .padding(horizontal = 10.dp, vertical = 6.dp),
                contentAlignment = Alignment.CenterStart
            ) {
                BasicTextField(
                    value = ip,
                    onValueChange = { ip = it.trim() },
                    singleLine = true,
                    textStyle = TextStyle(color = Hud.TextPrimary, fontSize = 13.sp),
                    cursorBrush = SolidColor(Hud.Accent),
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                    modifier = Modifier.fillMaxWidth()
                )
            }
            Spacer(Modifier.width(10.dp))
            // 网页加载状态
            val (dot, label) = when (status.value) {
                1 -> Hud.Success to "已加载"
                2 -> Hud.Danger to "打不开"
                else -> Hud.Warning to "加载中"
            }
            Box(Modifier.size(7.dp).clip(CircleShape).background(dot))
            Spacer(Modifier.width(4.dp))
            Text(label, color = dot, fontSize = 11.sp)

            Spacer(Modifier.weight(1f))
            IconChip(Icons.Filled.Refresh, "刷新") { com.fyx.beebot.util.Haptics.tick(); reload() }
            Spacer(Modifier.width(6.dp))
            IconChip(
                if (fullscreen) Icons.Filled.FullscreenExit else Icons.Filled.Fullscreen,
                if (fullscreen) "退出全屏" else "全屏"
            ) { com.fyx.beebot.util.Haptics.tick(); onToggleFullscreen() }
        }

        Spacer(Modifier.height(8.dp))
      }

        // WebView
        Box(
            modifier = Modifier
                .fillMaxSize()
                .clip(RoundedCornerShape(if (fullscreen) 0.dp else 10.dp))
                .background(if (fullscreen) androidx.compose.ui.graphics.Color.Black else Hud.PanelDeep)
                .then(if (fullscreen) Modifier else Modifier.border(1.dp, Hud.Border, RoundedCornerShape(10.dp)))
        ) {
            AndroidView(
                modifier = Modifier.fillMaxSize().zIndex(0f),
                factory = { ctx ->
                    WebView(ctx).apply {
                        setBackgroundColor(android.graphics.Color.BLACK)
                        settings.apply {
                            javaScriptEnabled = true
                            domStorageEnabled = true
                            mediaPlaybackRequiresUserGesture = false
                            mixedContentMode = WebSettings.MIXED_CONTENT_ALWAYS_ALLOW
                            allowFileAccess = true
                            allowContentAccess = true
                            cacheMode = WebSettings.LOAD_NO_CACHE
                        }
                        // 暴露给网页 JS：BtBridge，让网页摇杆经蓝牙直连驱动小车
                        addJavascriptInterface(BtWebBridge(), "BtBridge")
                        webViewClient = object : WebViewClient() {
                            override fun onPageFinished(view: WebView?, url: String?) {
                                if (status.value != 2) status.value = 1
                            }
                            override fun onReceivedError(
                                view: WebView?, request: WebResourceRequest?, error: WebResourceError?
                            ) {
                                if (request?.isForMainFrame == true) status.value = 2
                            }
                        }
                        loadUrl(urlFor(ip))
                        webViewRef.value = this
                    }
                }
            )
            if (fullscreen) {
                Popup(alignment = Alignment.TopEnd) {
                    Box(Modifier.padding(10.dp).clip(RoundedCornerShape(8.dp)).background(Hud.Panel)) {
                        IconChip(Icons.Filled.FullscreenExit, "退出全屏") {
                            com.fyx.beebot.util.Haptics.tick(); onToggleFullscreen()
                        }
                    }
                }
            }
        }
    }
}

/** 网页 ↔ 原生 蓝牙驱动桥。网页 JS 调用 window.BtBridge.*。 */
private class BtWebBridge {
    @JavascriptInterface fun connected(): Boolean = BluetoothRepository.connected
    /** 油门(前+/后-)、转向(右+/左-)，范围约 -1..1。 */
    @JavascriptInterface fun drive(throttle: Double, steer: Double) =
        AckermannController.webDrive(throttle.toFloat(), steer.toFloat())
    @JavascriptInterface fun stop() = AckermannController.webStop()
    /** 原生全屏状态：网页轮询它决定摄像头是否铺满整屏。 */
    @JavascriptInterface fun fullscreen(): Boolean = rosWebFullscreen
}

@Composable
private fun IconChip(icon: androidx.compose.ui.graphics.vector.ImageVector, label: String, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .clip(RoundedCornerShape(8.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(8.dp))
            .clickable { onClick() }
            .padding(horizontal = 10.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(icon, null, tint = Hud.Accent, modifier = Modifier.size(15.dp))
        Spacer(Modifier.width(4.dp))
        Text(label, color = Hud.Accent, fontSize = 11.sp)
    }
}
