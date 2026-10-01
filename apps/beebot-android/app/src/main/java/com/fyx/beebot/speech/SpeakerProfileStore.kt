package com.fyx.beebot.speech

import android.content.Context
import android.util.AtomicFile
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.DataInputStream
import java.io.DataOutputStream

class SpeakerProfileStore(context: Context) {
    private val file = AtomicFile(context.filesDir.resolve(FILE_NAME))

    fun load(): List<SpeakerProfile> {
        if (!file.baseFile.exists()) return emptyList()
        return DataInputStream(BufferedInputStream(file.openRead())).use { input ->
            require(input.readInt() == MAGIC) { "声纹档案格式无效" }
            require(input.readInt() == VERSION) { "声纹档案版本不受支持" }
            val profileCount = input.readInt().coerceIn(0, MAX_PROFILES)
            List(profileCount) {
                val profileId = input.readUTF()
                val profileName = input.readUTF()
                val sampleCount = input.readInt().coerceIn(0, MAX_SAMPLES_PER_PROFILE)
                val samples = List(sampleCount) {
                    val sampleId = input.readUTF()
                    val scene = VoiceScene.valueOf(input.readUTF())
                    val distance = VoiceDistance.valueOf(input.readUTF())
                    val durationMs = input.readLong()
                    val createdAtMs = input.readLong()
                    val rmsDb = input.readFloat()
                    val peak = input.readFloat()
                    val clippingRatio = input.readFloat()
                    val speechRatio = input.readFloat()
                    val qualityScore = input.readInt()
                    val usableForRecognition = input.readBoolean()
                    val usableForEnrollment = input.readBoolean()
                    val qualityMessage = input.readUTF()
                    val path = input.readUTF().ifBlank { null }
                    val source = input.readUTF()
                    val dimension = input.readInt()
                    require(dimension in 1..MAX_EMBEDDING_DIMENSION) { "声纹维度无效：$dimension" }
                    val embedding = FloatArray(dimension) { input.readFloat() }
                    SpeakerVoiceSample(
                        id = sampleId,
                        scene = scene,
                        distance = distance,
                        durationMs = durationMs,
                        createdAtMs = createdAtMs,
                        quality = AudioQuality(
                            durationMs, rmsDb, peak, clippingRatio, speechRatio, qualityScore,
                            usableForRecognition, usableForEnrollment, qualityMessage
                        ),
                        embedding = embedding,
                        recordingPath = path,
                        source = source
                    )
                }
                SpeakerProfile(profileId, profileName, samples)
            }
        }
    }

    fun save(profiles: List<SpeakerProfile>) {
        val stream = file.startWrite()
        try {
            val output = DataOutputStream(BufferedOutputStream(stream))
            output.writeInt(MAGIC)
            output.writeInt(VERSION)
            output.writeInt(profiles.size.coerceAtMost(MAX_PROFILES))
            profiles.take(MAX_PROFILES).forEach { profile ->
                output.writeUTF(profile.id)
                output.writeUTF(profile.name)
                output.writeInt(profile.samples.size.coerceAtMost(MAX_SAMPLES_PER_PROFILE))
                profile.samples.take(MAX_SAMPLES_PER_PROFILE).forEach { sample ->
                    output.writeUTF(sample.id)
                    output.writeUTF(sample.scene.name)
                    output.writeUTF(sample.distance.name)
                    output.writeLong(sample.durationMs)
                    output.writeLong(sample.createdAtMs)
                    output.writeFloat(sample.quality.rmsDb)
                    output.writeFloat(sample.quality.peak)
                    output.writeFloat(sample.quality.clippingRatio)
                    output.writeFloat(sample.quality.speechRatio)
                    output.writeInt(sample.quality.score)
                    output.writeBoolean(sample.quality.usableForRecognition)
                    output.writeBoolean(sample.quality.usableForEnrollment)
                    output.writeUTF(sample.quality.message)
                    output.writeUTF(sample.recordingPath.orEmpty())
                    output.writeUTF(sample.source)
                    output.writeInt(sample.embedding.size)
                    sample.embedding.forEach(output::writeFloat)
                }
            }
            output.flush()
            file.finishWrite(stream)
        } catch (t: Throwable) {
            file.failWrite(stream)
            throw t
        }
    }

    companion object {
        private const val FILE_NAME = "speaker_profiles_v1.bin"
        private const val MAGIC = 0x4253504B
        private const val VERSION = 1
        private const val MAX_PROFILES = 50
        private const val MAX_SAMPLES_PER_PROFILE = 200
        private const val MAX_EMBEDDING_DIMENSION = 4096
    }
}
