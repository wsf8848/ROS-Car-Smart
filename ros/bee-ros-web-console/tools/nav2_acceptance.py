#!/usr/bin/env python3
import argparse
import math
import time

import rclpy
from geometry_msgs.msg import PoseStamped
from nav2_msgs.action import FollowWaypoints, NavigateToPose
from nav_msgs.msg import Odometry
from rclpy.action import ActionClient
from rclpy.node import Node


def pose(x, y, yaw=0.0):
    msg = PoseStamped()
    msg.header.frame_id = 'map'
    msg.pose.position.x = float(x)
    msg.pose.position.y = float(y)
    msg.pose.orientation.z = math.sin(yaw / 2.0)
    msg.pose.orientation.w = math.cos(yaw / 2.0)
    return msg


class Acceptance(Node):
    def __init__(self):
        super().__init__('bee_nav2_acceptance')
        self.nav = ActionClient(self, NavigateToPose, '/navigate_to_pose')
        self.waypoints = ActionClient(self, FollowWaypoints, '/follow_waypoints')
        self.latest_odom = None
        self.create_subscription(Odometry, '/odom', self._odom, 10)

    def _odom(self, msg):
        self.latest_odom = msg

    def wait(self, client, seconds=20.0):
        if not client.wait_for_server(timeout_sec=seconds):
            raise RuntimeError('action server unavailable')

    def send(self, client, goal, cancel_after=None, timeout=90.0):
        future = client.send_goal_async(goal)
        rclpy.spin_until_future_complete(self, future, timeout_sec=10.0)
        handle = future.result()
        if handle is None or not handle.accepted:
            return {'accepted': False, 'status': None}
        started = time.monotonic()
        result_future = handle.get_result_async()
        while rclpy.ok() and not result_future.done():
            rclpy.spin_once(self, timeout_sec=0.1)
            if cancel_after is not None and time.monotonic() - started >= cancel_after:
                cancel_future = handle.cancel_goal_async()
                rclpy.spin_until_future_complete(self, cancel_future, timeout_sec=5.0)
                cancel_after = None
            if time.monotonic() - started > timeout:
                raise TimeoutError('action timed out')
        wrapped = result_future.result()
        return {'accepted': True, 'status': wrapped.status, 'result': wrapped.result}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('case', choices=['single', 'cancel', 'waypoints', 'occupied'])
    args = parser.parse_args()
    rclpy.init()
    node = Acceptance()
    try:
        if args.case == 'waypoints':
            node.wait(node.waypoints)
            goal = FollowWaypoints.Goal()
            goal.poses = [pose(-3.2, -1.5), pose(-2.7, -1.0), pose(-3.4, -0.7)]
            result = node.send(node.waypoints, goal, timeout=150.0)
        else:
            node.wait(node.nav)
            goal = NavigateToPose.Goal()
            if args.case == 'single':
                goal.pose = pose(-2.5, -1.2)
                result = node.send(node.nav, goal, timeout=90.0)
            elif args.case == 'cancel':
                goal.pose = pose(2.5, 1.8)
                result = node.send(node.nav, goal, cancel_after=2.0, timeout=30.0)
            else:
                goal.pose = pose(-5.8, -3.8)
                result = node.send(node.nav, goal, timeout=30.0)
        stop_deadline = time.monotonic() + 1.0
        while time.monotonic() < stop_deadline:
            rclpy.spin_once(node, timeout_sec=0.05)
        odom = node.latest_odom
        if odom:
            result['odom'] = [round(odom.pose.pose.position.x, 3), round(odom.pose.pose.position.y, 3)]
            result['speed'] = [round(odom.twist.twist.linear.x, 3), round(odom.twist.twist.angular.z, 3)]
        printable = {k: v for k, v in result.items() if k != 'result'}
        print(printable, flush=True)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
