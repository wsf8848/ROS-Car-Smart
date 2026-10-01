package com.fyx.beebot.ui

import android.app.Application
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import com.fyx.beebot.bt.BluetoothRepository
import com.fyx.beebot.bt.SmartCarMode
import com.fyx.beebot.bt.SmartCarModeController
import com.fyx.beebot.bt.SmartCarTelemetry
import com.fyx.beebot.input.GamepadButton
import com.fyx.beebot.input.GamepadRepository
import com.fyx.beebot.mqtt.RemoteControlProtocol
import com.fyx.beebot.mqtt.RemoteVehicleRepository
import com.fyx.beebot.transport.VehicleControlAuthorization
import com.fyx.beebot.ui.theme.Hud
import com.fyx.beebot.util.Haptics
import com.fyx.beebot.util.VoicePrompts
import kotlinx.coroutines.delay
import java.util.Locale

private enum class DashboardTransport(val label: String, val prompt: String) {
    WIFI("WiFi模块", "WiFi远程通信模式"),
    BLUETOOTH("蓝牙", "蓝牙通信模式");
    fun next() = values()[(ordinal + 1) % values().size]
}

private enum class DashboardVehicleMode(val label: String, val prompt: String, val command: Int, val bluetoothMode: SmartCarMode) {
    MANUAL("手动", "手动控制模式", RemoteControlProtocol.MODE_MANUAL, SmartCarMode.BLUETOOTH),
    AVOIDANCE("避障", "遥控避障模式", RemoteControlProtocol.MODE_AVOIDANCE, SmartCarMode.AVOIDANCE),
    AUTONOMOUS("自主", "自主导航模式", RemoteControlProtocol.MODE_AUTONOMOUS, SmartCarMode.AUTONOMOUS);
    fun next() = values()[(ordinal + 1) % values().size]
}

