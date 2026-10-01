#!/usr/bin/env python3
"""Bee UART5 v1 framing shared by the RDK driver and STM32 simulator."""
import struct

MAGIC = b'\xAA\x55'
VERSION = 1
MAX_PAYLOAD = 512

CMD_VEL = 0x10
HEARTBEAT = 0x11
SET_MODE = 0x12
ESTOP = 0x13
TELEMETRY = 0x80
ACK = 0x90

SOURCE_NONE = 0
SOURCE_BLUETOOTH = 1
SOURCE_MQTT = 2
SOURCE_ROS = 3

MODE_MANUAL = 0
MODE_ROS = 1

FLAG_COMMAND_TIMEOUT = 1 << 0
FLAG_ESTOP = 1 << 1
FLAG_MANUAL_OVERRIDE = 1 << 2

HEADER = struct.Struct('<2sBBHHI')
TELEMETRY_STRUCT = struct.Struct('<4i4f3f3ffffBHH')


def crc16_ccitt(data, initial=0xFFFF):
    crc = initial
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def encode_frame(msg_type, seq, timestamp_ms, payload=b''):
    payload = bytes(payload)
    if len(payload) > MAX_PAYLOAD:
        raise ValueError('payload_too_large')
    body = struct.pack('<BBHHI', VERSION, msg_type, len(payload), seq & 0xFFFF, timestamp_ms & 0xFFFFFFFF) + payload
    return MAGIC + body + struct.pack('<H', crc16_ccitt(body))


class Decoder:
    def __init__(self):
        self.buffer = bytearray()
        self.crc_errors = 0
        self.version_errors = 0
        self.length_errors = 0

    def feed(self, data):
        self.buffer.extend(data)
        frames = []
        while True:
            start = self.buffer.find(MAGIC)
            if start < 0:
                if len(self.buffer) > 1:
                    del self.buffer[:-1]
                break
            if start:
                del self.buffer[:start]
            if len(self.buffer) < HEADER.size:
                break
            _, version, msg_type, length, seq, timestamp_ms = HEADER.unpack_from(self.buffer)
            if length > MAX_PAYLOAD:
                self.length_errors += 1
                del self.buffer[:2]
                continue
            total = HEADER.size + length + 2
            if len(self.buffer) < total:
                break
            raw = bytes(self.buffer[:total])
            del self.buffer[:total]
            if version != VERSION:
                self.version_errors += 1
                continue
            expected = struct.unpack_from('<H', raw, total - 2)[0]
            if crc16_ccitt(raw[2:-2]) != expected:
                self.crc_errors += 1
                continue
            frames.append((msg_type, seq, timestamp_ms, raw[HEADER.size:-2]))
        return frames
