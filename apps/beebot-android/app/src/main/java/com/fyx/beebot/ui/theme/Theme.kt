package com.fyx.beebot.ui.theme

import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

/** 科技深色 HUD 调色板 —— 机器人控制器质感。 */
object Hud {
    val Background = Color(0xFF0A0E14)   // 页面深空底色
    val Panel = Color(0xFF121A26)        // 卡片面板
    val PanelDeep = Color(0xFF0D131C)    // 摇杆内圈/日志底
    val Border = Color(0xFF223247)       // 描边
    val BorderSoft = Color(0xFF17202E)

    val Accent = Color(0xFF22D3EE)       // 主青色荧光高亮
    val AccentDim = Color(0xFF0E7490)
    val Success = Color(0xFF34D399)      // 连接/激活绿
    val Warning = Color(0xFFFBBF24)      // 扳机/告警琥珀
    val Danger = Color(0xFFF87171)

    val TextPrimary = Color(0xFFE6EEF8)
    val TextSecondary = Color(0xFF8296B0)
    val TextMuted = Color(0xFF556681)
}

private val BeeDarkColors = darkColorScheme(
    primary = Hud.Accent,
    onPrimary = Color(0xFF04212A),
    secondary = Hud.Success,
    background = Hud.Background,
    onBackground = Hud.TextPrimary,
    surface = Hud.Panel,
    onSurface = Hud.TextPrimary,
    error = Hud.Danger
)

@Composable
fun BeeBotTheme(content: @Composable () -> Unit) {
    // 本 App 固定深色 HUD 风格
    @Suppress("UNUSED_EXPRESSION") isSystemInDarkTheme()
    MaterialTheme(colorScheme = BeeDarkColors, content = content)
}