@Composable
fun MainScreen(onLogout: () -> Unit) {
    val context = LocalContext.current
    var showTools by rememberSaveable { mutableStateOf(false) }
    var transportName by rememberSaveable { mutableStateOf(DashboardTransport.WIFI.name) }
    var toolsInitialTab by rememberSaveable { mutableStateOf(0) }
    var toolsSession by rememberSaveable { mutableStateOf(0) }
    var hornEnabled by rememberSaveable { mutableStateOf(false) }
    var lightMode by rememberSaveable { mutableStateOf(0) }
    var notice by remember { mutableStateOf("") }
    var now by remember { mutableStateOf(System.currentTimeMillis()) }
    val transport = DashboardTransport.valueOf(transportName)
    val vehicleMode = when (SmartCarModeController.mode) {
        SmartCarMode.BLUETOOTH -> DashboardVehicleMode.MANUAL
        SmartCarMode.AVOIDANCE -> DashboardVehicleMode.AVOIDANCE
        SmartCarMode.AUTONOMOUS -> DashboardVehicleMode.AUTONOMOUS
    }
    val controlLocked = VehicleControlAuthorization.locked
    val vehicleOnline = RemoteVehicleRepository.isVehicleOnline(now)

    LaunchedEffect(Unit) {
        VoicePrompts.speak("欢迎使用小蜜蜂智控系统，开始您的驾驶吧")
        while (true) { delay(1_000); now = System.currentTimeMillis() }
    }

    LaunchedEffect(showTools, transport, vehicleMode, controlLocked) {
        var linkWasOnline = false
        var driveWasActive = false
        var stopPressed = false
        var lastSpeedStepAt = 0L
        var lastLatencyProbeAt = 0L
        while (true) {
            val online = RemoteVehicleRepository.isVehicleOnline()
            val gamepad = GamepadRepository.state
            if (!online) {
                linkWasOnline = false
                driveWasActive = false
            } else if (showTools || transport == DashboardTransport.BLUETOOTH || controlLocked) {
                if (driveWasActive) RemoteVehicleRepository.publishStop()
                driveWasActive = false
            } else {
                if (!linkWasOnline) {
                    RemoteVehicleRepository.publishStop()
                    RemoteVehicleRepository.publishControl(vehicleMode.command)
                    lastLatencyProbeAt = android.os.SystemClock.elapsedRealtime()
                }
                if (vehicleMode == DashboardVehicleMode.AUTONOMOUS) {
                    linkWasOnline = true
                    delay(50L)
                    continue
                }
                val emergencyPressed = gamepad.isPressed(GamepadButton.A)
                if (emergencyPressed && !stopPressed) {
                    RemoteVehicleRepository.publishStop()
                    driveWasActive = false
                } else if (!emergencyPressed) {
                    val command = RemoteControlProtocol.driveCommand(gamepad.leftY, gamepad.rightX)
                    val driveActive = command != RemoteControlProtocol.STOP
                    if (driveActive) {
                        RemoteVehicleRepository.publishControl(command)
                    } else if (driveWasActive) {
                        RemoteVehicleRepository.publishStop()
                    }
                    driveWasActive = driveActive

                    val elapsed = android.os.SystemClock.elapsedRealtime()
                    if (!driveActive && elapsed - lastLatencyProbeAt >= 1_000L) {
                        RemoteVehicleRepository.publishStop(repeat = 1)
                        lastLatencyProbeAt = elapsed
                    }
                    if (elapsed - lastSpeedStepAt >= 250L) {
                        val speedCommand = when {
                            gamepad.rightTrigger > 0.5f || gamepad.isPressed(GamepadButton.R2) -> RemoteControlProtocol.SPEED_UP
                            gamepad.leftTrigger > 0.5f || gamepad.isPressed(GamepadButton.L2) -> RemoteControlProtocol.SPEED_DOWN
                            else -> null
                        }
                        speedCommand?.let {
                            RemoteVehicleRepository.publishControl(it)
                            lastSpeedStepAt = elapsed
                            Haptics.tick()
                        }
                    }
                }
                stopPressed = emergencyPressed
                linkWasOnline = true
            }
            delay(50L)
        }
    }

    LaunchedEffect(hornEnabled, transport) {
        while (hornEnabled) {
            if (transport == DashboardTransport.WIFI) {
                RemoteVehicleRepository.publishControl(RemoteControlProtocol.HORN_ON)
            } else {
                BluetoothRepository.sendRaw(byteArrayOf(RemoteControlProtocol.HORN_ON.toByte()))
            }
            delay(500L)
        }
    }

    DisposableEffect(Unit) {
        onDispose {
            RemoteVehicleRepository.publishStop()
            if (hornEnabled) {
                RemoteVehicleRepository.publishControl(RemoteControlProtocol.HORN_OFF)
                BluetoothRepository.sendRaw(byteArrayOf(RemoteControlProtocol.HORN_OFF.toByte()))
            }
        }
    }

    Box(Modifier.fillMaxSize().background(Hud.Background)) {
        DashboardCameraBackground()
        Box(Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color(0xB8000000), Color.Transparent, Color(0x7A000000)))))
        DashboardPreviewStatus(EzvizPreviewPlayer.status, EzvizPreviewPlayer.errorCode)

        DashboardHeader(vehicleOnline = vehicleOnline, now = now)
        DashboardToolsFooter(onOpenTools = {
            RemoteVehicleRepository.publishStop()
            Haptics.tick()
            toolsInitialTab = 0
            toolsSession++
            showTools = true
        })
        TelemetryStrip(transport = transport, vehicleMode = vehicleMode)
        CenterReticle()

        Column(
            Modifier.align(Alignment.CenterEnd).padding(end = 16.dp),
            verticalArrangement = Arrangement.spacedBy(7.dp)
        ) {
            DashboardAction(Icons.Filled.LockOpen, "解锁", if (!controlLocked) Hud.Success else Hud.TextMuted,
                if (!controlLocked) "已允许控制" else null) {
                VehicleControlAuthorization.unlock()
                val sent = if (transport == DashboardTransport.WIFI) {
                    RemoteVehicleRepository.publishControl(vehicleMode.command)
                } else {
                    SmartCarModeController.selectMode(vehicleMode.bluetoothMode)
                    BluetoothRepository.connected
                }
                Haptics.click()
                notice = if (sent) "遥控器指令已解锁" else "已解锁，等待当前通信链路上线"
                VoicePrompts.speak("车辆已解锁")
            }
            DashboardAction(Icons.Filled.Lock, "上锁", if (controlLocked) Hud.Danger else Hud.TextMuted,
                if (controlLocked) "已禁止控制" else null) {
                val sent = if (transport == DashboardTransport.WIFI) RemoteVehicleRepository.publishStop()
                else { SmartCarModeController.emergencyStop(); BluetoothRepository.connected }
                VehicleControlAuthorization.lock()
                Haptics.heavy()
                notice = if (sent) "已上锁并停车，遥控指令禁止下发" else "已上锁，车辆离线保护停车"
                VoicePrompts.speak("车辆已上锁")
            }
            DashboardAction(Icons.Filled.VolumeUp, "鸣笛", if (hornEnabled) Hud.Warning else Hud.TextMuted,
                if (hornEnabled) "滴滴中" else "已关闭") {
                val targetEnabled = !hornEnabled
                val command = if (targetEnabled) RemoteControlProtocol.HORN_ON else RemoteControlProtocol.HORN_OFF
                val sent = if (transport == DashboardTransport.WIFI) {
                    RemoteVehicleRepository.publishControl(command)
                } else {
                    BluetoothRepository.sendRaw(byteArrayOf(command.toByte()))
                    BluetoothRepository.connected
                }
                hornEnabled = targetEnabled
                Haptics.click()
                notice = if (sent) if (targetEnabled) "连续鸣笛已开启" else "鸣笛已关闭" else "鸣笛状态已切换，等待车辆上线"
                VoicePrompts.speak(if (targetEnabled) "鸣笛已开启" else "鸣笛已关闭")
            }
            DashboardAction(Icons.Filled.LightMode, "车灯", if (lightMode != 0) Hud.Warning else Hud.TextMuted,
                when (lightMode) { 1 -> "流水"; 2 -> "爆闪"; else -> "已关闭" }) {
                if (controlLocked) { Haptics.heavy(); notice = "车辆已上锁，请先解锁"; VoicePrompts.speak("请先解锁车辆")
                } else {
                    lightMode = (lightMode + 1) % 3
                    val command = when (lightMode) { 1 -> RemoteControlProtocol.LIGHT_WATER; 2 -> RemoteControlProtocol.LIGHT_FLASH; else -> RemoteControlProtocol.LIGHT_OFF }
                    val sent = if (transport == DashboardTransport.WIFI) RemoteVehicleRepository.publishControl(command) else { BluetoothRepository.sendRaw(byteArrayOf(command.toByte())); BluetoothRepository.connected }
                    Haptics.click(); notice = if (sent) "车灯${when (lightMode) { 1 -> "流水"; 2 -> "爆闪"; else -> "已关闭" }}" else "车灯状态已切换，等待当前通信链路上线"
                }
            }
            DashboardAction(Icons.Filled.StopCircle, "停车", Hud.Danger) {
                val sent = if (transport == DashboardTransport.WIFI) RemoteVehicleRepository.publishStop()
                else { SmartCarModeController.emergencyStop(); BluetoothRepository.connected }
                Haptics.heavy(); notice = if (sent) "停车指令已下发" else "车辆离线，车端失联保护停车"; VoicePrompts.speak("触发停车")
            }
            DashboardAction(Icons.Filled.Wifi, "通信切换", Hud.Accent, "当前 ${transport.label}") {
                if (hornEnabled) {
                    if (transport == DashboardTransport.WIFI) RemoteVehicleRepository.publishControl(RemoteControlProtocol.HORN_OFF)
                    else BluetoothRepository.sendRaw(byteArrayOf(RemoteControlProtocol.HORN_OFF.toByte()))
                }
                if (transport == DashboardTransport.WIFI) RemoteVehicleRepository.publishStop()
                else SmartCarModeController.emergencyStop()
                val target = transport.next()
                transportName = target.name
                if (target == DashboardTransport.BLUETOOTH) {
                    SmartCarModeController.selectMode(vehicleMode.bluetoothMode)
                    val reconnecting = BluetoothRepository.connectLastDevice(context)
                    toolsInitialTab = 2
                    toolsSession++
                    showTools = true
                    notice = if (reconnecting) "正在进入蓝牙控制" else BluetoothRepository.statusMsg
                } else {
                    val sent = !controlLocked && RemoteVehicleRepository.publishControl(vehicleMode.command)
                    notice = if (sent) "已切换${target.label}" else "已切换${target.label}，等待车辆上线或解锁"
                }
                Haptics.click()
                VoicePrompts.speak(target.prompt)
            }
            DashboardAction(Icons.Filled.Sync, "车辆模式", Hud.Warning, "当前 ${vehicleMode.label}") {
                if (controlLocked) {
                    Haptics.heavy(); notice = "车辆已上锁，请先解锁"; VoicePrompts.speak("请先解锁车辆")
                    return@DashboardAction
                }
                val target = vehicleMode.next()
                val sent = if (transport == DashboardTransport.WIFI) {
                    RemoteVehicleRepository.publishStop()
                    RemoteVehicleRepository.publishControl(target.command)
                } else {
                    SmartCarModeController.selectMode(target.bluetoothMode)
                    BluetoothRepository.connected
                }
                if (transport == DashboardTransport.WIFI) SmartCarModeController.rememberMode(target.bluetoothMode)
                Haptics.click()
                notice = if (sent) "已下发${target.label}模式" else "已切换${target.label}，等待当前通信链路上线"
                VoicePrompts.speak(target.prompt)
            }
        }

        AnimatedVisibility(
            visible = notice.isNotEmpty(), modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 42.dp)
        ) {
            Text(notice, color = Hud.TextPrimary, fontSize = 12.sp,
                modifier = Modifier.background(Color(0xE60A1720), RoundedCornerShape(8.dp))
                    .border(1.dp, Hud.Accent, RoundedCornerShape(8.dp)).padding(horizontal = 14.dp, vertical = 8.dp))
        }
        LaunchedEffect(notice) { if (notice.isNotEmpty()) { delay(1_800); notice = "" } }

        AnimatedVisibility(visible = showTools, enter = slideInHorizontally { it }, exit = slideOutHorizontally { it }) {
            key(toolsSession) {
                ToolsScreen(onClose = { showTools = false }, onLogout = { showTools = false; onLogout() }, initialTab = toolsInitialTab)
            }
        }
    }
}

