package com.fyx.beebot.speech

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import com.k2fsa.sherpa.onnx.EndpointConfig
import com.k2fsa.sherpa.onnx.EndpointRule
import com.k2fsa.sherpa.onnx.FeatureConfig
import com.k2fsa.sherpa.onnx.OnlineModelConfig
import com.k2fsa.sherpa.onnx.OnlineParaformerModelConfig
import com.k2fsa.sherpa.onnx.OnlineRecognizer
import com.k2fsa.sherpa.onnx.OnlineRecognizerConfig
import kotlin.concurrent.thread
import kotlin.math.sqrt

enum class SpeechRecognitionState {
    IDLE, LOADING, READY, LISTENING, ERROR
}

/** 管理离线模型、麦克风与流式解码，避免识别线程耦合到 Compose 页面。 */
object SpeechRecognitionController {
    private const val SAMPLE_RATE = 16000
    private const val MODEL_DIR = "sherpa-onnx-streaming-paraformer-bilingual-zh-en"

    var state by mutableStateOf(SpeechRecognitionState.IDLE)
        private set
    var completedText by mutableStateOf("")
        private set
    var partialText by mutableStateOf("")
        private set
    var audioLevel by mutableStateOf(0f)
        private set
    var decodeTimeMs by mutableStateOf<Long?>(null)
        private set
    var errorMessage by mutableStateOf<String?>(null)
        private set
    var audioSegmentSpeakerResolver: ((FloatArray) -> String?)? = null
    var finalSegmentListener: ((speakerName: String?, text: String) -> Unit)? = null

    private val mainHandler = Handler(Looper.getMainLooper())
    private val textLock = Any()
    private val completedSegments = mutableListOf<String>()
    private var recognizer: OnlineRecognizer? = null
    private var audioRecord: AudioRecord? = null
    private var recordingThread: Thread? = null

    @Volatile
    private var recording = false

    @Synchronized
    fun initialize(context: Context) {
        if (recognizer != null || state == SpeechRecognitionState.LOADING) return
        state = SpeechRecognitionState.LOADING
        errorMessage = null
        val appContext = context.applicationContext
        thread(name = "speech-model-loader", isDaemon = true) {
            try {
                val config = OnlineRecognizerConfig(
                    featConfig = FeatureConfig(sampleRate = SAMPLE_RATE, featureDim = 80),
                    modelConfig = OnlineModelConfig(
                        paraformer = OnlineParaformerModelConfig(
                            encoder = "$MODEL_DIR/encoder.int8.onnx",
                            decoder = "$MODEL_DIR/decoder.int8.onnx"
                        ),
                        tokens = "$MODEL_DIR/tokens.txt",
                        numThreads = 4,
                        provider = "cpu",
                        modelType = "paraformer"
                    ),
                    endpointConfig = EndpointConfig(
                        rule1 = EndpointRule(false, 2.4f, 0f),
                        rule2 = EndpointRule(true, 0.8f, 0f),
                        rule3 = EndpointRule(false, 0f, 20f)
                    ),
                    enableEndpoint = true,
                    decodingMethod = "greedy_search"
                )
                val loaded = OnlineRecognizer(appContext.assets, config)
                mainHandler.post {
                    recognizer = loaded
                    state = SpeechRecognitionState.READY
                }
            } catch (t: Throwable) {
                postError("模型加载失败：${t.message ?: t.javaClass.simpleName}")
            }
        }
    }

    fun start(context: Context) {
        if (state == SpeechRecognitionState.LOADING || state == SpeechRecognitionState.LISTENING) return
        if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            postError("需要麦克风权限才能开始识别")
            return
        }
        val activeRecognizer = recognizer ?: run {
            initialize(context)
            return
        }
        val minBytes = AudioRecord.getMinBufferSize(
            SAMPLE_RATE,
            AudioFormat.CHANNEL_IN_MONO,
            AudioFormat.ENCODING_PCM_16BIT
        )
        if (minBytes <= 0) {
            postError("无法获取麦克风缓冲区")
            return
        }

