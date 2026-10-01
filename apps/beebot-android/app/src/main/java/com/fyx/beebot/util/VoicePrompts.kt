package com.fyx.beebot.util

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.speech.tts.TextToSpeech
import android.speech.tts.UtteranceProgressListener
import java.util.Locale
import java.util.concurrent.ConcurrentHashMap

/** 全局中文语音提示，避免不同页面重复创建和抢占 TTS 实例。 */
object VoicePrompts {
    private var engine: TextToSpeech? = null
    private var ready = false
    private var pending: PendingSpeech? = null
    private val callbacks = ConcurrentHashMap<String, () -> Unit>()
    private val main = Handler(Looper.getMainLooper())

    private data class PendingSpeech(val text: String, val onDone: (() -> Unit)?)

    fun initialize(context: Context) {
        if (engine != null) return
        engine = TextToSpeech(context.applicationContext) { status ->
            ready = status == TextToSpeech.SUCCESS
            if (ready) {
                engine?.language = Locale.SIMPLIFIED_CHINESE
                pending?.let { speak(it.text, it.onDone) }
                pending = null
            } else {
                pending?.onDone?.let(main::post)
                pending = null
            }
        }
        engine?.setOnUtteranceProgressListener(object : UtteranceProgressListener() {
            override fun onStart(utteranceId: String?) = Unit
            override fun onDone(utteranceId: String?) = complete(utteranceId)
            override fun onStop(utteranceId: String?, interrupted: Boolean) = complete(utteranceId)
            @Deprecated("Deprecated in Android")
            override fun onError(utteranceId: String?) = complete(utteranceId)
            override fun onError(utteranceId: String?, errorCode: Int) = complete(utteranceId)
        })
    }

    fun speak(text: String, onDone: (() -> Unit)? = null) {
        if (!ready) {
            pending?.onDone?.let(main::post)
            pending = PendingSpeech(text, onDone)
            return
        }
        val utteranceId = "bee-control-${System.nanoTime()}"
        if (onDone != null) callbacks[utteranceId] = onDone
        val result = engine?.speak(text, TextToSpeech.QUEUE_FLUSH, null, utteranceId)
        if (result == TextToSpeech.ERROR) complete(utteranceId)
    }

    private fun complete(utteranceId: String?) {
        if (utteranceId == null) return
        callbacks.remove(utteranceId)?.let { callback -> main.post(callback) }
    }

    fun release() {
        engine?.stop()
        engine?.shutdown()
        engine = null
        ready = false
        pending = null
        callbacks.clear()
    }
}
