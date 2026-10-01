package com.fyx.beebot.speech

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class SpeakerMatcherTest {
    private val quality = AudioQuality(4000, -20f, 0.3f, 0f, 0.9f, 95, true, true, "合格")

    @Test
    fun `matches the closest enrolled speaker`() {
        val profiles = listOf(
            profile("a", "伍圣锋", floatArrayOf(1f, 0f, 0f), floatArrayOf(0.95f, 0.05f, 0f)),
            profile("b", "张三", floatArrayOf(0f, 1f, 0f), floatArrayOf(0.05f, 0.95f, 0f))
        )

        val result = SpeakerMatcher.match(profiles, floatArrayOf(0.98f, 0.02f, 0f))

        assertTrue(result.accepted)
        assertEquals("a", result.profileId)
        assertEquals("伍圣锋", result.name)
    }

    @Test
    fun `rejects an unknown embedding below threshold`() {
        val profiles = listOf(profile("a", "伍圣锋", floatArrayOf(1f, 0f, 0f)))

        val result = SpeakerMatcher.match(profiles, floatArrayOf(0f, 1f, 0f))

        assertFalse(result.accepted)
        assertEquals("未知说话人", result.name)
    }

    @Test
    fun `rejects ambiguous speakers without enough margin`() {
        val profiles = listOf(
            profile("a", "甲", floatArrayOf(1f, 0f)),
            profile("b", "乙", floatArrayOf(0.99f, 0.01f))
        )

        val result = SpeakerMatcher.match(profiles, floatArrayOf(1f, 0f))

        assertFalse(result.accepted)
    }

    private fun profile(id: String, name: String, vararg embeddings: FloatArray): SpeakerProfile =
        SpeakerProfile(
            id,
            name,
            embeddings.mapIndexed { index, embedding ->
                SpeakerVoiceSample("$id-$index", VoiceScene.QUIET_ROOM, VoiceDistance.NEAR, 4000, 1, quality, embedding)
            }
        )
}

