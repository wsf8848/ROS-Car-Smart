package com.fyx.beebot.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.BugReport
import androidx.compose.material.icons.filled.MoreVert
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material.icons.filled.WifiOff
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.input.GamepadButton
import com.fyx.beebot.input.GamepadRepository
import com.fyx.beebot.input.GamepadState
import com.fyx.beebot.input.InputEvent
import com.fyx.beebot.ui.theme.Hud
import com.fyx.beebot.util.WifiInfo
import com.fyx.beebot.util.rememberWifiStatus
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlin.math.min

@Composable
fun InputMonitorScreen() {
    val state = GamepadRepository.state
    val events = GamepadRepository.events
    val wifi by rememberWifiStatus()

    Box(Modifier.fillMaxSize().background(Hud.Background)) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(10.dp)
        ) {
            Header(state, wifi)
            Spacer(Modifier.height(8.dp))

            Row(
                modifier = Modifier.fillMaxWidth().weight(1f),
                horizontalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                JoystickCard(Modifier.weight(1f), isLeft = true, state = state)
                ButtonsCard(Modifier.weight(1.15f), state = state)
                JoystickCard(Modifier.weight(1f), isLeft = false, state = state)
            }

            Spacer(Modifier.height(8.dp))
            LogCard(Modifier.fillMaxWidth().height(132.dp), events)
        }
    }
}

@Composable
private fun Header(state: GamepadState, wifi: WifiInfo) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(Hud.Panel, RoundedCornerShape(12.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(12.dp))
            .padding(horizontal = 12.dp, vertical = 8.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(
            modifier = Modifier.size(30.dp).background(Hud.PanelDeep, RoundedCornerShape(8.dp)),
            contentAlignment = Alignment.Center
        ) { Icon(Icons.Filled.BugReport, null, tint = Hud.Warning, modifier = Modifier.size(20.dp)) }
        Spacer(Modifier.width(10.dp))
        Column {
            Text("掌机输入监视", color = Hud.TextPrimary, fontSize = 15.sp, fontWeight = FontWeight.Medium)
            Text("小蜜蜂智控系统 v2.6.2 · 实体摇杆与按键", color = Hud.TextMuted, fontSize = 10.sp)
        }
        Spacer(Modifier.weight(1f))

        WifiPill(wifi)
        Spacer(Modifier.width(8.dp))
        StatusPill(state.connected, state.deviceName)
    }
}

@Composable
private fun WifiPill(wifi: WifiInfo) {
    val fg = when {
        !wifi.connected -> Hud.TextMuted
        wifi.percent >= 60 -> Hud.Success
        wifi.percent >= 30 -> Hud.Warning
        else -> Hud.Danger
    }
    Row(
        modifier = Modifier
            .clip(RoundedCornerShape(20.dp))
            .background(Hud.PanelDeep)
            .padding(horizontal = 10.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Icon(
            if (wifi.connected) Icons.Filled.Wifi else Icons.Filled.WifiOff,
            null, tint = fg, modifier = Modifier.size(15.dp)
        )
        Spacer(Modifier.width(6.dp))
        if (wifi.connected) {
            Text(wifi.ssid, color = Hud.TextPrimary, fontSize = 11.sp, maxLines = 1)
            Spacer(Modifier.width(8.dp))
            val latColor = when {
                wifi.latencyMs < 0 -> Hud.TextMuted
                wifi.latencyMs <= 60 -> Hud.Success
                wifi.latencyMs <= 150 -> Hud.Warning
                else -> Hud.Danger
            }
            Text(
                if (wifi.latencyMs >= 0) "${wifi.latencyMs}ms" else "--ms",
                color = latColor, fontSize = 11.sp, fontFamily = FontFamily.Monospace, maxLines = 1
            )
        } else {
            Text("未连接", color = fg, fontSize = 11.sp, maxLines = 1)
        }
    }
}

@Composable
private fun StatusPill(connected: Boolean, name: String) {
    val bg = if (connected) Color(0xFF0E2A22) else Color(0xFF2A1416)
    val fg = if (connected) Hud.Success else Hud.Danger
    Row(
        modifier = Modifier.clip(RoundedCornerShape(20.dp)).background(bg).padding(horizontal = 10.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.size(7.dp).clip(CircleShape).background(fg))
        Spacer(Modifier.width(6.dp))
        Text(
            if (connected) (if (name.isNotEmpty()) name else "手柄已接入") else "未接入手柄",
            color = fg, fontSize = 11.sp, maxLines = 1
        )
    }
}

@Composable
private fun JoystickCard(modifier: Modifier, isLeft: Boolean, state: GamepadState) {
    val x = if (isLeft) state.leftX else state.rightX
    val y = if (isLeft) state.leftY else state.rightY
    val trigger = if (isLeft) state.leftTrigger else state.rightTrigger
    val b1 = if (isLeft) GamepadButton.L1 else GamepadButton.R1
    val b2 = if (isLeft) GamepadButton.L2 else GamepadButton.R2
    val b3 = if (isLeft) GamepadButton.L3 else GamepadButton.R3

    HudPanel(modifier) {
        SectionLabel(if (isLeft) "左摇杆 LEFT" else "右摇杆 RIGHT")
        Spacer(Modifier.height(6.dp))
        Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
            JoystickPad(x, y)
        }
        Spacer(Modifier.height(6.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
            AxisReadout("X", x, Modifier.weight(1f))
            AxisReadout("Y", y, Modifier.weight(1f))
        }
        Spacer(Modifier.height(6.dp))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
            KeyChip(b1.label, state.isPressed(b1), Modifier.weight(1f))
            KeyChip(
                "${b2.label}·${(trigger * 100).toInt()}%",
                state.isPressed(b2) || trigger > 0.05f,
                Modifier.weight(1.4f)
            )
            KeyChip(b3.label, state.isPressed(b3), Modifier.weight(1f))
        }
    }
}

