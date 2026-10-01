package com.fyx.beebot.speech

import android.Manifest
import android.content.Context
import android.content.pm.PackageManager
import android.media.AudioFormat
import android.media.AudioRecord
import android.media.MediaRecorder
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.core.content.ContextCompat
import com.k2fsa.sherpa.onnx.SpeakerEmbeddingExtractor
import com.k2fsa.sherpa.onnx.SpeakerEmbeddingExtractorConfig
import kotlin.concurrent.thread
import java.io.File
import java.io.FileOutputStream

enum class SpeakerRuntimeState { IDLE, LOADING, READY, RECORDING, ERROR }

object SpeakerRecognitionController {
    private const val SAMPLE_RATE = 16000
    var state by mutableStateOf(SpeakerRuntimeState.IDLE); private set
    var profiles by mutableStateOf<List<SpeakerProfile>>(emptyList()); private set
    var recordingLevel by mutableStateOf(0f); private set
    var lastResult by mutableStateOf<SpeakerIdentificationResult?>(null); private set
    var errorMessage by mutableStateOf<String?>(null); private set
    var enrollmentHint by mutableStateOf<String?>(null); private set
    private var extractor: SpeakerEmbeddingExtractor? = null
    private var store: SpeakerProfileStore? = null
    private var recordingsDir: File? = null
    private var recorder: AudioRecord? = null
    @Volatile private var recording = false

    fun initialize(context: Context) {
        if (state == SpeakerRuntimeState.LOADING || extractor != null) return
        state = SpeakerRuntimeState.LOADING
        val app = context.applicationContext
        thread(name = "speaker-model-loader", isDaemon = true) {
            try {
                val config = SpeakerEmbeddingExtractorConfig(
                    "3dspeaker_speech_campplus_sv_zh-cn_16k-common.onnx", 2, false, "cpu"
                )
                extractor = SpeakerEmbeddingExtractor(app.assets, config)
                store = SpeakerProfileStore(app)
                recordingsDir = File(app.filesDir, "speaker_recordings").apply { mkdirs() }
                profiles = runCatching { store!!.load() }.getOrDefault(emptyList())
                state = SpeakerRuntimeState.READY
            } catch (t: Throwable) { state = SpeakerRuntimeState.ERROR; errorMessage = "声纹模型加载失败：${t.message}" }
        }
    }

