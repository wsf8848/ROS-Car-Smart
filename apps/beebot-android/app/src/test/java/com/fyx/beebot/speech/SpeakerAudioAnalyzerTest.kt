package com.fyx.beebot.speech

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import kotlin.math.sin

class SpeakerAudioAnalyzerTest {
    @Test
    fun `accepts a clean four second enrollment sample`() {
        val samples = FloatArray(16000 * 4) { index -> (0.12 * sin(index * 0.03)).toFloat() }

        val quality = SpeakerAudioAnalyzer.analyze(samples)

        assertTrue(quality.usableForRecognition)
        assertTrue(quality.usableForEnrollment)
    }

    @Test
    fun `rejects short or silent enrollment samples`() {
        val short = FloatArray(16000) { 0.1f }
        val silent = FloatArray(16000 * 4)

        assertFalse(SpeakerAudioAnalyzer.analyze(short).usableForEnrollment)
        assertFalse(SpeakerAudioAnalyzer.analyze(silent).usableForEnrollment)
    }

    @Test
    fun `trims leading and trailing silence`() {
        val samples = FloatArray(16000 * 4)
        for (index in 16000 until 16000 * 3) samples[index] = 0.1f

        val trimmed = SpeakerAudioAnalyzer.trimSilence(samples)

        assertTrue(trimmed.size < samples.size)
        assertTrue(trimmed.size >= 16000 * 2)
    }
}

