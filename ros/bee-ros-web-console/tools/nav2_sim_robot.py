#!/usr/bin/env python3
import math
import signal
import rclpy
from geometry_msgs.msg import TransformStamped, Twist
from nav_msgs.msg import Odometry
from rclpy.node import Node
from rclpy.signals import SignalHandlerOptions
from sensor_msgs.msg import LaserScan
from tf2_ros import StaticTransformBroadcaster, TransformBroadcaster


class SimRobot(Node):
    def __init__(self):
        super().__init__('bee_nav2_sim_robot')
        self.x, self.y, self.yaw = -3.8, -1.7, 0.0
        self.v = self.w = 0.0
        self.last = self.get_clock().now()
        self.odom_pub = self.create_publisher(Odometry, '/odom', 20)
        self.scan_pub = self.create_publisher(LaserScan, '/bee/sim/scan', 10)
        self.create_subscription(Twist, '/cmd_vel', self.cmd, 20)
        self.tf = TransformBroadcaster(self)
        self.static_tf = StaticTransformBroadcaster(self)
        self.publish_static()
        self.create_timer(0.05, self.tick)

    @staticmethod
    def q(yaw):
        return math.sin(yaw / 2), math.cos(yaw / 2)

    def publish_static(self):
        transforms = []
        map_odom = TransformStamped()
        map_odom.header.stamp = self.get_clock().now().to_msg(); map_odom.header.frame_id = 'map'; map_odom.child_frame_id = 'odom'
        map_odom.transform.rotation.w = 1.0; transforms.append(map_odom)
        base_laser = TransformStamped()
        base_laser.header.stamp = self.get_clock().now().to_msg(); base_laser.header.frame_id = 'base_link'; base_laser.child_frame_id = 'laser'
        base_laser.transform.rotation.w = 1.0; transforms.append(base_laser)
        self.static_tf.sendTransform(transforms)

    def cmd(self, msg):
        self.v = max(-0.45, min(0.45, float(msg.linear.x)))
        self.w = max(-1.2, min(1.2, float(msg.angular.z)))

    def tick(self):
        now = self.get_clock().now(); dt = max(0.0, min(0.15, (now - self.last).nanoseconds / 1e9)); self.last = now
        self.x += self.v * math.cos(self.yaw) * dt; self.y += self.v * math.sin(self.yaw) * dt
        self.yaw = math.atan2(math.sin(self.yaw + self.w * dt), math.cos(self.yaw + self.w * dt))
        z, w = self.q(self.yaw)
        transform = TransformStamped(); transform.header.stamp = now.to_msg(); transform.header.frame_id = 'odom'; transform.child_frame_id = 'base_link'
        transform.transform.translation.x = self.x; transform.transform.translation.y = self.y; transform.transform.rotation.z = z; transform.transform.rotation.w = w
        self.tf.sendTransform(transform)
        odom = Odometry(); odom.header = transform.header; odom.child_frame_id = 'base_link'; odom.pose.pose.position.x = self.x; odom.pose.pose.position.y = self.y; odom.pose.pose.orientation.z = z; odom.pose.pose.orientation.w = w
        odom.twist.twist.linear.x = self.v; odom.twist.twist.angular.z = self.w; self.odom_pub.publish(odom)
        scan = LaserScan(); scan.header.stamp = now.to_msg(); scan.header.frame_id = 'laser'; scan.angle_min = -math.pi; scan.angle_max = math.pi; scan.angle_increment = math.pi / 180; scan.range_min = .15; scan.range_max = 8.0; scan.scan_time = .1
        scan.ranges = [float('inf')] * 361; scan.intensities = [0.0] * 361; self.scan_pub.publish(scan)


def main():
    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    node = SimRobot()
    stopping = False

    def stop(_signum, _frame):
        nonlocal stopping
        if not stopping:
            stopping = True
            node.v = node.w = 0.0
            rclpy.try_shutdown(context=node.context)

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, rclpy.executors.ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown(context=node.context)

if __name__ == '__main__': main()