@Composable
private fun DashboardCameraBackground() {
    val context = LocalContext.current
    val lifecycleOwner = LocalLifecycleOwner.current
    val application = context.applicationContext as Application

    DisposableEffect(lifecycleOwner) {
        val observer = LifecycleEventObserver { _, event ->
            when (event) {
                Lifecycle.Event.ON_START -> EzvizPreviewPlayer.resume()
                Lifecycle.Event.ON_STOP -> EzvizPreviewPlayer.pause()
                else -> Unit
            }
        }
        lifecycleOwner.lifecycle.addObserver(observer)
        onDispose {
            lifecycleOwner.lifecycle.removeObserver(observer)
            EzvizPreviewPlayer.release()
        }
    }

    AndroidView(
        modifier = Modifier.fillMaxSize().background(Color.Black),
        factory = { ctx ->
            SurfaceView(ctx).apply {
                holder.addCallback(object : SurfaceHolder.Callback {
                    override fun surfaceCreated(holder: SurfaceHolder) {
                        EzvizPreviewPlayer.attach(application, holder)
                    }

                    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
                        EzvizPreviewPlayer.attach(application, holder)
                    }

                    override fun surfaceDestroyed(holder: SurfaceHolder) {
                        EzvizPreviewPlayer.onSurfaceDestroyed(holder)
                    }
                })
            }
        },
        update = { }
    )
}

