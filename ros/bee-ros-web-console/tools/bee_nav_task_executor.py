#!/usr/bin/env python3
"""受控 Nav2 单点/巡逻任务执行器；生产默认锁定，仿真可用独立目录验证。"""

import json
import math
import os
import signal
import threading
from datetime import datetime, timezone
from pathlib import Path

import rclpy
from action_msgs.msg import GoalStatus
from geometry_msgs.msg import PoseStamped, Twist
from nav2_msgs.action import NavigateToPose
from rclpy.action import ActionClient
from rclpy.node import Node
from rclpy.signals import SignalHandlerOptions


COMMAND_FILE = Path(os.environ.get('BEE_TASK_COMMAND_FILE', '/run/bee-nav-task/command.json'))
STATUS_FILE = Path(os.environ.get('BEE_TASK_STATUS_FILE', '/run/bee-nav-task/status.json'))
VERIFICATION_FILE = Path(os.environ.get(
    'BEE_PHYSICAL_VERIFICATION_FILE',
    '/etc/bee-ros-web-console/physical-chassis-verified',
))
ALLOW_MOTION = os.environ.get('BEE_EXECUTOR_ALLOW_MOTION', '0') == '1'
NODE_NAME = os.environ.get('BEE_TASK_EXECUTOR_NODE', 'bee_nav_task_executor')


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def finite_number(value):
    return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(float(value))


