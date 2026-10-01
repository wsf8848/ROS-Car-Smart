package com.fyx.beebot.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Block
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material.icons.filled.Stop
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.bt.AckermannController
import com.fyx.beebot.bt.BluetoothRepository
import com.fyx.beebot.input.GamepadRepository
import com.fyx.beebot.ui.theme.Hud
import kotlinx.coroutines.delay
import kotlin.math.abs
import kotlin.math.min

@Composable
fun AckermannScreen() {
    val state = GamepadRepository.state
    val connected = BluetoothRepository.connected

    // 注册遥测解析，离开页面自动停车
    DisposableEffect(Unit) {
        BluetoothRepository.rxListener = { AckermannController.feedRx(it) }
        onDispose {
            BluetoothRepository.rxListener = null
            AckermannController.stopRemote()
        }
    }
    // 遥控发送循环（100ms）
    LaunchedEffect(AckermannController.remoting) {
        while (AckermannController.remoting) {
            AckermannController.tick(GamepadRepository.state)
            delay(100)
        }
    }

    val thr = AckermannController.throttleOf(state)
    val steer = AckermannController.steeringOf(state)
    val cmd = AckermannController.mapCommand(state)

    Column(Modifier.fillMaxSize()) {
        // 控制条
        Row(
            modifier = Modifier.fillMaxWidth()
                .background(Hud.Panel, RoundedCornerShape(10.dp))
                .border(1.dp, Hud.Border, RoundedCornerShape(10.dp))
                .padding(horizontal = 12.dp, vertical = 7.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            val fg = if (connected) Hud.Success else Hud.Danger
            Row(
                Modifier.clip(RoundedCornerShape(20.dp)).background(if (connected) Color0E2A22 else Color2A1416).padding(horizontal = 10.dp, vertical = 5.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Box(Modifier.size(7.dp).clip(CircleShape).background(fg))
                Spacer(Modifier.width(6.dp))
                Text(if (connected) "已连接 ${BluetoothRepository.deviceName}" else "未连接", color = fg, fontSize = 11.sp, maxLines = 1)
            }
            Spacer(Modifier.width(10.dp))
            // 遥控开关
            val remoting = AckermannController.remoting
            Row(
                Modifier.clip(RoundedCornerShape(20.dp))
                    .background(if (remoting) Color0E1E2A else Hud.PanelDeep)
                    .border(1.dp, if (remoting) Hud.Accent else Hud.Border, RoundedCornerShape(20.dp))
                    .clickable {
                        com.fyx.beebot.util.Haptics.click()
                        if (remoting) AckermannController.stopRemote()
                        else if (connected) AckermannController.startRemote()
                    }
                    .padding(horizontal = 12.dp, vertical = 5.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(if (remoting) Icons.Filled.Stop else Icons.Filled.PlayArrow, null,
                    tint = if (remoting) Hud.Accent else Hud.TextSecondary, modifier = Modifier.size(15.dp))
                Spacer(Modifier.width(5.dp))
                Text(if (remoting) "遥控中" else "开始遥控", color = if (remoting) Hud.Accent else Hud.TextSecondary, fontSize = 12.sp)
            }
            Spacer(Modifier.weight(1f))
            Text("发送周期", color = Hud.TextMuted, fontSize = 11.sp)
            Spacer(Modifier.width(6.dp))
            Text("100ms", color = Hud.TextPrimary, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
            Spacer(Modifier.width(12.dp))
            Row(
                Modifier.clip(RoundedCornerShape(8.dp)).border(1.dp, Hud.Danger, RoundedCornerShape(8.dp))
                    .clickable { com.fyx.beebot.util.Haptics.heavy(); AckermannController.emergencyStop() }
                    .padding(horizontal = 14.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(Icons.Filled.Block, null, tint = Hud.Danger, modifier = Modifier.size(16.dp))
                Spacer(Modifier.width(5.dp))
                Text("紧急停车", color = Hud.Danger, fontSize = 13.sp, fontWeight = FontWeight.Medium)
            }
        }

        Spacer(Modifier.height(8.dp))

        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {

            // 左：遥控源
            AckPanel(Modifier.weight(1f)) {
                SecLabel("遥控源（实体摇杆）")
                Spacer(Modifier.height(6.dp))
                Box(Modifier.fillMaxWidth().weight(1f), contentAlignment = Alignment.Center) {
                    ControlPad(steer, thr)
                }
                Spacer(Modifier.height(6.dp))
                Text("左摇杆Y=油门 · 右摇杆X=转向", color = Hud.TextMuted, fontSize = 10.sp, modifier = Modifier.align(Alignment.CenterHorizontally))
                Spacer(Modifier.height(6.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("油门 ${(thr * 100).toInt()}%", color = if (abs(thr) > 0.25f) Hud.Success else Hud.TextSecondary, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                    Text("转向 ${(steer * 100).toInt()}%", color = if (abs(steer) > 0.25f) Hud.Accent else Hud.TextSecondary, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
                }
                Spacer(Modifier.height(6.dp))
                Column(
                    Modifier.fillMaxWidth().background(Hud.PanelDeep, RoundedCornerShape(7.dp)).padding(vertical = 6.dp),
                    horizontalAlignment = Alignment.CenterHorizontally
                ) {
                    Text("当前指令", color = Hud.TextMuted, fontSize = 10.sp)
                    Text(cmd.second, color = Hud.Accent, fontSize = 15.sp, fontWeight = FontWeight.Medium)
                    Text("TX '${cmd.first}' (0x${"%02X".format(cmd.first.code)})", color = Hud.TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                }
                Spacer(Modifier.height(6.dp))
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Text("速度档", color = Hud.TextMuted, fontSize = 11.sp)
                    Spacer(Modifier.weight(1f))
                    Text("${AckermannController.rcVelocity} mm/s", color = Hud.Warning, fontSize = 13.sp, fontFamily = FontFamily.Monospace)
                }
                Text("R2/R1 加速 · L2/L1 减速", color = Hud.TextMuted, fontSize = 10.sp, modifier = Modifier.padding(top = 3.dp))
            }

            // 中：转向/油门可视化
            AckPanel(Modifier.weight(1.1f)) {
                SecLabel("转向 / 油门")
                Spacer(Modifier.height(6.dp))
                Box(Modifier.fillMaxWidth().weight(1f), contentAlignment = Alignment.Center) {
                    CarView(steer, thr)
                }
                Spacer(Modifier.height(6.dp))
                // 舵量条
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                    Text("左满舵", color = Hud.TextMuted, fontSize = 10.sp)
                    Text("直行", color = Hud.TextMuted, fontSize = 10.sp)
                    Text("右满舵", color = Hud.TextMuted, fontSize = 10.sp)
                }
                Box(Modifier.fillMaxWidth().height(8.dp).background(Hud.PanelDeep, RoundedCornerShape(4.dp))) {
                    Box(Modifier.fillMaxHeight().width(1.dp).align(Alignment.Center).background(Hud.Border))
                    Box(
                        Modifier.fillMaxWidth(fraction = ((steer + 1f) / 2f).coerceIn(0f, 1f))
                    ) {
                        Box(Modifier.size(12.dp).align(Alignment.CenterEnd).clip(CircleShape).background(Hud.Accent))
                    }
                }
                Spacer(Modifier.height(10.dp))
                Box(
                    Modifier.align(Alignment.CenterHorizontally).clip(RoundedCornerShape(7.dp))
                        .background(Color0E2A22).padding(horizontal = 14.dp, vertical = 6.dp)
                ) { Text("动作：${cmd.second}", color = Hud.Success, fontSize = 13.sp) }
            }

            // 右：小车遥测
            AckPanel(Modifier.weight(1f)) {
                Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    SecLabel("小车遥测 {A}")
                    Spacer(Modifier.weight(1f))
                    Text(if (AckermannController.telemetryMs >= 0) "${AckermannController.telemetryMs}ms" else "--ms",
                        color = Hud.TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
                }
                Spacer(Modifier.height(8.dp))
                TeleBig("左电机速度", "%.2f".format(AckermannController.leftSpeed), "m/s", Hud.Success)
                Spacer(Modifier.height(7.dp))
                TeleBig("右电机速度", "%.2f".format(AckermannController.rightSpeed), "m/s", Hud.Success)
                Spacer(Modifier.height(7.dp))
                Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                    TeleSmall("电量", "${AckermannController.battery}%", Hud.Warning, Modifier.weight(1f))
                    TeleSmall("角度 Z", "${AckermannController.gyroZ}", Hud.Accent, Modifier.weight(1f))
                }
            }
        }
    }
}

private val Color0E2A22 = androidx.compose.ui.graphics.Color(0xFF0E2A22)
private val Color2A1416 = androidx.compose.ui.graphics.Color(0xFF2A1416)
private val Color0E1E2A = androidx.compose.ui.graphics.Color(0xFF0E1E2A)

@Composable
private fun ControlPad(steer: Float, thr: Float) {
    Canvas(Modifier.fillMaxHeight().aspectRatio(1f)) {
        val r = min(size.width, size.height) / 2f
        val c = Offset(size.width / 2f, size.height / 2f)
        drawCircle(Hud.PanelDeep, r, c)
        drawCircle(Hud.Border, r, c, style = Stroke(1.5f))
        drawLine(Hud.BorderSoft, Offset(c.x, c.y - r), Offset(c.x, c.y + r), 1f)
        drawLine(Hud.BorderSoft, Offset(c.x - r, c.y), Offset(c.x + r, c.y), 1f)
        val knob = Offset(c.x + steer.coerceIn(-1f, 1f) * r * 0.85f, c.y - thr.coerceIn(-1f, 1f) * r * 0.85f)
        drawLine(Hud.AccentDim, c, knob, 2f)
        drawCircle(Hud.Accent, r * 0.16f, knob)
    }
}

@Composable
private fun CarView(steer: Float, thr: Float) {
    Canvas(Modifier.fillMaxHeight(0.9f).aspectRatio(0.8f)) {
        val w = size.width; val h = size.height
        val bodyW = w * 0.34f
        val bodyLeft = (w - bodyW) / 2f
        val bodyTop = h * 0.24f
        val bodyH = h * 0.52f
        // 车身
        drawRoundRect(Hud.PanelDeep, topLeft = Offset(bodyLeft, bodyTop), size = androidx.compose.ui.geometry.Size(bodyW, bodyH),
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(10f, 10f))
        drawRoundRect(Hud.Border, topLeft = Offset(bodyLeft, bodyTop), size = androidx.compose.ui.geometry.Size(bodyW, bodyH),
            cornerRadius = androidx.compose.ui.geometry.CornerRadius(10f, 10f), style = Stroke(1.5f))
        // 前进指示
        val fwd = thr > 0.25f
        val back = thr < -0.25f
        val dirColor = if (fwd) Hud.Success else if (back) Hud.Warning else Hud.TextMuted
        drawLine(dirColor, Offset(w / 2f, bodyTop), Offset(w / 2f, bodyTop - h * 0.12f), 3f)
        // 后轮（固定）
        val wheelW = w * 0.09f; val wheelH = h * 0.13f
        drawRoundRect(Hud.TextMuted, topLeft = Offset(bodyLeft - wheelW * 0.6f, bodyTop + bodyH - wheelH), size = androidx.compose.ui.geometry.Size(wheelW, wheelH), cornerRadius = androidx.compose.ui.geometry.CornerRadius(4f, 4f))
        drawRoundRect(Hud.TextMuted, topLeft = Offset(bodyLeft + bodyW - wheelW * 0.4f, bodyTop + bodyH - wheelH), size = androidx.compose.ui.geometry.Size(wheelW, wheelH), cornerRadius = androidx.compose.ui.geometry.CornerRadius(4f, 4f))
        // 前轮（随转向偏摆，仅示意）
        val ox = steer.coerceIn(-1f, 1f) * wheelW * 0.5f
        drawRoundRect(Hud.Accent, topLeft = Offset(bodyLeft - wheelW * 0.6f + ox, bodyTop + ox * 0.3f), size = androidx.compose.ui.geometry.Size(wheelW, wheelH), cornerRadius = androidx.compose.ui.geometry.CornerRadius(4f, 4f))
        drawRoundRect(Hud.Accent, topLeft = Offset(bodyLeft + bodyW - wheelW * 0.4f + ox, bodyTop - ox * 0.3f), size = androidx.compose.ui.geometry.Size(wheelW, wheelH), cornerRadius = androidx.compose.ui.geometry.CornerRadius(4f, 4f))
    }
}

@Composable
private fun AckPanel(modifier: Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = modifier.fillMaxHeight()
            .background(Hud.Panel, RoundedCornerShape(10.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(10.dp))
            .padding(10.dp),
        content = content
    )
}

@Composable
private fun SecLabel(t: String) = Text(t, color = Hud.TextSecondary, fontSize = 12.sp, fontWeight = FontWeight.Medium)

@Composable
private fun TeleBig(label: String, value: String, unit: String, color: androidx.compose.ui.graphics.Color) {
    Column(Modifier.fillMaxWidth().background(Hud.PanelDeep, RoundedCornerShape(8.dp)).padding(10.dp)) {
        Text(label, color = Hud.TextMuted, fontSize = 10.sp)
        Row(verticalAlignment = Alignment.Bottom) {
            Text(value, color = color, fontSize = 20.sp, fontWeight = FontWeight.Medium, fontFamily = FontFamily.Monospace)
            Spacer(Modifier.width(4.dp))
            Text(unit, color = Hud.TextMuted, fontSize = 11.sp)
        }
    }
}

@Composable
private fun TeleSmall(label: String, value: String, color: androidx.compose.ui.graphics.Color, modifier: Modifier) {
    Column(modifier.background(Hud.PanelDeep, RoundedCornerShape(8.dp)).padding(10.dp)) {
        Text(label, color = Hud.TextMuted, fontSize = 10.sp)
        Text(value, color = color, fontSize = 18.sp, fontWeight = FontWeight.Medium, fontFamily = FontFamily.Monospace)
    }
}
