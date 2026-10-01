#!/usr/bin/env python3
import json
import time

import rclpy
from geometry_msgs.msg import PoseArray, PoseStamped
from nav_msgs.msg import OccupancyGrid, Path
from rclpy.node import Node


def size(message):
    if isinstance(message, OccupancyGrid):
        return len(message.data)
    if isinstance(message, Path) or isinstance(message, PoseArray):
        return len(message.poses)
    return 1


def main():
    rclpy.init()
    node = Node('bee_sim_layers_audit')
    received = {}
    topics = {
        'map': (OccupancyGrid, '/map'),
        'robot': (PoseStamped, '/bee/sim/robot_pose'),
        'global_path': (Path, '/bee/sim/global_path'),
        'local_path': (Path, '/bee/sim/local_path'),
        'goal': (PoseStamped, '/bee/sim/goal'),
        'waypoints': (PoseArray, '/bee/sim/waypoints'),
        'costmap': (OccupancyGrid, '/bee/sim/costmap'),
        'trail': (Path, '/bee/sim/trail'),
        'obstacles': (PoseArray, '/bee/sim/obstacles'),
    }
    subscriptions = []
    for key, (message_type, topic) in topics.items():
        subscriptions.append(node.create_subscription(
            message_type, topic,
            lambda message, name=key: received.__setitem__(name, size(message)), 10,
        ))
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline and len(received) < len(topics):
        rclpy.spin_once(node, timeout_sec=0.1)
    print(json.dumps(received, sort_keys=True))
    node.destroy_node()
    rclpy.shutdown()
    if set(received) != set(topics):
        raise SystemExit(1)


if __name__ == '__main__':
    main()
