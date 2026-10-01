package com.fyx.beebot.bt

import android.content.Context
import android.speech.tts.TextToSpeech
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.fyx.beebot.util.Haptics
import java.util.Locale

enum class SmartCarMode(val label: String, val command: Int, val voicePrompt: String, val gamepadKey: String) {
    BLUETOOTH("手动", 0x5B, "手动控制模式", "B"),
    AVOIDANCE("避障", 0x5C, "避障生效", "Y"),
    AUTONOMOUS("自主", 0x5D, "自主导航模式", "X");

    fun next(): SmartCarMode {
        val modes = values()
        return modes[(ordinal + 1) % modes.size]
    }
}

/** 智能小车的运行模式、停车保护和掌机语音反馈。 */
object SmartCarModeController {

    var mode by mutableStateOf(SmartCarMode.BLUETOOTH)
        private set

    private var speaker: TextToSpeech? = null
    private var speechReady = false
    private var pendingPrompt: String? = null

    fun initializeVoice(context: Context) {
        if (speaker != null) return
        speaker = TextToSpeech(context.applicationContext) { status ->
            speechReady = status == TextToSpeech.SUCCESS
            if (speechReady) {
                speaker?.language = Locale.SIMPLIFIED_CHINESE
                pendingPrompt?.let(::speak)
                pendingPrompt = null
            }
        }
    }

    fun releaseVoice() {
        speaker?.stop()
        speaker?.shutdown()
        speaker = null
        speechReady = false
        pendingPrompt = null
    }

    fun cycleMode() = selectMode(mode.next())

    /** 仅同步跨页面显示的车辆模式，不向当前蓝牙连接发送命令。 */
    fun rememberMode(target: SmartCarMode) { mode = target }

    fun selectMode(target: SmartCarMode) {
        if (!BluetoothRepository.connected) {
            mode = target
            Haptics.click()
            speak(target.voicePrompt)
            return
        }
        AckermannController.stopRemote(forceStop = true)
        AckermannController.startSmartCarRemote(target.command)
        mode = target
        Haptics.click()
        speak(target.voicePrompt)
    }

    fun emergencyStop() {
        AckermannController.emergencyStop()
        Haptics.heavy()
        speak("触发停车")
    }

    fun lowBatteryReminder() {
        Haptics.tick()
        speak("小车电量低，请及时充电")
    }

    private fun speak(prompt: String) {
        if (!speechReady) {
            pendingPrompt = prompt
            return
        }
        speaker?.speak(prompt, TextToSpeech.QUEUE_FLUSH, null, "smart-car-mode")
    }
}