@Composable
private fun BoxScope.DashboardPreviewStatus(status: EzvizPreviewStatus, errorCode: Int?) {
    val color = when (status) {
        EzvizPreviewStatus.PLAYING -> Hud.Success
        EzvizPreviewStatus.CONNECTING -> Hud.Warning
        EzvizPreviewStatus.CONFIGURATION_REQUIRED -> Hud.TextMuted
        EzvizPreviewStatus.ERROR -> Hud.Danger
    }
    val detail = when (status) {
        EzvizPreviewStatus.PLAYING -> "EZOpenSDK 5.32"
        EzvizPreviewStatus.CONNECTING -> "正在建立实时预览"
        EzvizPreviewStatus.CONFIGURATION_REQUIRED -> "等待开放平台配置"
        EzvizPreviewStatus.ERROR -> errorCode?.let { "错误码 $it" } ?: "请检查网络与授权"
    }
    Row(
        Modifier.align(Alignment.TopStart).padding(start = 16.dp, top = 82.dp)
            .background(Color(0xD90A1118), RoundedCornerShape(8.dp))
            .border(1.dp, color, RoundedCornerShape(8.dp)).padding(horizontal = 10.dp, vertical = 7.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.size(7.dp).clip(CircleShape).background(color))
        Spacer(Modifier.width(7.dp))
        Column {
            Text(status.label, color = color, fontSize = 10.sp)
            Text(detail, color = Hud.TextMuted, fontSize = 8.sp, fontFamily = FontFamily.Monospace)
        }
    }
}