@Composable
private fun JoystickPad(x: Float, y: Float) {
    Canvas(Modifier.fillMaxHeight().aspectRatio(1f)) {
        val r = min(size.width, size.height) / 2f
        val c = Offset(size.width / 2f, size.height / 2f)
        // 外圈
        drawCircle(Hud.PanelDeep, radius = r, center = c)
        drawCircle(Hud.Border, radius = r, center = c, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 1.5f))
        drawCircle(Hud.BorderSoft, radius = r * 0.5f, center = c, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 1f))
        // 十字准线
        drawLine(Hud.BorderSoft, Offset(c.x, c.y - r), Offset(c.x, c.y + r), strokeWidth = 1f)
        drawLine(Hud.BorderSoft, Offset(c.x - r, c.y), Offset(c.x + r, c.y), strokeWidth = 1f)
        // 光点
        val knob = Offset(c.x + x * r * 0.86f, c.y + y * r * 0.86f)
        drawLine(Hud.AccentDim, c, knob, strokeWidth = 2f)
        drawCircle(Hud.Accent, radius = r * 0.16f, center = knob)
        drawCircle(Hud.Background, radius = r * 0.16f, center = knob, style = androidx.compose.ui.graphics.drawscope.Stroke(width = 2f))
    }
}

@Composable
private fun AxisReadout(label: String, value: Float, modifier: Modifier) {
    Row(
        modifier = modifier
            .background(Hud.PanelDeep, RoundedCornerShape(6.dp))
            .padding(horizontal = 8.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text(label, color = Hud.TextMuted, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
        Spacer(Modifier.weight(1f))
        Text(
            "%+.2f".format(value),
            color = if (kotlin.math.abs(value) > 0.02f) Hud.Accent else Hud.TextSecondary,
            fontSize = 13.sp, fontFamily = FontFamily.Monospace, fontWeight = FontWeight.Medium
        )
    }
}

@Composable
private fun ButtonsCard(modifier: Modifier, state: GamepadState) {
    HudPanel(modifier) {
        SectionLabel("按键 BUTTONS")
        Spacer(Modifier.height(6.dp))
        Row(
            modifier = Modifier.weight(1f).fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.SpaceEvenly
        ) {
            DpadCross(state)
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                KeyChip("SELECT", state.isPressed(GamepadButton.SELECT), Modifier.width(78.dp))
                Spacer(Modifier.height(8.dp))
                KeyChip("START", state.isPressed(GamepadButton.START), Modifier.width(78.dp))
            }
            FaceButtons(state)
        }
    }
}

@Composable
private fun DpadCross(state: GamepadState) {
    val s = 26.dp
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        KeyBox("↑", state.isPressed(GamepadButton.DPAD_UP), s)
        Row {
            KeyBox("←", state.isPressed(GamepadButton.DPAD_LEFT), s)
            Spacer(Modifier.width(s))
            KeyBox("→", state.isPressed(GamepadButton.DPAD_RIGHT), s)
        }
        KeyBox("↓", state.isPressed(GamepadButton.DPAD_DOWN), s)
    }
}

