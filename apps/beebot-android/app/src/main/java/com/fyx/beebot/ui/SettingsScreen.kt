package com.fyx.beebot.ui

import android.content.Context
import android.os.Build
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Bluetooth
import androidx.compose.material.icons.filled.Cable
import androidx.compose.material.icons.filled.Logout
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material3.Icon
import androidx.compose.material3.Slider
import androidx.compose.material3.SliderDefaults
import androidx.compose.material3.Switch
import androidx.compose.material3.SwitchDefaults
import androidx.compose.material3.Text
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.ui.theme.Hud
import com.fyx.beebot.util.Haptics

/**
 * 设置页内容（不含顶栏，顶栏与右滑关闭由 [ToolsScreen] 的 Tab 框架统一提供）。
 */
@Composable
fun SettingsContent(onLogout: () -> Unit) {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences(Auth.PREFS, Context.MODE_PRIVATE) }

    var deadzone by remember { mutableStateOf(prefs.getInt("deadzone_pct", 5).toFloat()) }
    var keepOn by remember { mutableStateOf(prefs.getBoolean("keep_screen_on", true)) }
    var haptic by remember { mutableStateOf(Haptics.enabled) }

    Row(Modifier.fillMaxSize(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        Column(Modifier.weight(1.3f).fillMaxHeight().verticalScroll(rememberScrollState())) {
            SettingSection("显示与交互") {
                SwitchRow("屏幕常亮", "监视时保持屏幕不熄灭", keepOn) {
                    keepOn = it; prefs.edit().putBoolean("keep_screen_on", it).apply()
                }
                Spacer(Modifier.height(7.dp))
                SwitchRow("按键震动反馈", "操作按钮时给予触觉反馈", haptic) {
                    haptic = it; Haptics.setEnabled(it); if (it) Haptics.click()
                }
                Spacer(Modifier.height(6.dp))
                Text("摇杆死区  ${deadzone.toInt()}%", color = Hud.TextSecondary, fontSize = 13.sp)
                Text("小于该幅度的摇杆抖动将被忽略", color = Hud.TextMuted, fontSize = 11.sp)
                Slider(
                    value = deadzone, onValueChange = { deadzone = it },
                    onValueChangeFinished = { prefs.edit().putInt("deadzone_pct", deadzone.toInt()).apply() },
                    valueRange = 0f..30f,
                    colors = SliderDefaults.colors(
                        thumbColor = Hud.Accent, activeTrackColor = Hud.Accent, inactiveTrackColor = Hud.Border
                    )
                )
            }
            Spacer(Modifier.height(10.dp))
            SettingSection("机器人连接（即将开放）") {
                Text("后续可选择遥控通道，把摇杆/按键指令下发给机器狗、机器车", color = Hud.TextMuted, fontSize = 11.sp)
                Spacer(Modifier.height(7.dp))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    TransportChip(Icons.Filled.Bluetooth, "蓝牙 SPP", Modifier.weight(1f))
                    TransportChip(Icons.Filled.Wifi, "WiFi UDP", Modifier.weight(1f))
                    TransportChip(Icons.Filled.Cable, "串口 TTL", Modifier.weight(1f))
                }
            }
        }

        Column(Modifier.weight(0.9f).fillMaxHeight().verticalScroll(rememberScrollState())) {
            SettingSection("关于小蜜蜂智控系统") {
                InfoRow("软件版本", "v2.6.3")
                InfoRow("登录账号", "FYX")
                InfoRow("适配设备", "Retroid Pocket 4")
                InfoRow("系统版本", "Android ${Build.VERSION.RELEASE}")
                InfoRow("开发者", "小蜜蜂机器人")
            }
            Spacer(Modifier.height(10.dp))
            Box(
                modifier = Modifier.fillMaxWidth().height(46.dp)
                    .clip(RoundedCornerShape(10.dp)).background(Color(0xFF2A1416))
                    .border(1.dp, Hud.Danger, RoundedCornerShape(10.dp))
                    .clickable { onLogout() },
                contentAlignment = Alignment.Center
            ) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Filled.Logout, null, tint = Hud.Danger, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(8.dp))
                    Text("退出登录", color = Hud.Danger, fontSize = 15.sp, fontWeight = FontWeight.Medium)
                }
            }
        }
    }
}

@Composable
private fun SettingSection(title: String, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .background(Hud.Panel, RoundedCornerShape(12.dp))
            .border(1.dp, Hud.Border, RoundedCornerShape(12.dp))
            .padding(12.dp)
    ) {
        Text(title, color = Hud.Accent, fontSize = 13.sp, fontWeight = FontWeight.Medium)
        Spacer(Modifier.height(7.dp))
        content()
    }
}

@Composable
private fun SwitchRow(title: String, sub: String, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, color = Hud.TextPrimary, fontSize = 14.sp)
            Text(sub, color = Hud.TextMuted, fontSize = 11.sp)
        }
        Switch(
            checked = checked, onCheckedChange = onChange,
            colors = SwitchDefaults.colors(
                checkedThumbColor = Color(0xFF042027), checkedTrackColor = Hud.Accent,
                uncheckedThumbColor = Hud.TextMuted, uncheckedTrackColor = Hud.PanelDeep
            )
        )
    }
}

@Composable
private fun TransportChip(icon: ImageVector, label: String, modifier: Modifier) {
    Column(
        modifier = modifier
            .background(Hud.PanelDeep, RoundedCornerShape(9.dp))
            .border(1.dp, Hud.BorderSoft, RoundedCornerShape(9.dp))
            .padding(vertical = 10.dp),
        horizontalAlignment = Alignment.CenterHorizontally
    ) {
        Icon(icon, null, tint = Hud.TextMuted, modifier = Modifier.size(20.dp))
        Spacer(Modifier.height(5.dp))
        Text(label, color = Hud.TextSecondary, fontSize = 11.sp)
    }
}

@Composable
private fun InfoRow(k: String, v: String) {
    Row(Modifier.fillMaxWidth().padding(vertical = 5.dp)) {
        Text(k, color = Hud.TextMuted, fontSize = 13.sp)
        Spacer(Modifier.weight(1f))
        Text(v, color = Hud.TextSecondary, fontSize = 13.sp, fontFamily = FontFamily.Monospace)
    }
}
