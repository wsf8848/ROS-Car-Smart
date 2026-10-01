package com.fyx.beebot.speech

import kotlin.math.abs
import kotlin.math.log10
import kotlin.math.sqrt

enum class VoiceScene(val label: String) {
    QUIET_ROOM("安静室内"),
    VEHICLE("车辆内部"),
    OUTDOOR("室外道路"),
    OTHER("其他场景")
}

enum class VoiceDistance(val label: String) {
    NEAR("近距离 20-40cm"),
    MEDIUM("中距离 1-2m"),
    FAR("远距离 3-5m")
}

data class AudioQuality(
    val durationMs: Long,
    val rmsDb: Float,
    val peak: Float,
    val clippingRatio: Float,
    val speechRatio: Float,
    val score: Int,
    val usableForRecognition: Boolean,
    val usableForEnrollment: Boolean,
    val message: String
)

data class SpeakerVoiceSample(
    val id: String,
    val scene: VoiceScene,
    val distance: VoiceDistance,
    val durationMs: Long,
    val createdAtMs: Long,
    val quality: AudioQuality,
    val embedding: FloatArray,
    val recordingPath: String? = null,
    val source: String = "enrollment"
)

data class SpeakerProfile(
    val id: String,
    val name: String,
    val samples: List<SpeakerVoiceSample>
)

data class SpeakerIdentificationResult(
    val profileId: String?,
    val speakerName: String,
    val score: Float,
    val embedding: FloatArray?,
    val quality: AudioQuality,
    val reason: String
)

data class SpeakerMatch(
    val profileId: String?,
    val name: String,
    val score: Float,
    val secondScore: Float,
    val accepted: Boolean
)

object SpeakerAudioAnalyzer {
    private const val WINDOW_MS = 20
    private const val SPEECH_RMS = 0.008f

    fun analyze(samples: FloatArray, sampleRate: Int = 16000): AudioQuality {
        if (samples.isEmpty()) {
            return AudioQuality(0, -120f, 0f, 0f, 0f, 0, false, false, "没有采集到声音")
        }

        var sumSquares = 0.0
        var peak = 0f
        var clipping = 0
        samples.forEach { sample ->
            val value = abs(sample)
            sumSquares += sample * sample
            if (value > peak) peak = value
            if (value >= 0.98f) clipping++
        }
        val rms = sqrt(sumSquares / samples.size).toFloat()
        val rmsDb = (20.0 * log10(rms.coerceAtLeast(0.000001f).toDouble())).toFloat()
        val clippingRatio = clipping.toFloat() / samples.size
        val durationMs = samples.size * 1000L / sampleRate

        val windowSize = (sampleRate * WINDOW_MS / 1000).coerceAtLeast(1)
        var voicedWindows = 0
        var totalWindows = 0
        var offset = 0
        while (offset < samples.size) {
            val end = (offset + windowSize).coerceAtMost(samples.size)
            var windowSquares = 0.0
            for (index in offset until end) windowSquares += samples[index] * samples[index]
            val windowRms = sqrt(windowSquares / (end - offset)).toFloat()
            if (windowRms >= SPEECH_RMS) voicedWindows++
            totalWindows++
            offset = end
        }
        val speechRatio = if (totalWindows == 0) 0f else voicedWindows.toFloat() / totalWindows

        val recognitionUsable = durationMs >= 900 && rmsDb >= -48f && speechRatio >= 0.20f && clippingRatio <= 0.03f
        val enrollmentUsable = durationMs in 2500..12000 && rmsDb in -42f..-3f &&
            speechRatio >= 0.42f && clippingRatio <= 0.01f
        val score = qualityScore(durationMs, rmsDb, clippingRatio, speechRatio)
        val message = when {
            durationMs < 2500 -> "录音过短，请至少说满 3 秒"
            durationMs > 12000 -> "录音过长，请控制在 12 秒内"
            rmsDb < -42f -> "声音太小，请靠近麦克风或提高音量"
            rmsDb > -3f || clippingRatio > 0.01f -> "音量过大并出现削波，请稍微远离麦克风"
            speechRatio < 0.42f -> "有效人声太少，请连续自然说话"
            else -> "样本质量合格"
        }
        return AudioQuality(
            durationMs = durationMs,
            rmsDb = rmsDb,
            peak = peak,
            clippingRatio = clippingRatio,
            speechRatio = speechRatio,
            score = score,
            usableForRecognition = recognitionUsable,
            usableForEnrollment = enrollmentUsable,
            message = message
        )
    }

