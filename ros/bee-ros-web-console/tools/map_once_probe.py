#!/usr/bin/env python3
import json
import sys
import time

import rclpy
from nav_msgs.msg import OccupancyGrid
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from rclpy.signals import SignalHandlerOptions


class MapProbe(Node):
    def __init__(self):
        super().__init__('bee_map_once_probe')
        self.message = None
        qos = QoSProfile(
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
        )
        self.create_subscription(OccupancyGrid, '/map', self._on_map, qos)

    def _on_map(self, message):
        self.message = message


def main():
    timeout = float(sys.argv[1]) if len(sys.argv) > 1 else 8.0
    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    node = MapProbe()
    deadline = time.monotonic() + timeout
    try:
        while node.message is None and time.monotonic() < deadline:
            rclpy.spin_once(node, timeout_sec=0.25)
        if node.message is None:
            raise SystemExit('map_timeout')
        message = node.message
        values = list(message.data)
        result = {
            'frame_id': message.header.frame_id,
            'width': message.info.width,
            'height': message.info.height,
            'resolution': message.info.resolution,
            'origin': {
                'x': message.info.origin.position.x,
                'y': message.info.origin.position.y,
            },
            'free': sum(1 for value in values if 0 <= value < 50),
            'occupied': sum(1 for value in values if value >= 50),
            'unknown': sum(1 for value in values if value < 0),
        }
        print(json.dumps(result, ensure_ascii=False, indent=2))
    finally:
        node.destroy_node()
        rclpy.try_shutdown()


if __name__ == '__main__':
    main()
