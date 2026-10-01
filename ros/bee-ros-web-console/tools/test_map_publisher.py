#!/usr/bin/env python3
import math
import signal
import threading

import rclpy
from nav_msgs.msg import OccupancyGrid
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from rclpy.signals import SignalHandlerOptions


class BeeTestMapPublisher(Node):
    def __init__(self):
        super().__init__('bee_test_map_publisher')
        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self.publisher = self.create_publisher(OccupancyGrid, '/map', qos)
        self.map = self.build_map()
        self.publish_map()

    def build_map(self):
        width, height = 48, 30
        grid = [-1] * (width * height)

        def set_cell(x, y, value):
            if 0 <= x < width and 0 <= y < height:
                grid[y * width + x] = value

        # 中央观测区：绿色可通行。外围保持深灰未知，便于验证三类栅格。
        for y in range(3, 27):
            for x in range(3, 45):
                set_cell(x, y, 0)

        # 红色边墙、隔断和孤立障碍。
        for x in range(3, 45):
            set_cell(x, 3, 100)
            set_cell(x, 26, 100)
        for y in range(3, 27):
            set_cell(3, y, 100)
            set_cell(44, y, 100)
        for y in range(7, 22):
            if y not in (13, 14):
                set_cell(16, y, 100)
        for x in range(25, 39):
            if x not in (31, 32):
                set_cell(x, 18, 100)
        for x, y in ((10, 9), (11, 9), (34, 10), (35, 10), (28, 23)):
            set_cell(x, y, 100)

        msg = OccupancyGrid()
        msg.header.frame_id = 'bee_test_map'
        msg.info.resolution = 0.20
        msg.info.width = width
        msg.info.height = height
        msg.info.origin.position.x = -4.0
        msg.info.origin.position.y = -2.5
        yaw = math.radians(10.0)
        msg.info.origin.orientation.z = math.sin(yaw / 2.0)
        msg.info.origin.orientation.w = math.cos(yaw / 2.0)
        msg.data = grid
        return msg

    def publish_map(self):
        now = self.get_clock().now().to_msg()
        self.map.header.stamp = now
        self.map.info.map_load_time = now
        self.publisher.publish(self.map)


def main():
    stop_event = threading.Event()
    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    node = BeeTestMapPublisher()
    signal.signal(signal.SIGINT, lambda _signum, _frame: stop_event.set())
    signal.signal(signal.SIGTERM, lambda _signum, _frame: stop_event.set())
    try:
        while not stop_event.is_set():
            rclpy.spin_once(node, timeout_sec=0.25)
    finally:
        node.destroy_node()
        if rclpy.ok():
            rclpy.shutdown()


if __name__ == '__main__':
    main()
