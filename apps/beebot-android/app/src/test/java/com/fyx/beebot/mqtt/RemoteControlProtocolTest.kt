package com.fyx.beebot.mqtt

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Test

class RemoteControlProtocolTest {
    @Test
    fun `parking frame matches ESP32 verified public-network sample`() {
        val expected = byteArrayOf(
            0x55, 0x43, 0x01, 0x42, 0x49,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            0xDA.toByte(), 0x5B
        )
        assertArrayEquals(expected, RemoteControlProtocol.encode(66, RemoteControlProtocol.STOP))
    }

    @Test
    fun `joystick maps all eight directions and neutral stop`() {
        assertEquals(0x41, RemoteControlProtocol.driveCommand(-1f, 0f))
        assertEquals(0x42, RemoteControlProtocol.driveCommand(-1f, 1f))
        assertEquals(0x43, RemoteControlProtocol.driveCommand(0f, 1f))
        assertEquals(0x44, RemoteControlProtocol.driveCommand(1f, 1f))
        assertEquals(0x45, RemoteControlProtocol.driveCommand(1f, 0f))
        assertEquals(0x46, RemoteControlProtocol.driveCommand(1f, -1f))
        assertEquals(0x47, RemoteControlProtocol.driveCommand(0f, -1f))
        assertEquals(0x48, RemoteControlProtocol.driveCommand(-1f, -1f))
        assertEquals(RemoteControlProtocol.STOP, RemoteControlProtocol.driveCommand(0.2f, -0.2f))
    }

    @Test
    fun `horn desired-state commands are accepted by encoder`() {
        assertEquals(RemoteControlProtocol.HORN_ON, RemoteControlProtocol.encode(1, RemoteControlProtocol.HORN_ON)[4].toInt() and 0xFF)
        assertEquals(RemoteControlProtocol.HORN_OFF, RemoteControlProtocol.encode(2, RemoteControlProtocol.HORN_OFF)[4].toInt() and 0xFF)
    }

    @Test
    fun `light effect commands are accepted by encoder`() {
        assertEquals(RemoteControlProtocol.LIGHT_OFF, RemoteControlProtocol.encode(3, RemoteControlProtocol.LIGHT_OFF)[4].toInt() and 0xFF)
        assertEquals(RemoteControlProtocol.LIGHT_WATER, RemoteControlProtocol.encode(4, RemoteControlProtocol.LIGHT_WATER)[4].toInt() and 0xFF)
        assertEquals(RemoteControlProtocol.LIGHT_FLASH, RemoteControlProtocol.encode(5, RemoteControlProtocol.LIGHT_FLASH)[4].toInt() and 0xFF)
    }
}