        try {
            val recorder = AudioRecord(
                MediaRecorder.AudioSource.VOICE_RECOGNITION,
                SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                minBytes * 2
            )
            if (recorder.state != AudioRecord.STATE_INITIALIZED) {
                recorder.release()
                postError("麦克风初始化失败")
                return
            }
            audioRecord = recorder
            recorder.startRecording()
            recording = true
            errorMessage = null
            state = SpeechRecognitionState.LISTENING
            recordingThread = thread(name = "speech-streaming-decoder", isDaemon = true) {
                processSamples(recorder, activeRecognizer)
            }
        } catch (t: Throwable) {
            runCatching { audioRecord?.release() }
            audioRecord = null
            postError("启动识别失败：${t.message ?: t.javaClass.simpleName}")
        }
    }

    fun stop() {
        recording = false
        val pending = partialText.trim()
        if (pending.isNotBlank()) {
            synchronized(textLock) {
                if (completedSegments.lastOrNull() != pending) completedSegments += pending
                completedText = completedSegments.joinToString("\n")
            }
        }
        val recorder = audioRecord
        audioRecord = null
        runCatching { recorder?.stop() }
        recordingThread?.interrupt()
        recordingThread = null
        mainHandler.post {
            audioLevel = 0f
            partialText = ""
            if (recognizer != null && state != SpeechRecognitionState.ERROR) {
                state = SpeechRecognitionState.READY
            }
        }
    }

    fun clearText() {
        synchronized(textLock) { completedSegments.clear() }
        completedText = ""
        partialText = ""
    }

    fun reportPermissionDenied() {
        postError("麦克风权限被拒绝，请在系统设置中允许录音权限")
    }

    private fun processSamples(recorder: AudioRecord, activeRecognizer: OnlineRecognizer) {
        val stream = activeRecognizer.createStream()
        val buffer = ShortArray(1600)
        val segmentSamples = ArrayList<Float>()
        try {
            while (recording) {
                val count = recorder.read(buffer, 0, buffer.size)
                if (count <= 0) continue
                var sumSquares = 0.0
                val samples = FloatArray(count) { index ->
                    val sample = buffer[index] / 32768f
                    sumSquares += sample * sample
                    sample
                }
                segmentSamples.ensureCapacity(segmentSamples.size + samples.size)
                samples.forEach(segmentSamples::add)
                stream.acceptWaveform(samples, SAMPLE_RATE)
                val startedAt = SystemClock.elapsedRealtime()
                while (activeRecognizer.isReady(stream)) activeRecognizer.decode(stream)
                val elapsed = SystemClock.elapsedRealtime() - startedAt
                val endpoint = activeRecognizer.isEndpoint(stream)
                var text = activeRecognizer.getResult(stream).text.trim()
                var completedSegment: Pair<String?, String>? = null

                if (endpoint) {
                    stream.acceptWaveform(FloatArray((0.8f * SAMPLE_RATE).toInt()), SAMPLE_RATE)
                    while (activeRecognizer.isReady(stream)) activeRecognizer.decode(stream)
                    text = activeRecognizer.getResult(stream).text.trim()
                }

                if (endpoint && text.isNotBlank()) {
                    val speaker = if (segmentSamples.isNotEmpty()) {
                        audioSegmentSpeakerResolver?.invoke(segmentSamples.toFloatArray())
                    } else null
                    synchronized(textLock) { completedSegments += if (speaker.isNullOrBlank()) text else "$speaker：$text" }
                    completedSegment = speaker to text
                    segmentSamples.clear()
                    activeRecognizer.reset(stream)
                } else if (endpoint) {
                    segmentSamples.clear()
                    activeRecognizer.reset(stream)
                }

                val finalSnapshot = synchronized(textLock) { completedSegments.joinToString("\n") }
                val partialSnapshot = if (endpoint) "" else text
                val rms = sqrt(sumSquares / count).toFloat()
                mainHandler.post {
                    completedText = finalSnapshot
                    partialText = partialSnapshot
                    audioLevel = (rms * 7f).coerceIn(0f, 1f)
                    decodeTimeMs = elapsed
                    completedSegment?.let { (speaker, finalText) -> finalSegmentListener?.invoke(speaker, finalText) }
                }
            }
        } catch (t: Throwable) {
            if (recording) postError("识别中断：${t.message ?: t.javaClass.simpleName}")
        } finally {
            recording = false
            runCatching { stream.release() }
            runCatching { recorder.stop() }
            recorder.release()
            mainHandler.post {
                if (audioRecord === recorder) audioRecord = null
                audioLevel = 0f
                if (state == SpeechRecognitionState.LISTENING) state = SpeechRecognitionState.READY
            }
        }
    }

    private fun postError(message: String) {
        mainHandler.post {
            recording = false
            errorMessage = message
            state = SpeechRecognitionState.ERROR
            audioLevel = 0f
        }
    }
}
