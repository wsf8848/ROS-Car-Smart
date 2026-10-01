#!/usr/bin/env python3
import math
import signal
import time

import rclpy
from rclpy.executors import ExternalShutdownException
from geometry_msgs.msg import Pose, PoseArray, PoseStamped
from nav_msgs.msg import OccupancyGrid, Path
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from rclpy.signals import SignalHandlerOptions


FRAME = 'bee_test_map'


def quaternion(yaw):
    pose = Pose()
    pose.orientation.z = math.sin(yaw / 2.0)
    pose.orientation.w = math.cos(yaw / 2.0)
    return pose.orientation


def pose(x, y, yaw=0.0):
    result = Pose()
    result.position.x = float(x)
    result.position.y = float(y)
    result.orientation = quaternion(yaw)
    return result


class SimLayersPublisher(Node):
    def __init__(self):
        super().__init__('bee_sim_layers_publisher')
        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self.map_pub = self.create_publisher(OccupancyGrid, '/map', qos)
        self.robot_pub = self.create_publisher(PoseStamped, '/bee/sim/robot_pose', qos)
        self.global_pub = self.create_publisher(Path, '/bee/sim/global_path', qos)
        self.local_pub = self.create_publisher(Path, '/bee/sim/local_path', qos)
        self.goal_pub = self.create_publisher(PoseStamped, '/bee/sim/goal', qos)
        self.waypoint_pub = self.create_publisher(PoseArray, '/bee/sim/waypoints', qos)
        self.cost_pub = self.create_publisher(OccupancyGrid, '/bee/sim/costmap', qos)
        self.trail_pub = self.create_publisher(Path, '/bee/sim/trail', qos)
        self.obstacle_pub = self.create_publisher(PoseArray, '/bee/sim/obstacles', qos)
        self.phase = 0.0
        self.timer = self.create_timer(0.2, self.publish_all)

    def stamp(self, message):
        message.header.stamp = self.get_clock().now().to_msg()
        message.header.frame_id = FRAME
        return message

    def make_map(self):
        width, height, resolution = 60, 40, 0.20
        message = self.stamp(OccupancyGrid())
        message.info.width = width
        message.info.height = height
        message.info.resolution = resolution
        message.info.origin.position.x = -6.0
        message.info.origin.position.y = -4.0
        message.info.origin.orientation.w = 1.0
        cells = [-1] * (width * height)
        for row in range(3, height - 3):
            for col in range(3, width - 3):
                cells[row * width + col] = 0
        for col in range(3, width - 3):
            cells[3 * width + col] = 100
            cells[(height - 4) * width + col] = 100
        for row in range(3, height - 3):
            cells[row * width + 3] = 100
            cells[row * width + width - 4] = 100
        for row in range(12, 25):
            for col in range(26, 30):
                cells[row * width + col] = 100
        message.data = cells
        return message

    def make_path(self, topic_points):
        message = self.stamp(Path())
        for x, y in topic_points:
            stamped = self.stamp(PoseStamped())
            stamped.pose = pose(x, y)
            message.poses.append(stamped)
        return message

    def make_costmap(self, robot_x, robot_y):
        width, height, resolution = 60, 40, 0.20
        message = self.stamp(OccupancyGrid())
        message.info.width = width
        message.info.height = height
        message.info.resolution = resolution
        message.info.origin.position.x = -6.0
        message.info.origin.position.y = -4.0
        message.info.origin.orientation.w = 1.0
        cells = [0] * (width * height)
        for ox, oy in ((-0.6, 0.9), (1.8, -0.7), (3.2, 1.4)):
            center_col = int((ox + 6.0) / resolution)
            center_row = int((oy + 4.0) / resolution)
            for row in range(max(0, center_row - 4), min(height, center_row + 5)):
                for col in range(max(0, center_col - 4), min(width, center_col + 5)):
                    distance = math.hypot(row - center_row, col - center_col)
                    if distance <= 2:
                        value = 100
                    elif distance <= 4:
                        value = max(1, int(80 - distance * 15))
                    else:
                        continue
                    cells[row * width + col] = max(cells[row * width + col], value)
        message.data = cells
        return message

    def publish_all(self):
        self.phase = (self.phase + 0.025) % 1.0
        robot_x = -3.8 + self.phase * 6.8
        robot_y = -1.7 + 0.45 * math.sin(self.phase * math.tau)
        robot_yaw = 0.10 * math.cos(self.phase * math.tau)
        global_points = [(-3.8, -1.7), (-2.5, -1.2), (-1.0, -0.8), (0.4, -0.2), (1.7, 0.5), (3.0, 1.2)]
        local_points = [(robot_x + index * 0.18, robot_y + 0.12 * math.sin(index / 2)) for index in range(12)]
        trail_points = [(-3.8 + index * max(self.phase, 0.03) * 6.8 / 25, -1.7 + 0.45 * math.sin(index / 25 * self.phase * math.tau)) for index in range(26)]

        robot = self.stamp(PoseStamped())
        robot.pose = pose(robot_x, robot_y, robot_yaw)
        goal = self.stamp(PoseStamped())
        goal.pose = pose(3.0, 1.2, 0.2)
        waypoints = self.stamp(PoseArray())
        waypoints.poses = [pose(-2.5, -1.2), pose(-1.0, -0.8), pose(1.7, 0.5), pose(3.0, 1.2)]
        obstacles = self.stamp(PoseArray())
        obstacles.poses = [pose(-0.6, 0.9), pose(1.8, -0.7), pose(3.2, 1.4)]

        self.map_pub.publish(self.make_map())
        self.robot_pub.publish(robot)
        self.global_pub.publish(self.make_path(global_points))
        self.local_pub.publish(self.make_path(local_points))
        self.goal_pub.publish(goal)
        self.waypoint_pub.publish(waypoints)
        self.cost_pub.publish(self.make_costmap(robot_x, robot_y))
        self.trail_pub.publish(self.make_path(trail_points))
        self.obstacle_pub.publish(obstacles)


def main():
    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    node = SimLayersPublisher()
    stopping = False

    def stop(_signum, _frame):
        nonlocal stopping
        if not stopping:
            stopping = True
            rclpy.try_shutdown(context=node.context)

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, ExternalShutdownException):
        pass
    finally:
        try:
            node.destroy_node()
        except Exception:
            pass
        rclpy.try_shutdown(context=node.context)


if __name__ == '__main__':
    main()
