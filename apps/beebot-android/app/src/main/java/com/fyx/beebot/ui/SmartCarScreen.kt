package com.fyx.beebot.ui

import android.os.SystemClock
import androidx.compose.ui.platform.LocalContext
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
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.bt.AckermannController
import com.fyx.beebot.bt.BluetoothRepository
import com.fyx.beebot.bt.SmartCarTelemetry
import com.fyx.beebot.bt.SmartCarMode
import com.fyx.beebot.bt.SmartCarModeController
import com.fyx.beebot.input.GamepadButton
import com.fyx.beebot.input.GamepadRepository
import com.fyx.beebot.transport.VehicleControlAuthorization
import com.fyx.beebot.ui.theme.Hud
import kotlinx.coroutines.delay
import java.util.Locale
import kotlin.math.abs
import kotlin.math.min

@Composable
fun SmartCarScreen() {
    val gamepad = GamepadRepository.state
    val connected = BluetoothRepository.connected
    val controlLocked = VehicleControlAuthorization.locked
    val context = LocalContext.current
    var clock by remember { mutableStateOf(SystemClock.elapsedRealtime()) }
    var previousButtons by remember { mutableStateOf(emptySet<GamepadButton>()) }

    DisposableEffect(Unit) {
        SmartCarModeController.initializeVoice(context)
        SmartCarTelemetry.resetSession()
        BluetoothRepository.frameLoggingEnabled = false
        BluetoothRepository.backgroundRxListener = SmartCarTelemetry::feedRx
        onDispose {
            BluetoothRepository.backgroundRxListener = null
            BluetoothRepository.frameLoggingEnabled = true
            AckermannController.stopRemote()
            SmartCarModeController.releaseVoice()
        }
    }
    LaunchedEffect(AckermannController.remoting, controlLocked, SmartCarModeController.mode) {
        if (controlLocked && AckermannController.remoting) SmartCarModeController.emergencyStop()
        while (AckermannController.remoting && !controlLocked) {
            if (SmartCarModeController.mode != SmartCarMode.AUTONOMOUS) {
                AckermannController.smartCarTick(GamepadRepository.state)
            }
            delay(100)
        }
    }
    LaunchedEffect(connected) {
        if (connected && !AckermannController.remoting) {
            SmartCarModeController.selectMode(SmartCarModeController.mode)
        }
    }
    LaunchedEffect(Unit) {
        while (true) {
            clock = SystemClock.elapsedRealtime()
            delay(250)
        }
    }
    LaunchedEffect(Unit) {
        while (true) {
            val percent = SmartCarTelemetry.powerPercent
            if (percent != null && percent in 1..9) {
                SmartCarModeController.lowBatteryReminder()
            }
            delay(10_000)
        }
    }
    LaunchedEffect(gamepad.seq) {
        val pressedNow = gamepad.buttons
        val newlyPressed = pressedNow - previousButtons
        when {
            GamepadButton.A in newlyPressed -> SmartCarModeController.emergencyStop()
            !controlLocked && GamepadButton.B in newlyPressed -> SmartCarModeController.selectMode(SmartCarMode.BLUETOOTH)
            !controlLocked && GamepadButton.Y in newlyPressed -> SmartCarModeController.selectMode(SmartCarMode.AVOIDANCE)
            !controlLocked && GamepadButton.X in newlyPressed -> SmartCarModeController.selectMode(SmartCarMode.AUTONOMOUS)
        }
        previousButtons = pressedNow
    }

    val throttle = AckermannController.throttleOf(gamepad)
    val steering = AckermannController.steeringOf(gamepad)
    val command = AckermannController.mapCommand(gamepad)
    val radarFresh = SmartCarTelemetry.radarAgeMs(clock) in 0..SmartCarTelemetry.TELEMETRY_TIMEOUT_MS
    val chassisFresh = SmartCarTelemetry.chassisAgeMs(clock) in 0..SmartCarTelemetry.TELEMETRY_TIMEOUT_MS
    val powerFresh = SmartCarTelemetry.powerAgeMs(clock) in 0..SmartCarTelemetry.TELEMETRY_TIMEOUT_MS

    Column(Modifier.fillMaxSize()) {
        SmartControlBar(connected, radarFresh, controlLocked)
        Spacer(Modifier.height(8.dp))
        Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            SmartPanel(Modifier.weight(1.08f)) {
                RadarPanel(radarFresh)
            }
            SmartPanel(Modifier.weight(1.2f)) {
                DrivePanel(throttle, steering, command)
            }
            SmartPanel(Modifier.weight(1f)) {
                ChassisPanel(chassisFresh, powerFresh)
            }
        }
    }
}

