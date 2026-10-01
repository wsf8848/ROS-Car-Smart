package com.fyx.beebot.ui

import android.content.Context
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.DirectionsCar
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.Visibility
import androidx.compose.material.icons.filled.VisibilityOff
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.ui.theme.Hud

object Auth {
    const val ACCOUNT = "FYX"
    const val PASSWORD = "wsf680221"
    const val PREFS = "beebot_prefs"
    const val KEY_ACCOUNT = "login_account"
    const val KEY_PASSWORD = "login_password"
    const val KEY_AUTO = "auto_login"

    fun validate(account: String, password: String): Boolean =
        account.trim() == ACCOUNT && password == PASSWORD
}

@Composable
fun LoginScreen(onLoginSuccess: () -> Unit) {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences(Auth.PREFS, Context.MODE_PRIVATE) }
    var account by remember { mutableStateOf(prefs.getString(Auth.KEY_ACCOUNT, Auth.ACCOUNT) ?: Auth.ACCOUNT) }
    var password by remember { mutableStateOf("") }
    var autoLogin by remember { mutableStateOf(prefs.getBoolean(Auth.KEY_AUTO, false)) }
    var revealPassword by remember { mutableStateOf(false) }
    var error by remember { mutableStateOf("") }

    Row(
        Modifier.fillMaxSize().background(
            Brush.horizontalGradient(listOf(Hud.Background, Color(0xFF0B1720), Hud.Background))
        ).verticalScroll(rememberScrollState()).padding(horizontal = 34.dp, vertical = 18.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(36.dp)
    ) {
        VehicleWelcomeArt(Modifier.weight(1.15f).heightIn(min = 360.dp))
        Column(
            Modifier.widthIn(min = 330.dp, max = 400.dp).background(Hud.Panel, RoundedCornerShape(18.dp))
                .border(1.dp, Hud.Border, RoundedCornerShape(18.dp)).padding(horizontal = 26.dp, vertical = 22.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Text("小蜜蜂智控系统", color = Hud.TextPrimary, fontSize = 22.sp, fontWeight = FontWeight.Medium)
            Text("BEE INTELLIGENT CONTROL", color = Hud.Accent, fontSize = 10.sp, fontFamily = FontFamily.Monospace)
            Spacer(Modifier.height(18.dp))
            OutlinedTextField(
                value = account, onValueChange = { account = it; error = "" }, label = { Text("登录账号") },
                singleLine = true, leadingIcon = { Icon(Icons.Filled.Person, null) }, colors = hudFieldColorsV2(),
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(10.dp))
            OutlinedTextField(
                value = password, onValueChange = { password = it; error = "" }, label = { Text("登录密码") },
                singleLine = true,
                visualTransformation = if (revealPassword) VisualTransformation.None else PasswordVisualTransformation(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                leadingIcon = { Icon(Icons.Filled.Lock, null) },
                trailingIcon = { IconButton(onClick = { revealPassword = !revealPassword }) {
                    Icon(if (revealPassword) Icons.Filled.VisibilityOff else Icons.Filled.Visibility, "显示或隐藏密码")
                } }, colors = hudFieldColorsV2(), modifier = Modifier.fillMaxWidth()
            )
            Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                Checkbox(checked = autoLogin, onCheckedChange = { autoLogin = it },
                    colors = CheckboxDefaults.colors(checkedColor = Hud.Accent, uncheckedColor = Hud.TextMuted))
                Text("下次自动登录", color = Hud.TextSecondary, fontSize = 13.sp)
            }
            if (error.isNotEmpty()) Text(error, color = Hud.Danger, fontSize = 12.sp)
            Spacer(Modifier.height(10.dp))
            Box(
                Modifier.fillMaxWidth().height(50.dp).background(Hud.Accent, RoundedCornerShape(10.dp))
                    .clickable {
                        com.fyx.beebot.util.Haptics.click()
                        if (Auth.validate(account, password)) {
                            prefs.edit().putString(Auth.KEY_ACCOUNT, account.trim())
                                .putString(Auth.KEY_PASSWORD, password).putBoolean(Auth.KEY_AUTO, autoLogin).apply()
                            onLoginSuccess()
                        } else error = "账号或密码错误"
                    }, contentAlignment = Alignment.Center
            ) { Text("进入智控系统", color = Color(0xFF042027), fontSize = 16.sp, fontWeight = FontWeight.Bold) }
            Spacer(Modifier.height(10.dp))
            Text("安全登录 · v2.3.1", color = Hud.TextMuted, fontSize = 10.sp)
        }
    }
}

@Composable
private fun VehicleWelcomeArt(modifier: Modifier) {
    val transition = rememberInfiniteTransition()
    val carOffset by transition.animateFloat(-8f, 8f, infiniteRepeatable(tween(1800), RepeatMode.Reverse))
    val glow by transition.animateFloat(0.35f, 0.9f, infiniteRepeatable(tween(1400), RepeatMode.Reverse))
    Box(modifier, contentAlignment = Alignment.Center) {
        repeat(3) { index ->
            Box(Modifier.size((190 + index * 70).dp).alpha(glow / (index + 1)).border(1.dp, Hud.Accent, CircleShape))
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally, modifier = Modifier.offset(y = carOffset.dp)) {
            Box(
                Modifier.size(126.dp).clip(RoundedCornerShape(30.dp)).background(Color(0xFF0D2731))
                    .border(1.dp, Hud.Accent, RoundedCornerShape(30.dp)), contentAlignment = Alignment.Center
            ) { Icon(Icons.Filled.DirectionsCar, null, tint = Hud.Accent, modifier = Modifier.size(76.dp)) }
            Spacer(Modifier.height(18.dp))
            Text("公网连接 · 实时遥测 · 无界驾驶", color = Hud.TextPrimary, fontSize = 16.sp)
            Text("车辆 001 已准备连接", color = Hud.TextMuted, fontSize = 12.sp, fontFamily = FontFamily.Monospace)
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun hudFieldColorsV2() = OutlinedTextFieldDefaults.colors(
    focusedBorderColor = Hud.Accent, unfocusedBorderColor = Hud.Border,
    focusedLabelColor = Hud.Accent, unfocusedLabelColor = Hud.TextMuted,
    focusedTextColor = Hud.TextPrimary, unfocusedTextColor = Hud.TextPrimary,
    cursorColor = Hud.Accent, focusedLeadingIconColor = Hud.Accent, unfocusedLeadingIconColor = Hud.TextMuted,
    focusedTrailingIconColor = Hud.Accent, unfocusedTrailingIconColor = Hud.TextMuted
)
