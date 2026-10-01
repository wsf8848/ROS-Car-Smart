package com.fyx.beebot.ui

import android.content.Context
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Lock
import androidx.compose.material.icons.filled.Person
import androidx.compose.material.icons.filled.SmartToy
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.fyx.beebot.ui.theme.Hud

/** 登录鉴权常量。 */
private object LegacyAuth {
    val ACCOUNT: String get() = com.fyx.beebot.BuildConfig.LOCAL_LOGIN_ACCOUNT
    val PASSWORD: String get() = com.fyx.beebot.BuildConfig.LOCAL_LOGIN_PASSWORD

    const val PREFS = "beebot_prefs"
    const val KEY_ACCOUNT = "login_account"
    const val KEY_PASSWORD = "login_password"
    const val KEY_AUTO = "auto_login"

    fun validate(account: String, password: String): Boolean =
        ACCOUNT.isNotBlank() && PASSWORD.isNotBlank() &&
            account.trim() == ACCOUNT && password == PASSWORD
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun LegacyLoginScreen(onLoginSuccess: () -> Unit) {
    val context = LocalContext.current
    val prefs = remember { context.getSharedPreferences(LegacyAuth.PREFS, Context.MODE_PRIVATE) }

    var account by remember { mutableStateOf(prefs.getString(LegacyAuth.KEY_ACCOUNT, "") ?: "") }
    var password by remember { mutableStateOf("") }
    var autoLogin by remember { mutableStateOf(prefs.getBoolean(LegacyAuth.KEY_AUTO, false)) }
    var error by remember { mutableStateOf("") }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Hud.Background)
            .verticalScroll(rememberScrollState())
            .padding(vertical = 16.dp),
        contentAlignment = Alignment.Center
    ) {
        Column(
            modifier = Modifier
                .width(360.dp)
                .background(Hud.Panel, RoundedCornerShape(16.dp))
                .border(1.dp, Hud.Border, RoundedCornerShape(16.dp))
                .padding(horizontal = 28.dp, vertical = 16.dp),
            horizontalAlignment = Alignment.CenterHorizontally
        ) {
            Box(
                modifier = Modifier
                    .size(46.dp)
                    .background(Hud.PanelDeep, RoundedCornerShape(12.dp))
                    .border(1.dp, Hud.Accent, RoundedCornerShape(12.dp)),
                contentAlignment = Alignment.Center
            ) {
                Icon(Icons.Filled.SmartToy, contentDescription = null, tint = Hud.Accent, modifier = Modifier.size(26.dp))
            }
            Spacer(Modifier.height(10.dp))
            Text("小蜜蜂机器人控制器", color = Hud.TextPrimary, fontSize = 18.sp, fontWeight = FontWeight.Medium)
            Text("XiaoMiFeng Robot Controller", color = Hud.TextMuted, fontSize = 11.sp, fontFamily = FontFamily.Monospace)
            Spacer(Modifier.height(14.dp))

            OutlinedTextField(
                value = account,
                onValueChange = { account = it; error = "" },
                label = { Text("登录账号") },
                singleLine = true,
                leadingIcon = { Icon(Icons.Filled.Person, contentDescription = null) },
                colors = hudFieldColors(),
                modifier = Modifier.fillMaxWidth()
            )
            Spacer(Modifier.height(10.dp))
            OutlinedTextField(
                value = password,
                onValueChange = { password = it; error = "" },
                label = { Text("登录密码") },
                singleLine = true,
                visualTransformation = PasswordVisualTransformation(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.NumberPassword),
                leadingIcon = { Icon(Icons.Filled.Lock, contentDescription = null) },
                colors = hudFieldColors(),
                modifier = Modifier.fillMaxWidth()
            )

            Spacer(Modifier.height(10.dp))
            Row(
                modifier = Modifier.fillMaxWidth(),
                verticalAlignment = Alignment.CenterVertically
            ) {
                Checkbox(
                    checked = autoLogin,
                    onCheckedChange = { autoLogin = it },
                    colors = CheckboxDefaults.colors(checkedColor = Hud.Accent, uncheckedColor = Hud.TextMuted)
                )
                Text("下次自动登录", color = Hud.TextSecondary, fontSize = 13.sp)
            }

            if (error.isNotEmpty()) {
                Spacer(Modifier.height(4.dp))
                Text(error, color = Hud.Danger, fontSize = 12.sp)
            }

            Spacer(Modifier.height(14.dp))
            Box(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(50.dp)
                    .background(Hud.Accent, RoundedCornerShape(10.dp))
                    .clickable {
                        com.fyx.beebot.util.Haptics.click()
                        if (LegacyAuth.validate(account, password)) {
                            prefs.edit()
                                .putString(LegacyAuth.KEY_ACCOUNT, account.trim())
                                .putString(LegacyAuth.KEY_PASSWORD, password)
                                .putBoolean(LegacyAuth.KEY_AUTO, autoLogin)
                                .apply()
                            onLoginSuccess()
                        } else {
                            error = "账号或密码错误"
                        }
                    },
                contentAlignment = Alignment.Center
            ) {
                Text("登 录", color = Color(0xFF042027), fontSize = 17.sp, fontWeight = FontWeight.Bold)
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun hudFieldColors() = OutlinedTextFieldDefaults.colors(
    focusedBorderColor = Hud.Accent,
    unfocusedBorderColor = Hud.Border,
    focusedLabelColor = Hud.Accent,
    unfocusedLabelColor = Hud.TextMuted,
    focusedTextColor = Hud.TextPrimary,
    unfocusedTextColor = Hud.TextPrimary,
    cursorColor = Hud.Accent,
    focusedLeadingIconColor = Hud.Accent,
    unfocusedLeadingIconColor = Hud.TextMuted
)