@Composable
private fun CameraFallback() {
    Box(Modifier.fillMaxSize().background(Brush.verticalGradient(listOf(Color(0xFF19343C), Color(0xFF0B141A))))) {
        Icon(Icons.Filled.Videocam, null, tint = Hud.TextMuted, modifier = Modifier.size(68.dp).align(Alignment.Center))
    }
}

@Composable
private fun DashboardHeader(vehicleOnline: Boolean, now: Long) {
    val latencyMs = RemoteVehicleRepository.freshLatencyMs(now)
    Row(Modifier.fillMaxWidth().padding(16.dp), verticalAlignment = Alignment.CenterVertically) {
        Row(
            Modifier.background(Color(0xD90A1118), RoundedCornerShape(10.dp)).border(1.dp, Hud.Border, RoundedCornerShape(10.dp))
                .padding(horizontal = 12.dp, vertical = 8.dp), verticalAlignment = Alignment.CenterVertically
        ) {
            Icon(Icons.Filled.DirectionsCar, null, tint = Hud.Warning, modifier = Modifier.size(22.dp))
            Spacer(Modifier.width(9.dp))
            Column { Text("小蜜蜂智控系统", color = Hud.TextPrimary, fontSize = 14.sp, fontWeight = FontWeight.Medium)
                Text("车辆 001 · LIVE", color = Hud.TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace) }
            Spacer(Modifier.width(13.dp))
            Box(Modifier.width(1.dp).height(27.dp).background(Hud.Border))
            Spacer(Modifier.width(11.dp))
            Box(Modifier.size(8.dp).clip(CircleShape).background(if (vehicleOnline) Hud.Success else Hud.Danger))
            Spacer(Modifier.width(7.dp))
            Column {
                Text(if (vehicleOnline) "车辆在线" else "车辆离线", color = if (vehicleOnline) Hud.Success else Hud.Danger, fontSize = 11.sp)
                Text(
                    if (RemoteVehicleRepository.brokerConnected) "远程 · ${latencyMs?.let { "$it ms" } ?: "刷新中"}"
                    else RemoteVehicleRepository.connectionState.label,
                    color = Hud.TextMuted, fontSize = 9.sp, fontFamily = FontFamily.Monospace
                )
            }
        }
        Spacer(Modifier.width(8.dp))
        DashboardLinkStatus(vehicleOnline)
    }
}

@Composable
private fun TelemetryStrip(transport: DashboardTransport, vehicleMode: DashboardVehicleMode) {
    val telemetryRevision = SmartCarTelemetry.parsedPacketCount
    val chassisFresh = telemetryRevision > 0 && SmartCarTelemetry.chassisAgeMs().let { it in 0..1_500 }
    val radarFresh = telemetryRevision > 0 && SmartCarTelemetry.radarAgeMs().let { it in 0..1_500 }
    val powerFresh = telemetryRevision > 0 && SmartCarTelemetry.powerAgeMs().let { it in 0..1_500 }
    Row(
        Modifier.fillMaxWidth().padding(top = 76.dp, start = 84.dp, end = 104.dp),
        horizontalArrangement = Arrangement.Center
    ) {
        TelemetryPill("电量", if (powerFresh) "${SmartCarTelemetry.powerPercent ?: 0}%  ${SmartCarTelemetry.powerVoltageDeciVolt?.let { String.format(Locale.US, "%.1fV", it / 10f) } ?: "--"}" else "--")
        Spacer(Modifier.width(6.dp))
        TelemetryPill("四轮脉冲", if (chassisFresh) "${SmartCarTelemetry.leftFrontSpeed}  ${SmartCarTelemetry.rightFrontSpeed}  ${SmartCarTelemetry.leftRearSpeed}  ${SmartCarTelemetry.rightRearSpeed}" else "--  --  --  --")
        Spacer(Modifier.width(6.dp))
        TelemetryPill("姿态 P/R/Y", "${RemoteVehicleRepository.pitch?.format1() ?: "--"}  ${RemoteVehicleRepository.roll?.format1() ?: "--"}  ${RemoteVehicleRepository.yaw?.format1() ?: "--"}")
        Spacer(Modifier.width(6.dp))
        TelemetryPill("雷达 cm", if (radarFresh) "${SmartCarTelemetry.frontDistanceCm ?: "--"}  ${SmartCarTelemetry.rearDistanceCm ?: "--"}  ${SmartCarTelemetry.leftDistanceCm ?: "--"}  ${SmartCarTelemetry.rightDistanceCm ?: "--"}" else "--  --  --  --")
        Spacer(Modifier.width(6.dp))
        TelemetryPill("通信 / 模式", "${transport.label} · ${vehicleMode.label}", active = true)
    }
}