@Composable
private fun SmartControlBar(connected: Boolean, radarFresh: Boolean, controlLocked: Boolean) {
    Column(
        Modifier.fillMaxWidth().background(Hud.Panel, RoundedCornerShape(10.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(10.dp)).padding(horizontal = 12.dp, vertical = 7.dp)
    ) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            StatusPill(
                if (connected) "已连接 ${BluetoothRepository.deviceName}" else BluetoothRepository.statusMsg,
                if (connected) Hud.Success else Hud.Danger
            )
            Spacer(Modifier.width(7.dp))
            StatusPill(
                "雷达 ${if (radarFresh) SmartCarTelemetry.lidarStatusText() else "无数据"}",
                if (radarFresh && SmartCarTelemetry.lidarStatus == 1) Hud.Success else Hud.Danger
            )
            Spacer(Modifier.width(7.dp))
            StatusPill("小车 ${SmartCarTelemetry.carStatusText()}", Hud.Accent)
            Spacer(Modifier.weight(1f))
            Text("速度 ${AckermannController.rcVelocity} mm/s", color = Hud.Warning, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
        }
        Spacer(Modifier.height(6.dp))

        val mode = SmartCarModeController.mode
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
            Row(
                Modifier.clip(RoundedCornerShape(20.dp)).background(Hud.PanelDeep)
                    .border(1.dp, Hud.Accent, RoundedCornerShape(20.dp))
                    .clickable(enabled = !controlLocked) { SmartCarModeController.cycleMode() }
                    .padding(horizontal = 11.dp, vertical = 5.dp),
                verticalAlignment = Alignment.CenterVertically
            ) { Text("${mode.label} · ${mode.gamepadKey}", color = Hud.Accent, fontSize = 12.sp, maxLines = 1) }
            Spacer(Modifier.width(7.dp))
            val remoting = AckermannController.remoting
            Row(
                Modifier.clip(RoundedCornerShape(20.dp))
                    .background(if (remoting) Color(0xFF0E1E2A) else Hud.PanelDeep)
                    .border(1.dp, if (remoting) Hud.Accent else Hud.Border, RoundedCornerShape(20.dp))
                    .clickable {
                        com.fyx.beebot.util.Haptics.click()
                        if (remoting) AckermannController.stopRemote()
                        else if (connected && !controlLocked) AckermannController.startRemote()
                    }.padding(horizontal = 11.dp, vertical = 5.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(if (remoting) Icons.Filled.Stop else Icons.Filled.PlayArrow, null,
                    tint = if (remoting) Hud.Accent else Hud.TextSecondary, modifier = Modifier.size(15.dp))
                Spacer(Modifier.width(5.dp))
                Text(if (remoting) "遥控中" else "开始遥控", color = if (remoting) Hud.Accent else Hud.TextSecondary, fontSize = 12.sp, maxLines = 1)
            }
            Spacer(Modifier.weight(1f))
            if (controlLocked) Text("车辆已上锁", color = Hud.Danger, fontSize = 12.sp, modifier = Modifier.padding(end = 9.dp))
            Row(
                Modifier.clip(RoundedCornerShape(8.dp)).border(1.dp, Hud.Danger, RoundedCornerShape(8.dp))
                    .clickable { SmartCarModeController.emergencyStop() }
                    .padding(horizontal = 12.dp, vertical = 6.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Icon(Icons.Filled.Block, null, tint = Hud.Danger, modifier = Modifier.size(16.dp))
                Spacer(Modifier.width(5.dp))
                Text("A · 紧急停车", color = Hud.Danger, fontSize = 13.sp, fontWeight = FontWeight.Medium, maxLines = 1)
            }
        }
    }
}

@Composable
private fun StatusPill(text: String, color: Color) {
    Row(
        Modifier.clip(RoundedCornerShape(20.dp)).background(color.copy(alpha = 0.1f))
            .padding(horizontal = 9.dp, vertical = 5.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.size(7.dp).clip(CircleShape).background(color))
        Spacer(Modifier.width(5.dp))
        Text(text, color = color, fontSize = 11.sp, maxLines = 1)
    }
}

@Composable
private fun ColumnScope.RadarPanel(fresh: Boolean) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        SectionLabel("激光雷达 · 四向最近距离")
        Spacer(Modifier.weight(1f))
        Text(
            if (SmartCarTelemetry.radarIntervalMs >= 0) "${SmartCarTelemetry.radarIntervalMs}ms" else "--ms",
            color = Hud.TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace
        )
    }
    Spacer(Modifier.height(4.dp))
    Box(Modifier.fillMaxWidth().weight(1f), contentAlignment = Alignment.Center) {
        RadarView(fresh)
    }
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
        RadarLegend("安全", Hud.Success)
        RadarLegend("接近", Hud.Warning)
        RadarLegend("危险", Hud.Danger)
    }
}

