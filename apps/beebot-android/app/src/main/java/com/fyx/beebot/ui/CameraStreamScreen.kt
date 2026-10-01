package com.fyx.beebot.ui

import android.content.Context
import android.net.Uri
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.VideoView
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
import androidx.compose.material.icons.filled.Link
import androidx.compose.material.icons.filled.LinkOff
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.fyx.beebot.ui.theme.Hud
import com.fyx.beebot.util.Haptics
import kotlinx.coroutines.delay

private const val CAMERA_PREFS = "beebot_prefs"
private const val CAMERA_URL_KEY = "camera_stream_url"
private const val CAMERA_TYPE_KEY = "camera_stream_type"
private const val DEFAULT_CAMERA_URL = "http://192.168.4.1/stream"

private enum class CameraStreamType(val label: String) {
    AUTO("自动"),
    MJPEG("MJPEG"),
    RTSP("RTSP"),
    JPEG("JPEG")
}

@Composable
fun CameraStreamScreen(fullscreen: Boolean, onToggleFullscreen: () -> Unit) {
    val context = LocalContext.current
    val preferences = remember { context.getSharedPreferences(CAMERA_PREFS, Context.MODE_PRIVATE) }
    var streamUrl by rememberSaveable {
        mutableStateOf(preferences.getString(CAMERA_URL_KEY, DEFAULT_CAMERA_URL) ?: DEFAULT_CAMERA_URL)
    }
    var streamTypeName by rememberSaveable {
        mutableStateOf(preferences.getString(CAMERA_TYPE_KEY, CameraStreamType.AUTO.name) ?: CameraStreamType.AUTO.name)
    }
    val streamType = CameraStreamType.values().firstOrNull { it.name == streamTypeName } ?: CameraStreamType.AUTO
    var activeUrl by rememberSaveable { mutableStateOf("") }
    var activeTypeName by rememberSaveable { mutableStateOf(CameraStreamType.AUTO.name) }
    val activeType = CameraStreamType.values().firstOrNull { it.name == activeTypeName } ?: CameraStreamType.AUTO
    var status by rememberSaveable { mutableStateOf(0) }
    var connected by rememberSaveable { mutableStateOf(false) }
    val webViewRef = remember { mutableStateOf<WebView?>(null) }
    val videoViewRef = remember { mutableStateOf<VideoView?>(null) }

    fun connect() {
        val url = streamUrl.trim()
        if (url.isEmpty()) {
            status = 2
            return
        }
        preferences.edit()
            .putString(CAMERA_URL_KEY, url)
            .putString(CAMERA_TYPE_KEY, streamType.name)
            .apply()
        videoViewRef.value?.stopPlayback()
        webViewRef.value?.stopLoading()
        activeUrl = url
        activeTypeName = streamType.name
        status = 0
        connected = true
    }

    fun disconnect() {
        webViewRef.value?.stopLoading()
        webViewRef.value?.loadUrl("about:blank")
        videoViewRef.value?.stopPlayback()
        connected = false
        activeUrl = ""
        status = 0
    }

    DisposableEffect(Unit) {
        onDispose {
            webViewRef.value?.stopLoading()
            webViewRef.value?.destroy()
            videoViewRef.value?.stopPlayback()
            webViewRef.value = null
            videoViewRef.value = null
        }
    }

    LaunchedEffect(connected, activeUrl) {
        if (connected) {
            delay(8_000)
            if (status == 0) status = 2
        }
    }

    Column(Modifier.fillMaxSize()) {
        if (!fullscreen) {
            CameraControlBar(
                streamUrl = streamUrl,
                onUrlChange = { streamUrl = it },
                streamType = streamType,
                onTypeChange = { streamTypeName = it.name },
                status = status,
                connected = connected,
                onConnect = { Haptics.click(); connect() },
                onDisconnect = { Haptics.click(); disconnect() },
                onToggleFullscreen = { Haptics.tick(); onToggleFullscreen() }
            )
            Spacer(Modifier.height(8.dp))
        }

        Box(
            Modifier.fillMaxSize()
                .clip(RoundedCornerShape(if (fullscreen) 0.dp else 10.dp))
                .background(Color(0xFF05080D))
                .then(if (fullscreen) Modifier else Modifier.border(1.dp, Hud.Border, RoundedCornerShape(10.dp))),
            contentAlignment = Alignment.Center
        ) {
            if (!connected) {
                CameraPlaceholder("等待连接摄像头", "输入局域网推流地址后点击连接预览")
            } else if (isRtsp(activeType, activeUrl)) {
                AndroidView(
                    modifier = Modifier.fillMaxSize(),
                    factory = { ctx ->
                        VideoView(ctx).apply {
                            tag = activeUrl
                            setOnPreparedListener { player ->
                                player.isLooping = true
                                status = 1
                                start()
                            }
                            setOnErrorListener { _, _, _ ->
                                status = 2
                                true
                            }
                            videoViewRef.value = this
                            setVideoURI(Uri.parse(activeUrl))
                            requestFocus()
                            start()
                        }
                    },
                    update = { view ->
                        if (view.tag != activeUrl) {
                            view.tag = activeUrl
                            view.setVideoURI(Uri.parse(activeUrl))
                            view.start()
                        }
                    }
                )
            } else {
                AndroidView(
                    modifier = Modifier.fillMaxSize(),
                    factory = { ctx ->
                        WebView(ctx).apply {
                            setBackgroundColor(android.graphics.Color.BLACK)
                            settings.apply {
                                javaScriptEnabled = true
                                domStorageEnabled = true
                                mediaPlaybackRequiresUserGesture = false
                                mixedContentMode = WebSettings.MIXED_CONTENT_ALWAYS_ALLOW
                                cacheMode = WebSettings.LOAD_NO_CACHE
                                loadWithOverviewMode = true
                                useWideViewPort = true
                                builtInZoomControls = true
                                displayZoomControls = false
                            }
                            webViewClient = object : WebViewClient() {
                                override fun onPageFinished(view: WebView?, url: String?) {
                                    if (status != 2 && url != "about:blank") status = 1
                                }

                                override fun onReceivedError(
                                    view: WebView?, request: WebResourceRequest?, error: WebResourceError?
                                ) {
                                    if (request?.isForMainFrame == true) status = 2
                                }
                            }
                            webViewRef.value = this
                            loadUrl(activeUrl)
                        }
                    },
                    update = { view ->
                        if (view.url != activeUrl) view.loadUrl(activeUrl)
                    }
                )
            }

            if (connected && status == 0) CameraStatusOverlay("正在连接…", Hud.Warning)
            if (connected && status == 2) CameraStatusOverlay("画面连接失败，请检查 WiFi 和地址", Hud.Danger)

            if (fullscreen) {
                Row(
                    Modifier.align(Alignment.TopEnd).padding(10.dp)
                        .clip(RoundedCornerShape(8.dp)).background(Hud.Panel.copy(alpha = 0.92f))
                        .border(1.dp, Hud.Border, RoundedCornerShape(8.dp))
                        .clickable { Haptics.tick(); onToggleFullscreen() }
                        .padding(horizontal = 10.dp, vertical = 7.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Icon(Icons.Filled.FullscreenExit, null, tint = Hud.Accent, modifier = Modifier.size(16.dp))
                    Spacer(Modifier.width(5.dp))
                    Text("退出全屏", color = Hud.Accent, fontSize = 11.sp)
                }
            }
        }
    }
}

@Composable
private fun CameraControlBar(
    streamUrl: String,
    onUrlChange: (String) -> Unit,
    streamType: CameraStreamType,
    onTypeChange: (CameraStreamType) -> Unit,
    status: Int,
    connected: Boolean,
    onConnect: () -> Unit,
    onDisconnect: () -> Unit,
    onToggleFullscreen: () -> Unit
) {
    Column(
        Modifier.fillMaxWidth().background(Hud.Panel, RoundedCornerShape(10.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(10.dp))
            .padding(horizontal = 10.dp, vertical = 7.dp)
    ) {
        val statusColor = when {
            !connected -> Hud.TextMuted
            status == 1 -> Hud.Success
            status == 2 -> Hud.Danger
            else -> Hud.Warning
        }
        val statusText = when {
            !connected -> "未连接"
            status == 1 -> "画面已接入"
            status == 2 -> "连接失败"
            else -> "连接中"
        }
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text("摄像头地址", color = Hud.TextSecondary, fontSize = 11.sp)
            Spacer(Modifier.width(7.dp))
            Box(
                Modifier.weight(1f).clip(RoundedCornerShape(6.dp)).background(Hud.PanelDeep)
                    .border(1.dp, Hud.Border, RoundedCornerShape(6.dp)).padding(horizontal = 9.dp, vertical = 6.dp)
            ) {
                BasicTextField(
                    value = streamUrl,
                    onValueChange = onUrlChange,
                    singleLine = true,
                    textStyle = TextStyle(color = Hud.TextPrimary, fontSize = 12.sp),
                    cursorBrush = SolidColor(Hud.Accent),
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Uri),
                    modifier = Modifier.fillMaxWidth()
                )
            }
            Spacer(Modifier.width(9.dp))
            Box(Modifier.size(7.dp).clip(CircleShape).background(statusColor))
            Spacer(Modifier.width(4.dp))
            Text(statusText, color = statusColor, fontSize = 11.sp, maxLines = 1)
        }
        Spacer(Modifier.height(6.dp))
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Text("流类型", color = Hud.TextMuted, fontSize = 10.sp)
            Spacer(Modifier.width(7.dp))
            CameraStreamType.values().forEach { type ->
                CameraTypeChip(type.label, type == streamType) { Haptics.tick(); onTypeChange(type) }
                Spacer(Modifier.width(4.dp))
            }
            Spacer(Modifier.weight(1f))
            CameraActionChip(
                if (connected) Icons.Filled.LinkOff else Icons.Filled.Link,
                if (connected) "断开" else "连接",
                if (connected) Hud.Danger else Hud.Accent,
                if (connected) onDisconnect else onConnect
            )
            Spacer(Modifier.width(6.dp))
            CameraActionChip(Icons.Filled.Fullscreen, "全屏", Hud.Accent, onToggleFullscreen)
        }
    }
}