private fun Float.format1() = String.format(Locale.US, "%+.1f°", this)

@Composable
private fun TelemetryPill(label: String, value: String, active: Boolean = false) {
    Column(
        Modifier.background(Color(0xD90A1118), RoundedCornerShape(22.dp))
            .border(1.dp, if (active) Hud.Accent else Hud.Border, RoundedCornerShape(22.dp))
            .padding(horizontal = 11.dp, vertical = 6.dp), horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Text(label, color = Hud.TextMuted, fontSize = 8.sp)
        Text(value, color = if (active) Hud.Accent else Hud.TextPrimary, fontSize = 10.sp, fontFamily = FontFamily.Monospace, maxLines = 1)
    }
}

@Composable
private fun DashboardAction(icon: androidx.compose.ui.graphics.vector.ImageVector, label: String, color: Color, sub: String? = null, onClick: () -> Unit) {
    Column(
        Modifier.width(78.dp).heightIn(min = if (sub == null) 51.dp else 61.dp).clip(RoundedCornerShape(10.dp))
            .background(Color(0xE60A1118)).border(1.dp, color.copy(alpha = 0.7f), RoundedCornerShape(10.dp))
            .clickable(onClick = onClick).padding(vertical = 6.dp), horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center
    ) {
        Icon(icon, null, tint = color, modifier = Modifier.size(18.dp))
        Text(label, color = Hud.TextPrimary, fontSize = 10.sp)
        if (sub != null) Text(sub, color = color, fontSize = 8.sp)
    }
}

@Composable
private fun BoxScope.CenterReticle() {
    Box(Modifier.size(64.dp).align(Alignment.Center), contentAlignment = Alignment.Center) {
        Box(Modifier.size(38.dp).border(1.dp, Color(0x99E6EEF8), CircleShape))
        Box(Modifier.size(5.dp).clip(CircleShape).background(Hud.Accent))
    }
}

@Composable
private fun DashboardLinkStatus(vehicleOnline: Boolean) {
    Row(
        Modifier.background(Color(0xD90A1118), RoundedCornerShape(8.dp)).border(1.dp, Hud.Border, RoundedCornerShape(8.dp))
            .padding(horizontal = 11.dp, vertical = 7.dp), verticalAlignment = Alignment.CenterVertically
    ) {
        Box(Modifier.size(7.dp).clip(CircleShape).background(if (vehicleOnline) Hud.Success else Hud.Danger))
        Spacer(Modifier.width(7.dp))
        Column { Text(if (vehicleOnline) "遥控链路正常" else "等待车辆上线", color = Hud.TextPrimary, fontSize = 10.sp)
            Text(if (RemoteVehicleRepository.brokerConnected) {
                    val command = RemoteVehicleRepository.lastControlCommand?.let { "ESP32 0x%02X · ".format(it) } ?: ""
                    "${command}RX ${RemoteVehicleRepository.telemetryMessageCount}/${SmartCarTelemetry.parsedPacketCount} · UART ${RemoteVehicleRepository.uartFrameCount}"
                } else RemoteVehicleRepository.connectionState.label,
                color = Hud.TextMuted, fontSize = 8.sp) }
    }
}

@Composable
private fun BoxScope.DashboardToolsFooter(onOpenTools: () -> Unit) {
    Box(
        Modifier.align(Alignment.BottomStart).padding(start = 16.dp, bottom = 14.dp)
            .size(42.dp).clip(RoundedCornerShape(9.dp)).background(Color(0xD90A1118))
            .border(1.dp, Hud.Border, RoundedCornerShape(9.dp)).clickable(onClick = onOpenTools),
        contentAlignment = Alignment.Center
    ) {
        Icon(Icons.Filled.MoreVert, "功能中心", tint = Hud.TextPrimary)
    }
}