    fun startEnrollment(context: Context, profileId: String?, name: String, scene: VoiceScene, distance: VoiceDistance, keepAudio: Boolean = false) {
        if (ContextCompat.checkSelfPermission(context, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) { errorMessage = "需要麦克风权限"; return }
        if (state == SpeakerRuntimeState.LOADING || state == SpeakerRuntimeState.RECORDING) return
        if (extractor == null) { initialize(context); enrollmentHint = "模型加载中，请稍候"; return }
        val min = AudioRecord.getMinBufferSize(SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT)
        if (min <= 0) { errorMessage = "无法初始化麦克风"; return }
        val id = profileId ?: java.util.UUID.randomUUID().toString()
        val samples = java.util.ArrayList<Float>()
        val ar = AudioRecord(MediaRecorder.AudioSource.VOICE_RECOGNITION, SAMPLE_RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, min * 2)
        recorder = ar; recording = true; state = SpeakerRuntimeState.RECORDING; enrollmentHint = "请连续说话 3-12 秒，然后点击停止"
        ar.startRecording()
        thread(name = "speaker-enrollment-recorder", isDaemon = true) {
            val buf = ShortArray(1600)
            try {
                while (recording) {
                    val n = ar.read(buf, 0, buf.size); if (n <= 0) continue
                    var sum = 0.0
                    for (i in 0 until n) { val v = buf[i] / 32768f; samples.add(v); sum += v * v }
                    recordingLevel = (kotlin.math.sqrt(sum / n) * 7f).toFloat().coerceIn(0f, 1f)
                }
            } finally { runCatching { ar.stop() }; ar.release(); recorder = null; recordingLevel = 0f; state = SpeakerRuntimeState.READY }
            if (samples.isNotEmpty()) finishEnrollment(samples.toFloatArray(), id, name, scene, distance, keepAudio)
        }
    }

    fun stopEnrollment() { recording = false; runCatching { recorder?.stop() } }

    private fun finishEnrollment(raw: FloatArray, id: String, name: String, scene: VoiceScene, distance: VoiceDistance, keepAudio: Boolean) {
        val trimmed = SpeakerAudioAnalyzer.trimSilence(raw); val quality = SpeakerAudioAnalyzer.analyze(trimmed)
        if (!quality.usableForEnrollment) { enrollmentHint = "样本未收录：${quality.message}"; return }
        val ex = extractor ?: return; val stream = ex.createStream(); stream.acceptWaveform(trimmed, SAMPLE_RATE); stream.inputFinished()
        val embedding = ex.compute(stream); stream.release()
        val sampleId = java.util.UUID.randomUUID().toString()
        val recordingPath = if (keepAudio) runCatching { writeWav(sampleId, trimmed) }.getOrNull() else null
        val sample = SpeakerVoiceSample(sampleId, scene, distance, quality.durationMs, System.currentTimeMillis(), quality, embedding, recordingPath, "enrollment")
        val current = profiles.toMutableList(); val index = current.indexOfFirst { it.id == id }
        if (index >= 0) current[index] = current[index].copy(name = name.ifBlank { current[index].name }, samples = current[index].samples + sample)
        else current += SpeakerProfile(id, name.ifBlank { "未命名角色" }, listOf(sample))
        profiles = current; store?.save(current); enrollmentHint = "已加入样本（质量 ${quality.score} 分）"
    }

    fun identify(samples: FloatArray): SpeakerIdentificationResult? {
        val ex = extractor ?: return null; val trimmed = SpeakerAudioAnalyzer.trimSilence(samples); val quality = SpeakerAudioAnalyzer.analyze(trimmed)
        if (!quality.usableForRecognition) return null
        val stream = ex.createStream(); stream.acceptWaveform(trimmed, SAMPLE_RATE); stream.inputFinished(); val embedding = ex.compute(stream); stream.release()
        val match = SpeakerMatcher.match(profiles, embedding)
        return SpeakerIdentificationResult(match.profileId, match.name, match.score, embedding, quality, if (match.accepted) "匹配成功" else "置信度不足或存在歧义").also { lastResult = it }
    }

    fun rename(id: String, name: String) { profiles = profiles.map { if (it.id == id) it.copy(name = name) else it }; store?.save(profiles) }
    fun delete(id: String) { profiles = profiles.filterNot { it.id == id }; store?.save(profiles) }
    fun confirmLearning(id: String, embedding: FloatArray, scene: VoiceScene = VoiceScene.OTHER, distance: VoiceDistance = VoiceDistance.MEDIUM) {
        val quality = AudioQuality(0, 0f, 0f, 0f, 1f, 70, true, false, "人工确认")
        val sample = SpeakerVoiceSample(java.util.UUID.randomUUID().toString(), scene, distance, 0, System.currentTimeMillis(), quality, embedding, null, "confirmed-learning")
        profiles = profiles.map { if (it.id == id) it.copy(samples = it.samples + sample) else it }; store?.save(profiles)
    }

    private fun writeWav(id: String, samples: FloatArray): String {
        val file = File(requireNotNull(recordingsDir), "$id.wav")
        val dataSize = samples.size * 2
        FileOutputStream(file).use { out ->
            fun le16(value: Int) { out.write(value and 0xff); out.write((value ushr 8) and 0xff) }
            fun le32(value: Int) { le16(value and 0xffff); le16(value ushr 16) }
            out.write("RIFF".toByteArray()); le32(36 + dataSize); out.write("WAVEfmt ".toByteArray())
            le32(16); le16(1); le16(1); le32(SAMPLE_RATE); le32(SAMPLE_RATE * 2); le16(2); le16(16)
            out.write("data".toByteArray()); le32(dataSize)
            samples.forEach { sample -> le16((sample.coerceIn(-1f, 1f) * 32767f).toInt()) }
        }
        return file.absolutePath
    }
}