@Composable
private fun RadarView(fresh: Boolean) {
    Box(Modifier.fillMaxHeight().aspectRatio(1f)) {
        Canvas(Modifier.fillMaxSize()) {
            val center = Offset(size.width / 2f, size.height / 2f)
            val radius = min(size.width, size.height) * 0.38f
            drawCircle(Hud.PanelDeep, radius, center)
            drawCircle(Hud.Border, radius, center, style = Stroke(1.5f))
            drawCircle(Hud.BorderSoft, radius * 0.66f, center, style = Stroke(1f))
            drawCircle(Hud.BorderSoft, radius * 0.33f, center, style = Stroke(1f))
            drawLine(Hud.BorderSoft, Offset(center.x, center.y - radius), Offset(center.x, center.y + radius), 1f)
            drawLine(Hud.BorderSoft, Offset(center.x - radius, center.y), Offset(center.x + radius, center.y), 1f)
            drawRoundRect(
                Hud.AccentDim, Offset(center.x - radius * 0.17f, center.y - radius * 0.29f),
                androidx.compose.ui.geometry.Size(radius * 0.34f, radius * 0.58f),
                androidx.compose.ui.geometry.CornerRadius(10f, 10f)
            )
        }
        DistanceValue("前方", SmartCarTelemetry.frontDistanceCm, fresh, Modifier.align(Alignment.TopCenter))
        DistanceValue("后方", SmartCarTelemetry.rearDistanceCm, fresh, Modifier.align(Alignment.BottomCenter))
        DistanceValue("左侧", SmartCarTelemetry.leftDistanceCm, fresh, Modifier.align(Alignment.CenterStart))
        DistanceValue("右侧", SmartCarTelemetry.rightDistanceCm, fresh, Modifier.align(Alignment.CenterEnd))
        Text("车头 ↑", color = Hud.TextPrimary, fontSize = 10.sp, modifier = Modifier.align(Alignment.Center))
    }
}