    fun trimSilence(samples: FloatArray, sampleRate: Int = 16000): FloatArray {
        if (samples.isEmpty()) return samples
        val window = (sampleRate * WINDOW_MS / 1000).coerceAtLeast(1)
        var first = -1
        var last = -1
        var offset = 0
        while (offset < samples.size) {
            val end = (offset + window).coerceAtMost(samples.size)
            var sum = 0.0
            for (index in offset until end) sum += samples[index] * samples[index]
            if (sqrt(sum / (end - offset)) >= SPEECH_RMS) {
                if (first < 0) first = offset
                last = end
            }
            offset = end
        }
        if (first < 0) return samples
        val padding = sampleRate / 10
        return samples.copyOfRange((first - padding).coerceAtLeast(0), (last + padding).coerceAtMost(samples.size))
    }

    private fun qualityScore(durationMs: Long, rmsDb: Float, clippingRatio: Float, speechRatio: Float): Int {
        var score = 100
        if (durationMs < 3000) score -= 25
        if (durationMs > 10000) score -= 10
        if (rmsDb < -36f) score -= 20
        if (rmsDb > -6f) score -= 15
        if (speechRatio < 0.55f) score -= 20
        if (clippingRatio > 0.002f) score -= 25
        return score.coerceIn(0, 100)
    }
}

object SpeakerMatcher {
    const val DEFAULT_THRESHOLD = 0.58f
    const val DEFAULT_MARGIN = 0.035f

    fun match(
        profiles: List<SpeakerProfile>,
        embedding: FloatArray,
        threshold: Float = DEFAULT_THRESHOLD,
        minimumMargin: Float = DEFAULT_MARGIN
    ): SpeakerMatch {
        val scores = profiles.mapNotNull { profile ->
            if (profile.samples.isEmpty()) return@mapNotNull null
            val sampleScores = profile.samples.map { cosineSimilarity(it.embedding, embedding) }
            val maxScore = sampleScores.maxOrNull() ?: return@mapNotNull null
            val centroidScore = cosineSimilarity(centroid(profile.samples.map { it.embedding }), embedding)
            profile to (maxScore * 0.65f + centroidScore * 0.35f)
        }.sortedByDescending { it.second }

        val best = scores.firstOrNull()
            ?: return SpeakerMatch(null, "未知说话人", 0f, 0f, false)
        val second = scores.getOrNull(1)?.second ?: -1f
        val accepted = best.second >= threshold && (scores.size == 1 || best.second - second >= minimumMargin)
        return SpeakerMatch(
            profileId = if (accepted) best.first.id else null,
            name = if (accepted) best.first.name else "未知说话人",
            score = best.second,
            secondScore = second.coerceAtLeast(0f),
            accepted = accepted
        )
    }

    fun cosineSimilarity(left: FloatArray, right: FloatArray): Float {
        if (left.isEmpty() || left.size != right.size) return -1f
        var dot = 0.0
        var leftNorm = 0.0
        var rightNorm = 0.0
        for (index in left.indices) {
            dot += left[index] * right[index]
            leftNorm += left[index] * left[index]
            rightNorm += right[index] * right[index]
        }
        if (leftNorm <= 0.0 || rightNorm <= 0.0) return -1f
        return (dot / sqrt(leftNorm * rightNorm)).toFloat().coerceIn(-1f, 1f)
    }

    private fun centroid(embeddings: List<FloatArray>): FloatArray {
        val size = embeddings.first().size
        val result = FloatArray(size)
        var accepted = 0
        embeddings.forEach { embedding ->
            if (embedding.size != size) return@forEach
            for (index in result.indices) result[index] += embedding[index]
            accepted++
        }
        if (accepted > 0) {
            for (index in result.indices) result[index] = result[index] / accepted.toFloat()
        }
        return result
    }
}