class TaskExecutor(Node):
    def __init__(self):
        super().__init__(NODE_NAME)
        COMMAND_FILE.parent.mkdir(parents=True, exist_ok=True)
        STATUS_FILE.parent.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._client = ActionClient(self, NavigateToPose, '/navigate_to_pose')
        self._zero_pub = self.create_publisher(Twist, '/cmd_vel', 10)
        self._goal_handle = None
        self._after_cancel = None
        self._zero_remaining = 0
        self._plan = []
        self._repeat_total = 1
        self._repeat_index = 0
        self._waypoint_index = 0
        self._current_pose = None
        self._status = {
            'online': True,
            'motion_allowed': False,
            'state': 'locked',
            'action': None,
            'request_id': None,
            'progress_percent': 0,
            'repeat_index': 0,
            'repeat_total': 0,
            'waypoint_index': 0,
            'waypoint_total': 0,
            'distance_remaining': None,
            'last_error': None,
            'updated_at': utc_now(),
        }
        self.create_timer(0.10, self._poll_command)
        self.create_timer(0.25, self._safety_tick)
        self.create_timer(0.50, self._write_status)
        self._refresh_lock_state()
        self._write_status()

    def _motion_allowed(self):
        return ALLOW_MOTION and VERIFICATION_FILE.is_file()

    def _refresh_lock_state(self):
        allowed = self._motion_allowed()
        with self._lock:
            self._status['motion_allowed'] = allowed
            if not allowed and self._status['state'] in ('idle', 'completed', 'failed', 'canceled', 'locked'):
                self._status['state'] = 'locked'
            elif allowed and self._status['state'] == 'locked':
                self._status['state'] = 'idle'
            self._status['updated_at'] = utc_now()

    def _write_status(self):
        self._refresh_lock_state()
        with self._lock:
            payload = dict(self._status)
        temporary = STATUS_FILE.with_suffix('.tmp')
        temporary.write_text(json.dumps(payload, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
        temporary.replace(STATUS_FILE)

    def _set_status(self, **updates):
        with self._lock:
            self._status.update(updates)
            self._status['updated_at'] = utc_now()
        self._write_status()

    def _publish_zero(self):
        self._zero_remaining = max(self._zero_remaining, 5)

    def _safety_tick(self):
        if self._zero_remaining > 0:
            self._zero_pub.publish(Twist())
            self._zero_remaining -= 1
        if self._motion_allowed():
            return
        if self._goal_handle is not None and self._after_cancel is None:
            self._cancel_goal('locked', 'motion_interlock_opened')

    @staticmethod
    def _validate_pose(pose):
        if not isinstance(pose, dict) or set(pose) != {'x', 'y', 'yaw'}:
            raise ValueError('invalid_pose')
        if not all(finite_number(pose.get(key)) for key in ('x', 'y', 'yaw')):
            raise ValueError('invalid_pose')
        if abs(float(pose['x'])) > 100000 or abs(float(pose['y'])) > 100000 or abs(float(pose['yaw'])) > math.pi:
            raise ValueError('invalid_pose')
        return {key: float(pose[key]) for key in ('x', 'y', 'yaw')}

    def _poll_command(self):
        if not COMMAND_FILE.is_file():
            return
        try:
            request = json.loads(COMMAND_FILE.read_text(encoding='utf-8'))
        except (OSError, json.JSONDecodeError) as error:
            self._set_status(last_error=f'invalid_command:{error}')
            return
        finally:
            try:
                COMMAND_FILE.unlink()
            except FileNotFoundError:
                pass
        try:
            self._handle_command(request)
        except (ValueError, RuntimeError) as error:
            self._set_status(state='failed', last_error=str(error))
            self._publish_zero()

    def _handle_command(self, request):
        if not isinstance(request, dict) or not isinstance(request.get('action'), str):
            raise ValueError('invalid_command')
        action = request['action']
        request_id = request.get('request_id')
        if not self._motion_allowed():
            raise RuntimeError('motion_interlock_closed')
        self._set_status(request_id=request_id)
        if action in ('navigate', 'patrol_start') and self._goal_handle is not None:
            raise RuntimeError('task_already_running')
        if action == 'navigate':
            pose = self._validate_pose({key: request.get(key) for key in ('x', 'y', 'yaw')})
            self._plan = [pose]
            self._repeat_total = 1
            self._repeat_index = 0
            self._waypoint_index = 0
            self._set_status(
                state='starting', action='navigate', request_id=request_id,
                progress_percent=0, repeat_index=1, repeat_total=1,
                waypoint_index=1, waypoint_total=1, distance_remaining=None, last_error=None,
            )
            self._send_current_goal()
            return
        if action == 'patrol_start':
            points = request.get('waypoints')
            repeats = request.get('repeat_count')
            if not isinstance(points, list) or not 1 <= len(points) <= 100:
                raise ValueError('invalid_waypoints')
            if isinstance(repeats, bool) or not isinstance(repeats, int) or not 1 <= repeats <= 99:
                raise ValueError('invalid_repeat_count')
            self._plan = [self._validate_pose(point) for point in points]
            self._repeat_total = repeats
            self._repeat_index = 0
            self._waypoint_index = 0
            self._set_status(
                state='starting', action='patrol', request_id=request_id,
                progress_percent=0, repeat_index=1, repeat_total=repeats,
                waypoint_index=1, waypoint_total=len(self._plan), distance_remaining=None, last_error=None,
            )
            self._send_current_goal()
            return
        if action == 'pause':
            if self._status['state'] != 'running' or self._goal_handle is None:
                raise RuntimeError('task_not_running')
            self._cancel_goal('paused', None)
            return
        if action == 'resume':
            if self._status['state'] != 'paused' or self._goal_handle is not None:
                raise RuntimeError('task_not_paused')
            self._send_current_goal()
            return
        if action == 'retry':
            if self._status['state'] != 'failed' or not self._current_pose:
                raise RuntimeError('task_not_retryable')
            self._send_current_goal()
            return
        if action == 'skip':
            if self._status['action'] != 'patrol' or self._status['state'] not in ('failed', 'paused'):
                raise RuntimeError('task_not_skippable')
            self._advance_patrol()
            return
        if action in ('cancel', 'patrol_stop', 'takeover', 'estop'):
            if self._goal_handle is None:
                self._set_status(state='canceled', last_error=action)
                self._publish_zero()
            else:
                self._cancel_goal('canceled', action)
            return
        raise ValueError('unsupported_action')

    def _goal_message(self, pose):
        goal = NavigateToPose.Goal()
        goal.pose = PoseStamped()
        goal.pose.header.frame_id = 'map'
        goal.pose.header.stamp = self.get_clock().now().to_msg()
        goal.pose.pose.position.x = pose['x']
        goal.pose.pose.position.y = pose['y']
        goal.pose.pose.orientation.z = math.sin(pose['yaw'] / 2.0)
        goal.pose.pose.orientation.w = math.cos(pose['yaw'] / 2.0)
        return goal

    def _send_current_goal(self):
        if not self._plan:
            raise RuntimeError('empty_task')
        if not self._client.wait_for_server(timeout_sec=2.0):
            raise RuntimeError('navigate_to_pose_unavailable')
        self._current_pose = dict(self._plan[self._waypoint_index])
        self._after_cancel = None
        self._set_status(
            state='starting', distance_remaining=None, last_error=None,
            repeat_index=self._repeat_index + 1,
            waypoint_index=self._waypoint_index + 1,
        )
        future = self._client.send_goal_async(
            self._goal_message(self._current_pose),
            feedback_callback=self._feedback,
        )
        future.add_done_callback(self._goal_response)

    def _goal_response(self, future):
        handle = future.result()
        if handle is None or not handle.accepted:
            self._set_status(state='failed', last_error='goal_rejected')
            self._publish_zero()
            return
        self._goal_handle = handle
        self._set_status(state='running')
        result_future = handle.get_result_async()
        result_future.add_done_callback(self._goal_result)

    def _feedback(self, feedback):
        distance = float(feedback.feedback.distance_remaining)
        self._set_status(distance_remaining=round(distance, 3))

    def _goal_result(self, future):
        wrapped = future.result()
        status = wrapped.status if wrapped is not None else GoalStatus.STATUS_UNKNOWN
        self._goal_handle = None
        after_cancel = self._after_cancel
        self._after_cancel = None
        if after_cancel is not None:
            state, reason = after_cancel
            self._set_status(state=state, last_error=reason, distance_remaining=None)
            self._publish_zero()
            return
        if status == GoalStatus.STATUS_SUCCEEDED:
            if self._status['action'] == 'patrol':
                self._advance_patrol()
            else:
                self._set_status(state='completed', progress_percent=100, distance_remaining=0.0)
                self._publish_zero()
            return
        self._set_status(state='failed', last_error=f'nav2_status_{status}', distance_remaining=None)
        self._publish_zero()

    def _advance_patrol(self):
        completed = self._repeat_index * len(self._plan) + self._waypoint_index + 1
        total = self._repeat_total * len(self._plan)
        self._waypoint_index += 1
        if self._waypoint_index >= len(self._plan):
            self._waypoint_index = 0
            self._repeat_index += 1
        if self._repeat_index >= self._repeat_total:
            self._set_status(
                state='completed', progress_percent=100,
                repeat_index=self._repeat_total, waypoint_index=len(self._plan), distance_remaining=0.0,
            )
            self._publish_zero()
            return
        self._set_status(progress_percent=round(completed * 100.0 / total, 1))
        self._send_current_goal()

    def _cancel_goal(self, state, reason):
        self._after_cancel = (state, reason)
        self._set_status(state='canceling')
        future = self._goal_handle.cancel_goal_async()
        future.add_done_callback(lambda _future: None)

    def shutdown(self):
        if self._goal_handle is not None:
            try:
                self._goal_handle.cancel_goal_async()
            except Exception:
                pass
        for _ in range(5):
            self._zero_pub.publish(Twist())
        self._set_status(online=False, state='stopped')


def main():
    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    node = TaskExecutor()
    stopping = False

    def stop(_signum, _frame):
        nonlocal stopping
        if stopping:
            return
        stopping = True
        node.shutdown()
        rclpy.try_shutdown(context=node.context)

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        rclpy.spin(node)
    except (KeyboardInterrupt, rclpy.executors.ExternalShutdownException):
        pass
    finally:
        node.destroy_node()
        rclpy.try_shutdown(context=node.context)


if __name__ == '__main__':
    main()
