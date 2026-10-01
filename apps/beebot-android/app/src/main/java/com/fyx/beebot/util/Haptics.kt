package com.fyx.beebot.util

import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager

/**
 * 全局震动反馈（触觉）。三档强度：
 *  - tick()  轻·点触感（Tab切换/编码档位/速度±/实体手柄键）
 *  - click() 中·确认感（登录/发送/遥控开关/连接设备）
 *  - heavy() 强·双震（紧急停车/连接成功/断开）
 *
 * 单例，`init(context)` 在 MainActivity.onCreate 调用一次；可全局开关（存 SharedPreferences）。
 * Retroid Pocket 4 支持幅度控制，用 VibrationEffect 精细区分强弱。
 */
object Haptics {
    const val KEY_ENABLED = "haptic_enabled"
    private const val PREFS = "beebot_prefs"

    private var vibrator: Vibrator? = null
    private var prefs: SharedPreferences? = null
    var enabled = true; private set

    fun init(context: Context) {
        val app = context.applicationContext
        prefs = app.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        enabled = prefs?.getBoolean(KEY_ENABLED, true) ?: true
        vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            (app.getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager).defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            app.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
        }
    }

    fun setEnabled(e: Boolean) {
        enabled = e
        prefs?.edit()?.putBoolean(KEY_ENABLED, e)?.apply()
    }

    fun tick() = waveform(longArrayOf(0, 55), intArrayOf(0, 255))
    fun click() = waveform(longArrayOf(0, 70), intArrayOf(0, 255))

    /** 强·双震（最醒目，用于紧急停车/连接成功/断开）。 */
    fun heavy() {
        waveform(longArrayOf(0, 55, 55, 55), intArrayOf(0, 255, 0, 255))
    }

    private fun waveform(timings: LongArray, amplitudes: IntArray) {
        if (!enabled) return
        val v = vibrator ?: return
        if (!v.hasVibrator()) return
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                v.vibrate(VibrationEffect.createWaveform(timings, amplitudes, -1))
            } else {
                @Suppress("DEPRECATION") v.vibrate(timings, -1)
            }
        } catch (_: Exception) {}
    }
}