@Composable
private fun FaceButtons(state: GamepadState) {
    val s = 30.dp
    // 布局：X(上) · Y(左) · A(右) · B(下)
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        KeyCircle("X", state.isPressed(GamepadButton.X), s)
        Row(verticalAlignment = Alignment.CenterVertically) {
            KeyCircle("Y", state.isPressed(GamepadButton.Y), s)
            Spacer(Modifier.width(s))
            KeyCircle("A", state.isPressed(GamepadButton.A), s)
        }
        KeyCircle("B", state.isPressed(GamepadButton.B), s)
    }
}

@Composable
private fun LogCard(modifier: Modifier, events: List<InputEvent>) {
    val fmt = remember { SimpleDateFormat("HH:mm:ss.SSS", Locale.US) }
    HudPanel(modifier) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            SectionLabel("实时信号日志")
            Spacer(Modifier.weight(1f))
            Text("采样 · 事件驱动", color = Hud.TextMuted, fontSize = 10.sp)
        }
        Spacer(Modifier.height(4.dp))
        LazyColumn(Modifier.fillMaxSize()) {
            items(events) { ev ->
                Row(Modifier.fillMaxWidth().padding(vertical = 1.dp)) {
                    Text(fmt.format(Date(ev.timestamp)), color = Hud.TextMuted, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                    Spacer(Modifier.width(8.dp))
                    Text(
                        tagOf(ev.type), color = colorOf(ev.type), fontSize = 11.sp,
                        fontFamily = FontFamily.Monospace, modifier = Modifier.width(72.dp)
                    )
                    Text(ev.message, color = Hud.TextSecondary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
                }
            }
        }
    }
}

private fun tagOf(t: InputEvent.Type) = when (t) {
    InputEvent.Type.KEY_DOWN -> "KEY_DOWN"
    InputEvent.Type.KEY_UP -> "KEY_UP"
    InputEvent.Type.AXIS -> "AXIS"
    InputEvent.Type.SYSTEM -> "SYS"
}

private fun colorOf(t: InputEvent.Type) = when (t) {
    InputEvent.Type.KEY_DOWN -> Hud.Success
    InputEvent.Type.KEY_UP -> Hud.TextMuted
    InputEvent.Type.AXIS -> Hud.Accent
    InputEvent.Type.SYSTEM -> Hud.Warning
}

// ---- 通用小组件 ----

@Composable
private fun HudPanel(modifier: Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = modifier
            .background(Hud.Panel, RoundedCornerShape(12.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(12.dp))
            .padding(10.dp),
        content = content
    )
}

@Composable
private fun SectionLabel(text: String) {
    Text(text, color = Hud.TextSecondary, fontSize = 12.sp, fontWeight = FontWeight.Medium)
}

@Composable
private fun KeyChip(text: String, active: Boolean, modifier: Modifier = Modifier) {
    Box(
        modifier = modifier
            .clip(RoundedCornerShape(6.dp))
            .background(if (active) Hud.Accent else Hud.PanelDeep)
            .border(1.dp, if (active) Hud.Accent else Hud.BorderSoft, RoundedCornerShape(6.dp))
            .padding(vertical = 6.dp),
        contentAlignment = Alignment.Center
    ) {
        Text(
            text, color = if (active) Color(0xFF04212A) else Hud.TextMuted,
            fontSize = 11.sp, fontWeight = if (active) FontWeight.Medium else FontWeight.Normal, maxLines = 1
        )
    }
}

@Composable
private fun KeyBox(text: String, active: Boolean, size: androidx.compose.ui.unit.Dp) {
    Box(
        modifier = Modifier.padding(2.dp).size(size)
            .clip(RoundedCornerShape(6.dp))
            .background(if (active) Hud.Accent else Hud.PanelDeep)
            .border(1.dp, if (active) Hud.Accent else Hud.Border, RoundedCornerShape(6.dp)),
        contentAlignment = Alignment.Center
    ) {
        Text(text, color = if (active) Color(0xFF04212A) else Hud.TextSecondary, fontSize = 14.sp)
    }
}

@Composable
private fun KeyCircle(text: String, active: Boolean, size: androidx.compose.ui.unit.Dp) {
    val fg = if (active) Color(0xFF04212A) else Hud.TextSecondary
    Box(
        modifier = Modifier.padding(2.dp).size(size)
            .clip(CircleShape)
            .background(if (active) Hud.Accent else Hud.PanelDeep)
            .border(1.dp, if (active) Hud.Accent else Hud.Border, CircleShape),
        contentAlignment = Alignment.Center
    ) {
        Text(text, color = fg, fontSize = 14.sp, fontWeight = FontWeight.Medium)
    }
}
