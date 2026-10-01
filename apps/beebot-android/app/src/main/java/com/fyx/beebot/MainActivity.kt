package com.fyx.beebot

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.platform.LocalContext
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.fyx.beebot.input.GamepadRepository
import com.fyx.beebot.mqtt.RemoteVehicleRepository
import com.fyx.beebot.speech.SpeakerRecognitionController
import com.fyx.beebot.speech.VoiceAssistantController
import com.fyx.beebot.util.Haptics
import com.fyx.beebot.util.VoicePrompts
import com.fyx.beebot.ui.Auth
import com.fyx.beebot.ui.LoginScreen
import com.fyx.beebot.ui.MainScreen
import com.fyx.beebot.ui.theme.BeeBotTheme

/**
 * 应用入口。
 *
 * 手柄输入通过 Activity 的事件分发入口统一截获：
 *  - [dispatchGenericMotionEvent] —— 摇杆 / 扳机模拟量
 *  - [dispatchKeyEvent]           —— 按键 / 方向键
 * 交给 [GamepadRepository] 归一化后驱动界面刷新。
 */
class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // 监视界面常亮
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        WindowCompat.getInsetsController(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        Haptics.init(this)
        VoicePrompts.initialize(this)
        RemoteVehicleRepository.initialize(this)
        GamepadRepository.refreshConnectedDevices()

        // 运行时权限：定位（WiFi 名称）+ 蓝牙（API31+ 需运行时授予；本掌机 API30 为安装期权限）
        val perms = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.READ_PHONE_STATE)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            perms += Manifest.permission.BLUETOOTH_CONNECT
            perms += Manifest.permission.BLUETOOTH_SCAN
        }
        val need = perms.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (need.isNotEmpty()) requestPermissions(need.toTypedArray(), 1001)

        setContent {
            BeeBotTheme {
                AppRoot()
            }
        }
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (GamepadRepository.processMotionEvent(event)) return true
        return super.dispatchGenericMotionEvent(event)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (GamepadRepository.processKeyEvent(event)) return true
        return super.dispatchKeyEvent(event)
    }

    override fun onStop() {
        VoiceAssistantController.pause()
        SpeakerRecognitionController.stopEnrollment()
        super.onStop()
    }

    override fun onDestroy() {
        com.fyx.beebot.speech.SpeechRecognitionController.stop()
        com.fyx.beebot.speech.SpeakerRecognitionController.stopEnrollment()
        RemoteVehicleRepository.disconnect()
        VoicePrompts.release()
        super.onDestroy()
    }
}

@Composable
private fun AppRoot() {
    val context = LocalContext.current
    var loggedIn by rememberSaveable { mutableStateOf(false) }
    var checked by rememberSaveable { mutableStateOf(false) }

    LaunchedEffect(Unit) {
        if (!checked) {
            val prefs = context.getSharedPreferences(Auth.PREFS, Context.MODE_PRIVATE)
            val auto = prefs.getBoolean(Auth.KEY_AUTO, false)
            val acc = prefs.getString(Auth.KEY_ACCOUNT, "") ?: ""
            val pwd = prefs.getString(Auth.KEY_PASSWORD, "") ?: ""
            if (auto && Auth.validate(acc, pwd)) loggedIn = true
            checked = true
        }
    }

    if (loggedIn) {
        MainScreen(onLogout = {
            context.getSharedPreferences(Auth.PREFS, Context.MODE_PRIVATE)
                .edit().putBoolean(Auth.KEY_AUTO, false).apply()
            loggedIn = false
        })
    } else {
        LoginScreen(onLoginSuccess = { loggedIn = true })
    }
}