@Composable
private fun CameraTypeChip(label: String, active: Boolean, onClick: () -> Unit) {
    Text(
        label,
        color = if (active) Hud.Accent else Hud.TextMuted,
        fontSize = 10.sp,
        modifier = Modifier.clip(RoundedCornerShape(6.dp))
            .background(if (active) Hud.Accent.copy(alpha = 0.1f) else Color.Transparent)
            .border(1.dp, if (active) Hud.Accent else Hud.Border, RoundedCornerShape(6.dp))
            .clickable { onClick() }.padding(horizontal = 7.dp, vertical = 6.dp)
    )
}

@Composable
private fun CameraActionChip(
    icon: androidx.compose.ui.graphics.vector.ImageVector,
    label: String,
    color: Color,
    onClick: () -> Unit
) {
    Row(
        Modifier.clip(RoundedCornerShape(8.dp)).border(1.dp, color, RoundedCornerShape(8.dp))
            .clickable { onClick() }.padding(horizontal = 9.dp, vertical = 6.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(icon, null, tint = color, modifier = Modifier.size(15.dp))
        Spacer(Modifier.width(4.dp))
        Text(label, color = color, fontSize = 11.sp, maxLines = 1)
    }
}

@Composable
private fun CameraPlaceholder(title: String, subtitle: String) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        Icon(Icons.Filled.Link, null, tint = Hud.TextMuted, modifier = Modifier.size(42.dp))
        Spacer(Modifier.height(10.dp))
        Text(title, color = Hud.TextSecondary, fontSize = 16.sp)
        Spacer(Modifier.height(4.dp))
        Text(subtitle, color = Hud.TextMuted, fontSize = 11.sp)
    }
}

@Composable
private fun CameraStatusOverlay(text: String, color: Color) {
    Row(
        Modifier.clip(RoundedCornerShape(8.dp)).background(Hud.Panel.copy(alpha = 0.94f))
            .border(1.dp, color, RoundedCornerShape(8.dp)).padding(horizontal = 14.dp, vertical = 9.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.size(7.dp).clip(CircleShape).background(color))
        Spacer(Modifier.width(7.dp))
        Text(text, color = color, fontSize = 12.sp)
    }
}

private fun isRtsp(type: CameraStreamType, url: String): Boolean =
    type == CameraStreamType.RTSP || (type == CameraStreamType.AUTO && url.startsWith("rtsp://", ignoreCase = true))
