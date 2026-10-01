package com.fyx.beebot.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.DirectionsCar
import androidx.compose.material.icons.filled.Gamepad
import androidx.compose.material.icons.filled.Public
import androidx.compose.material.icons.filled.RecordVoiceOver
import androidx.compose.material.icons.filled.Settings
import androidx.compose.material.icons.filled.SmartToy
import androidx.compose.material.icons.filled.Videocam
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.speech.VoiceAssistantController
import com.fyx.beebot.ui.theme.Hud

/**
 * ⋮ 打开的工具页：保留原有控制与调试功能，并把旧默认掌机页移入首个标签。
 */
@Composable
fun ToolsScreen(onClose: () -> Unit, onLogout: () -> Unit, initialTab: Int = 0) {
    var tab by rememberSaveable { mutableStateOf(initialTab.coerceIn(0, 7)) } // 0=掌机监视,1=蓝牙调试,2=智能小车,3=阿克曼小车,4=ROS网页,5=摄像头画面,6=语音识别,7=设置
    var rosFull by rememberSaveable { mutableStateOf(false) }
    var cameraFull by rememberSaveable { mutableStateOf(false) }
    val contentFullscreen = (tab == 4 && rosFull) || (tab == 5 && cameraFull)

    fun closeTools() {
        if (tab == 6) VoiceAssistantController.detach()
        onClose()
    }

    fun selectTab(nextTab: Int) {
        if (tab == 6 && nextTab != 6) VoiceAssistantController.detach()
        tab = nextTab
    }

    // ROS 网页页禁用「右滑返回」手势，避免与网页里地图拖拽/横向操作冲突
    val base = Modifier.fillMaxSize().background(Hud.Background)
    val swipe = if (tab != 4 && tab != 5) base.pointerInput(Unit) {
        var total = 0f
        detectHorizontalDragGestures(
            onDragEnd = { if (total > 140f) closeTools(); total = 0f },
            onDragCancel = { total = 0f }
        ) { _, drag -> total += drag }
    } else base

    Box(modifier = swipe) {
        if (contentFullscreen) {
            ToolContent(tab, rosFull, cameraFull, { rosFull = !rosFull }, { cameraFull = !cameraFull }, onLogout)
        } else {
            Row(Modifier.fillMaxSize().padding(10.dp)) {
                Column(
                    Modifier.width(132.dp).fillMaxHeight().background(Hud.Panel, RoundedCornerShape(12.dp))
                        .border(1.dp, Hud.Border, RoundedCornerShape(12.dp)).padding(8.dp),
                    verticalArrangement = Arrangement.spacedBy(5.dp)
                ) {
                    ToolRailChip(Icons.Filled.ArrowBack, "返回主控", false) {
                        com.fyx.beebot.util.Haptics.tick(); closeTools()
                    }
                    Spacer(Modifier.height(3.dp))
                    ToolRailChip(Icons.Filled.Gamepad, "掌机监视", tab == 0) { selectTab(0) }
                    ToolRailChip(Icons.Filled.Bluetooth, "蓝牙调试", tab == 1) { selectTab(1) }
                    ToolRailChip(Icons.Filled.SmartToy, "智能小车", tab == 2) { selectTab(2) }
                    ToolRailChip(Icons.Filled.DirectionsCar, "阿克曼小车", tab == 3) { selectTab(3) }
                    ToolRailChip(Icons.Filled.Public, "ROS 网页", tab == 4) { selectTab(4) }
                    ToolRailChip(Icons.Filled.Videocam, "摄像头画面", tab == 5) { selectTab(5) }
                    ToolRailChip(Icons.Filled.RecordVoiceOver, "语音识别", tab == 6) { selectTab(6) }
                    ToolRailChip(Icons.Filled.Settings, "系统设置", tab == 7) { selectTab(7) }
                    Spacer(Modifier.weight(1f))
                    Text("向右滑动返回", color = Hud.TextMuted, fontSize = 9.sp, modifier = Modifier.align(Alignment.CenterHorizontally))
                }
                Spacer(Modifier.width(8.dp))
                Box(Modifier.weight(1f).fillMaxHeight()) {
                    ToolContent(tab, rosFull, cameraFull, { rosFull = !rosFull }, { cameraFull = !cameraFull }, onLogout)
                }
            }
        }
    }
}

@Composable
private fun ToolContent(
    tab: Int,
    rosFull: Boolean,
    cameraFull: Boolean,
    toggleRos: () -> Unit,
    toggleCamera: () -> Unit,
    onLogout: () -> Unit
) {
    when (tab) {
        0 -> InputMonitorScreen()
        1 -> BluetoothScreen()
        2 -> SmartCarScreen()
        3 -> AckermannScreen()
        4 -> RosWebScreen(fullscreen = rosFull, onToggleFullscreen = toggleRos)
        5 -> CameraStreamScreen(fullscreen = cameraFull, onToggleFullscreen = toggleCamera)
        6 -> SpeechRecognitionScreen()
        else -> SettingsContent(onLogout)
    }
}

@Composable
private fun ToolRailChip(icon: ImageVector, label: String, active: Boolean, onClick: () -> Unit) {
    Row(
        modifier = Modifier
            .fillMaxWidth().height(38.dp).clip(RoundedCornerShape(8.dp))
            .background(if (active) Color(0xFF0E1E2A) else Color.Transparent)
            .border(1.dp, if (active) Hud.Accent else Color.Transparent, RoundedCornerShape(8.dp))
            .clickable { com.fyx.beebot.util.Haptics.tick(); onClick() }
            .padding(horizontal = 9.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(icon, null, tint = if (active) Hud.Accent else Hud.TextSecondary, modifier = Modifier.size(18.dp))
        Spacer(Modifier.width(8.dp))
        Text(
            label, color = if (active) Hud.Accent else Hud.TextSecondary,
            fontSize = 11.sp, fontWeight = if (active) FontWeight.Medium else FontWeight.Normal,
            maxLines = 1
        )
    }
}
