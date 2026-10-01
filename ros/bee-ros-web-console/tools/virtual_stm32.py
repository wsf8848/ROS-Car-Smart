#!/usr/bin/env python3
import json
import math
import os
import pty
import signal
import struct
import time
from pathlib import Path

from bee_uart5_protocol import (
    ACK, CMD_VEL, ESTOP, FLAG_COMMAND_TIMEOUT, FLAG_ESTOP, FLAG_MANUAL_OVERRIDE,
    HEARTBEAT, MODE_ROS, SET_MODE, SOURCE_BLUETOOTH, SOURCE_ROS, TELEMETRY,
    TELEMETRY_STRUCT, Decoder, encode_frame,
)

LINK = Path('/tmp/bee_uart5_sim')
CONTROL = Path('/tmp/bee_virtual_stm32_control.json')
STATUS = Path('/tmp/bee_virtual_stm32_status.json')


class VirtualSTM32:
    def __init__(self):
        self.master, slave = pty.openpty()
        os.set_blocking(self.master, False)
        slave_name = os.ttyname(slave)
        os.close(slave)
        LINK.unlink(missing_ok=True)
        LINK.symlink_to(slave_name)
        self.decoder = Decoder()
        self.running = True
        self.seq = 0
        self.telemetry_seq = 0
        self.ticks = [0, 0, 0, 0]
        self.wheels = [0.0] * 4
        self.linear = self.angular = 0.0
        self.mode = MODE_ROS
        self.source = SOURCE_ROS
        self.estop = False
        self.last_cmd = 0.0
        self.cmd_count = 0
        self.nonzero_cmd_count = 0
        self.last_nonzero = [0.0, 0.0]
        self.heartbeat_count = 0
        self.inject_bad_crc = False
        self.inject_seq_jump = False

    def command_control(self):
        try:
            command = json.loads(CONTROL.read_text())
            CONTROL.unlink(missing_ok=True)
        except (FileNotFoundError, json.JSONDecodeError):
            return
        action = command.get('action')
        if action == 'manual_override':
            self.source = SOURCE_BLUETOOTH
            self.linear = self.angular = 0.0
        elif action == 'ros_control':
            self.source = SOURCE_ROS
        elif action == 'bad_crc':
            self.inject_bad_crc = True
        elif action == 'seq_jump':
            self.inject_seq_jump = True

    def receive(self):
        try:
            data = os.read(self.master, 4096)
        except (BlockingIOError, OSError):
            return
        for msg_type, seq, timestamp_ms, payload in self.decoder.feed(data):
            self.seq = seq
            if msg_type == CMD_VEL and len(payload) == 9:
                linear, angular, requested_source = struct.unpack('<ffB', payload)
                self.cmd_count += 1
                if abs(linear) > 1e-4 or abs(angular) > 1e-4:
                    self.nonzero_cmd_count += 1
                    self.last_nonzero = [linear, angular]
                if self.source == SOURCE_ROS and requested_source == SOURCE_ROS and not self.estop:
                    self.linear = max(-0.8, min(0.8, linear))
                    self.angular = max(-2.0, min(2.0, angular))
                    self.last_cmd = time.monotonic()
            elif msg_type == HEARTBEAT:
                self.heartbeat_count += 1
            elif msg_type == SET_MODE and payload:
                self.mode = payload[0]
                if self.mode == MODE_ROS:
                    self.source = SOURCE_ROS
            elif msg_type == ESTOP and payload:
                self.estop = bool(payload[0])
                if self.estop:
                    self.linear = self.angular = 0.0
            ack = encode_frame(ACK, seq, self.millis(), struct.pack('<BB', msg_type, 0))
            os.write(self.master, ack)

    @staticmethod
    def millis():
        return int(time.monotonic() * 1000) & 0xFFFFFFFF

    def publish(self, dt):
        timeout = self.source == SOURCE_ROS and time.monotonic() - self.last_cmd > 0.35
        if timeout:
            self.linear = self.angular = 0.0
        left = self.linear - self.angular * 0.15
        right = self.linear + self.angular * 0.15
        self.wheels = [left, left, right, right]
        for i, speed in enumerate(self.wheels):
            self.ticks[i] += int(speed * dt * 1000)
        flags = (FLAG_COMMAND_TIMEOUT if timeout else 0) | (FLAG_ESTOP if self.estop else 0)
        if self.source != SOURCE_ROS:
            flags |= FLAG_MANUAL_OVERRIDE
        payload = TELEMETRY_STRUCT.pack(
            *self.ticks, *self.wheels,
            0.0, 0.0, 9.80665,
            0.0, 0.0, self.angular,
            31.5, 12.4, 0.78,
            self.source, flags, 0,
        )
        if self.inject_seq_jump:
            self.telemetry_seq = (self.telemetry_seq + 7) & 0xFFFF
            self.inject_seq_jump = False
        frame = encode_frame(TELEMETRY, self.telemetry_seq, self.millis(), payload)
        self.telemetry_seq = (self.telemetry_seq + 1) & 0xFFFF
        if self.inject_bad_crc:
            frame = frame[:-1] + bytes([frame[-1] ^ 0xFF])
            self.inject_bad_crc = False
        try:
            os.write(self.master, frame)
        except OSError:
            pass
        STATUS.write_text(json.dumps({
            'pty': str(LINK), 'linear': self.linear, 'angular': self.angular,
            'source': self.source, 'flags': flags, 'cmd_count': self.cmd_count,
            'nonzero_cmd_count': self.nonzero_cmd_count, 'last_nonzero': self.last_nonzero,
            'heartbeat_count': self.heartbeat_count, 'rx_crc_errors': self.decoder.crc_errors,
            'telemetry_seq': self.telemetry_seq,
        }))

    def run(self):
        previous = time.monotonic()
        next_publish = previous
        while self.running:
            now = time.monotonic()
            self.receive()
            self.command_control()
            if now >= next_publish:
                self.publish(min(0.1, now - previous))
                previous = now
                next_publish = now + 0.05
            time.sleep(0.002)

    def close(self):
        self.running = False
        LINK.unlink(missing_ok=True)
        os.close(self.master)


def main():
    sim = VirtualSTM32()
    signal.signal(signal.SIGTERM, lambda *_: setattr(sim, 'running', False))
    signal.signal(signal.SIGINT, lambda *_: setattr(sim, 'running', False))
    try:
        sim.run()
    finally:
        sim.close()


if __name__ == '__main__':
    main()
