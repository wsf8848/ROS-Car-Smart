#!/usr/bin/env python3
import json
import math
import signal
import struct
import time

import rclpy
import serial
from geometry_msgs.msg import Twist
from sensor_msgs.msg import BatteryState, Imu, JointState
from std_msgs.msg import Int32MultiArray, String
from rclpy.node import Node
from rclpy.signals import SignalHandlerOptions

from bee_uart5_protocol import (
    CMD_VEL, HEARTBEAT, SOURCE_ROS, TELEMETRY, TELEMETRY_STRUCT,
    Decoder, encode_frame,
)


class BeeBaseDriver(Node):
    def __init__(self):
        super().__init__('bee_base_driver')
        self.declare_parameter('port', '/dev/ttyS5')
        self.declare_parameter('baud', 115200)
        self.port = self.get_parameter('port').value
        self.baud = int(self.get_parameter('baud').value)
        self.serial = None
        self.decoder = Decoder()
        self.tx_seq = 0
        self.last_rx_seq = None
        self.seq_gaps = 0
        self.reconnects = 0
        self.source = 0
        self.flags = 0
        self.last_rx = 0.0
        self.last_cmd = Twist()
        self.last_cmd_time = 0.0
        self.ticks_pub = self.create_publisher(Int32MultiArray, '/wheel_ticks', 10)
        self.joints_pub = self.create_publisher(JointState, '/wheel_speeds', 10)
        self.imu_pub = self.create_publisher(Imu, '/imu/data_raw', 10)
        self.battery_pub = self.create_publisher(BatteryState, '/battery_state', 10)
        self.status_pub = self.create_publisher(String, '/chassis/status', 10)
        self.create_subscription(Twist, '/cmd_vel', self.on_cmd, 10)
        self.create_timer(0.01, self.poll)
        self.create_timer(0.1, self.send_command)
        self.create_timer(0.25, self.send_heartbeat)

    def connect(self):
        if self.serial is not None:
            return True
        try:
            self.serial = serial.Serial(self.port, self.baud, timeout=0, write_timeout=0.1)
            self.reconnects += 1
            self.get_logger().info(f'UART5 connected: {self.port} @ {self.baud}')
            return True
        except (serial.SerialException, OSError):
            return False

    def disconnect(self):
        if self.serial is not None:
            try:
                self.serial.close()
            except serial.SerialException:
                pass
        self.serial = None

    def write(self, msg_type, payload=b''):
        if not self.connect():
            return False
        frame = encode_frame(msg_type, self.tx_seq, int(time.monotonic() * 1000), payload)
        self.tx_seq = (self.tx_seq + 1) & 0xFFFF
        try:
            self.serial.write(frame)
            return True
        except (serial.SerialException, OSError):
            self.disconnect()
            return False

    def on_cmd(self, msg):
        self.last_cmd = msg
        self.last_cmd_time = time.monotonic()

    def send_command(self):
        fresh = time.monotonic() - self.last_cmd_time <= 0.30
        allowed = self.source in (0, SOURCE_ROS)
        linear = float(self.last_cmd.linear.x) if fresh and allowed else 0.0
        angular = float(self.last_cmd.angular.z) if fresh and allowed else 0.0
        self.write(CMD_VEL, struct.pack('<ffB', linear, angular, SOURCE_ROS))

    def send_heartbeat(self):
        self.write(HEARTBEAT, bytes([SOURCE_ROS]))

    def poll(self):
        if not self.connect():
            return
        try:
            waiting = self.serial.in_waiting
            data = self.serial.read(waiting or 1)
        except (serial.SerialException, OSError):
            self.disconnect()
            return
        for msg_type, seq, timestamp_ms, payload in self.decoder.feed(data):
            if msg_type != TELEMETRY or len(payload) != TELEMETRY_STRUCT.size:
                continue
            if self.last_rx_seq is not None:
                expected = (self.last_rx_seq + 1) & 0xFFFF
                if seq != expected:
                    self.seq_gaps += (seq - expected) & 0xFFFF
            self.last_rx_seq = seq
            self.last_rx = time.monotonic()
            self.publish_telemetry(timestamp_ms, payload)

    def publish_telemetry(self, timestamp_ms, payload):
        values = TELEMETRY_STRUCT.unpack(payload)
        ticks = values[0:4]
        wheels = values[4:8]
        accel = values[8:11]
        gyro = values[11:14]
        temperature, voltage, percentage = values[14:17]
        self.source, self.flags, fault = values[17:20]
        stamp = self.get_clock().now().to_msg()
        ticks_msg = Int32MultiArray(data=list(ticks)); self.ticks_pub.publish(ticks_msg)
        joints = JointState(); joints.header.stamp = stamp
        joints.name = ['front_left', 'rear_left', 'front_right', 'rear_right']
        joints.velocity = list(wheels); self.joints_pub.publish(joints)
        imu = Imu(); imu.header.stamp = stamp; imu.header.frame_id = 'imu_link'
        imu.orientation_covariance[0] = -1.0
        imu.linear_acceleration.x, imu.linear_acceleration.y, imu.linear_acceleration.z = accel
        imu.angular_velocity.x, imu.angular_velocity.y, imu.angular_velocity.z = gyro
        self.imu_pub.publish(imu)
        battery = BatteryState(); battery.header.stamp = stamp; battery.voltage = voltage
        battery.percentage = percentage; battery.temperature = temperature
        self.battery_pub.publish(battery)
        status = {
            'protocol_version': 1, 'stm32_timestamp_ms': timestamp_ms,
            'control_source': self.source, 'state_flags': self.flags, 'fault': fault,
            'crc_errors': self.decoder.crc_errors, 'sequence_gaps': self.seq_gaps,
            'reconnects': self.reconnects, 'rx_age_ms': round((time.monotonic() - self.last_rx) * 1000, 1),
            'cmd_age_ms': None if self.last_cmd_time == 0.0 else round((time.monotonic() - self.last_cmd_time) * 1000, 1),
            'calibration_pending': ['wheel_radius', 'track_width', 'encoder_cpr', 'imu_mounting'],
        }
        self.status_pub.publish(String(data=json.dumps(status, separators=(',', ':'))))


def main():
    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    node = BeeBaseDriver()
    stopping = False

    def stop(_signum, _frame):
        nonlocal stopping
        if not stopping:
            stopping = True
            node.last_cmd = Twist()
            rclpy.try_shutdown(context=node.context)

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, rclpy.executors.ExternalShutdownException):
        pass
    except Exception:
        # Humble may surface InvalidHandle while the executor wakes during
        # an explicit context shutdown. Treat it as a normal exit only when
        # this process is already handling SIGINT/SIGTERM.
        if not stopping:
            raise
    finally:
        node.disconnect(); node.destroy_node(); rclpy.try_shutdown()


if __name__ == '__main__':
    main()