@Composable
private fun DistanceValue(label: String, distance: Int?, fresh: Boolean, modifier: Modifier) {
    val value = if (fresh) distance else null
    val color = distanceColor(value)
    Column(modifier.width(72.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Text(label, color = Hud.TextMuted, fontSize = 10.sp)
        Text(
            value?.let { "$it cm" } ?: "---",
            color = color, fontSize = 15.sp, fontWeight = FontWeight.Medium, fontFamily = FontFamily.Monospace
        )
    }
}

private fun distanceColor(distance: Int?): Color = when {
    distance == null -> Hud.Danger
    distance < 40 -> Hud.Danger
    distance <= 90 -> Hud.Warning
    else -> Hud.Success
}

@Composable
private fun RadarLegend(text: String, color: Color) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(6.dp).clip(CircleShape).background(color))
        Spacer(Modifier.width(4.dp))
        Text(text, color = Hud.TextMuted, fontSize = 9.sp)
    }
}

@Composable
private fun ColumnScope.DrivePanel(throttle: Float, steering: Float, command: Pair<Char, String>) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        SectionLabel("实体摇杆 · 8方向蓝牙控制")
        Spacer(Modifier.weight(1f))
        Text("100ms 连发", color = Hud.TextMuted, fontSize = 10.sp)
    }
    Spacer(Modifier.height(4.dp))
    Box(Modifier.fillMaxWidth().weight(1f), contentAlignment = Alignment.Center) {
        SmartControlPad(steering, throttle)
    }
    Column(
        Modifier.fillMaxWidth().background(Color(0xFF0E1E2A), RoundedCornerShape(7.dp)).padding(vertical = 6.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text("当前动作", color = Hud.TextMuted, fontSize = 10.sp)
        Text(command.second, color = Hud.Accent, fontSize = 14.sp, fontWeight = FontWeight.Medium)
        Text("TX '${command.first}' (0x${"%02X".format(command.first.code)})", color = Hud.TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace)
    }
    Spacer(Modifier.height(6.dp))
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
        Text("油门 ${(throttle * 100).toInt()}%", color = if (abs(throttle) > 0.25f) Hud.Success else Hud.TextSecondary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
        Text("转向 ${(steering * 100).toInt()}%", color = if (abs(steering) > 0.25f) Hud.Accent else Hud.TextSecondary, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
    }
    Text("L2 减速 0x6A · R2 加速 0x6B", color = Hud.TextMuted, fontSize = 9.sp, modifier = Modifier.align(Alignment.CenterHorizontally).padding(top = 3.dp))
}

@Composable
private fun SmartControlPad(steering: Float, throttle: Float) {
    Canvas(Modifier.fillMaxHeight().aspectRatio(1f)) {
        val radius = min(size.width, size.height) / 2f
        val center = Offset(size.width / 2f, size.height / 2f)
        drawCircle(Hud.PanelDeep, radius, center)
        drawCircle(Hud.Border, radius, center, style = Stroke(1.5f))
        drawLine(Hud.BorderSoft, Offset(center.x, center.y - radius), Offset(center.x, center.y + radius), 1f)
        drawLine(Hud.BorderSoft, Offset(center.x - radius, center.y), Offset(center.x + radius, center.y), 1f)
        val knob = Offset(
            center.x + steering.coerceIn(-1f, 1f) * radius * 0.82f,
            center.y - throttle.coerceIn(-1f, 1f) * radius * 0.82f
        )
        drawLine(Hud.AccentDim, center, knob, 2f)
        drawCircle(Hud.Accent, radius * 0.15f, knob)
    }
}

@Composable
private fun ColumnScope.ChassisPanel(fresh: Boolean, powerFresh: Boolean) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        SectionLabel("小车状态与底盘遥测")
        Spacer(Modifier.weight(1f))
        Text(
            if (SmartCarTelemetry.chassisIntervalMs >= 0) "${SmartCarTelemetry.chassisIntervalMs}ms" else "--ms",
            color = Hud.TextMuted, fontSize = 10.sp, fontFamily = FontFamily.Monospace
        )
    }
    Spacer(Modifier.height(7.dp))
    RuntimeStatusMetric(fresh, powerFresh)
    Spacer(Modifier.height(7.dp))
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        SmartMetric("左前电机", valueOrDash(SmartCarTelemetry.leftFrontSpeed, fresh), Hud.Success, Modifier.weight(1f))
        SmartMetric("右前电机", valueOrDash(SmartCarTelemetry.rightFrontSpeed, fresh), Hud.Success, Modifier.weight(1f))
    }
    Spacer(Modifier.height(7.dp))
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(7.dp)) {
        SmartMetric("左后电机", valueOrDash(SmartCarTelemetry.leftRearSpeed, fresh), Hud.Success, Modifier.weight(1f))
        SmartMetric("右后电机", valueOrDash(SmartCarTelemetry.rightRearSpeed, fresh), Hud.Success, Modifier.weight(1f))
    }
    Spacer(Modifier.height(7.dp))
    SmartMetric("陀螺仪角度", if (fresh) "${SmartCarTelemetry.gyroscopeAngle}°" else "---", Hud.Warning)
    Spacer(Modifier.weight(1f))
    Text(
        if (fresh) "底盘帧连续 · 0x55 0x52" else "底盘遥测超时 · 数值已失效",
        color = if (fresh) Hud.TextMuted else Hud.Danger, fontSize = 10.sp, modifier = Modifier.align(Alignment.CenterHorizontally)
    )
}

@Composable
private fun RuntimeStatusMetric(chassisFresh: Boolean, powerFresh: Boolean) {
    Column(
        Modifier.fillMaxWidth().background(Hud.PanelDeep, RoundedCornerShape(8.dp)).padding(9.dp)
    ) {
        val voltage = SmartCarTelemetry.powerVoltageDeciVolt?.let {
            String.format(Locale.US, "%.1fV", it / 10f)
        } ?: "---"
        val percent = SmartCarTelemetry.powerPercent?.let { "$it%" } ?: "---"
        Row(
            Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp)
        ) {
            Text(
                "状态 ${if (chassisFresh) SmartCarTelemetry.carStatusText() else "无数据"}",
                color = if (chassisFresh) Hud.Accent else Hud.Danger,
                fontSize = 11.sp,
                fontWeight = FontWeight.Medium,
                fontFamily = FontFamily.Monospace,
                maxLines = 1,
                modifier = Modifier.weight(1.2f)
            )
            Text(
                "电源 $voltage",
                color = if (powerFresh) Hud.Warning else Hud.TextMuted,
                fontSize = 10.sp,
                fontFamily = FontFamily.Monospace,
                maxLines = 1,
                modifier = Modifier.weight(1f)
            )
            Text(
                "电量 $percent",
                color = if (powerFresh) Hud.Success else Hud.TextMuted,
                fontSize = 10.sp,
                fontFamily = FontFamily.Monospace,
                maxLines = 1,
                modifier = Modifier.weight(0.9f)
            )
        }
        Text(if (powerFresh) "电源帧 · 0x55 0x53" else "电源帧无数据",
            color = if (powerFresh) Hud.TextMuted else Hud.Danger, fontSize = 9.sp,
            modifier = Modifier.align(Alignment.CenterHorizontally).padding(top = 3.dp))
    }
}

private fun valueOrDash(value: Int, fresh: Boolean): String = if (fresh) value.toString() else "---"

@Composable
private fun SmartMetric(label: String, value: String, color: Color, modifier: Modifier = Modifier) {
    Column(modifier.fillMaxWidth().background(Hud.PanelDeep, RoundedCornerShape(8.dp)).padding(9.dp)) {
        Text(label, color = Hud.TextMuted, fontSize = 10.sp)
        Text(value, color = color, fontSize = 16.sp, fontWeight = FontWeight.Medium, fontFamily = FontFamily.Monospace)
    }
}

@Composable
private fun SmartPanel(modifier: Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier.fillMaxHeight().background(Hud.Panel, RoundedCornerShape(10.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(10.dp)).padding(10.dp),
        content = content
    )
}

@Composable
private fun SectionLabel(text: String) = Text(text, color = Hud.TextSecondary, fontSize = 12.sp, fontWeight = FontWeight.Medium)
