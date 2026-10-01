#!/usr/bin/env python3
import asyncio
import base64
import hashlib
import hmac
import json
import math
import os
import signal
import secrets
import subprocess
import threading
import time
import uuid
import zlib
from collections import deque
from datetime import datetime, timezone
from array import array
from http.cookies import CookieError, SimpleCookie
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

import rclpy
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy, qos_profile_sensor_data
from rclpy.signals import SignalHandlerOptions
from rclpy.executors import ExternalShutdownException
from sensor_msgs.msg import LaserScan
from geometry_msgs.msg import PoseArray, PoseStamped
from nav_msgs.msg import OccupancyGrid, Path as NavPath
from websockets.asyncio.server import serve


WEB_ROOT = Path(os.environ.get('BEE_WEB_ROOT', '/opt/bee-ros-web-console/frontend'))
HTTP_PORT = int(os.environ.get('BEE_HTTP_PORT', '8080'))
WS_PORT = int(os.environ.get('BEE_WS_PORT', '8765'))
BAGS_ROOT = Path(os.environ.get('BEE_BAGS_ROOT', '/opt/bee-ros-web-console/data/rosbags'))
MAPS_ROOT = Path(os.environ.get('BEE_MAPS_ROOT', '/opt/bee-ros-web-console/data/maps'))
PATROLS_ROOT = Path(os.environ.get('BEE_PATROLS_ROOT', '/opt/bee-ros-web-console/data/patrol_tasks'))
PHYSICAL_VERIFICATION_FILE = Path(os.environ.get(
    'BEE_PHYSICAL_VERIFICATION_FILE',
    '/etc/bee-ros-web-console/physical-chassis-verified',
))
TASK_COMMAND_FILE = Path(os.environ.get('BEE_TASK_COMMAND_FILE', '/run/bee-nav-task/command.json'))
TASK_STATUS_FILE = Path(os.environ.get('BEE_TASK_STATUS_FILE', '/run/bee-nav-task/status.json'))
MOTION_ENABLE_REQUESTED = os.environ.get('BEE_MOTION_ENABLE', '0') == '1'
STM32_READINESS_REPORT = Path(os.environ.get(
    'BEE_STM32_READINESS_REPORT',
    '/opt/bee-ros-web-console/data/stm32-arrival-readiness.json',
))
DELIVERY_AUDIT_REPORT = Path(os.environ.get(
    'BEE_DELIVERY_AUDIT_REPORT',
    '/opt/bee-ros-web-console/data/delivery-audit.json',
))
TIME_GUARD_STATUS = Path(os.environ.get('BEE_TIME_GUARD_STATUS', '/run/bee-time-guard/status.json'))
TIME_SYNC_HELPER = os.environ.get('BEE_TIME_SYNC_HELPER', '/usr/local/sbin/bee-time-sync')
BROWSER_TIME_SYNC_ENABLED = os.environ.get('BEE_BROWSER_TIME_SYNC', '1') == '1'
CAMERA_DEVICE = os.environ.get(
    'BEE_CAMERA_DEVICE',
    '/dev/v4l/by-id/usb-Generic_Integrated_Webcam_200901010001-video-index0',
)
CAMERA_OUTPUT_WIDTH = int(os.environ.get('BEE_CAMERA_OUTPUT_WIDTH', '960'))
CAMERA_OUTPUT_HEIGHT = int(os.environ.get('BEE_CAMERA_OUTPUT_HEIGHT', '540'))
CAMERA_OUTPUT_FPS = int(os.environ.get('BEE_CAMERA_OUTPUT_FPS', '15'))
POWER_DRY_RUN = os.environ.get('BEE_POWER_DRY_RUN', '0') == '1'
POWER_ACTIONS = {
    'reboot': ['/usr/bin/sudo', '-n', '/usr/bin/systemctl', 'reboot'],
    'poweroff': ['/usr/bin/sudo', '-n', '/usr/bin/systemctl', 'poweroff'],
}
ADMIN_USERNAME = os.environ.get('BEE_ADMIN_USERNAME', 'FYX')
ADMIN_PASSWORD = os.environ.get('BEE_ADMIN_PASSWORD', '')
ADMIN_PASSWORD_HASH = os.environ.get('BEE_ADMIN_PASSWORD_HASH', '')
SESSION_SECRET = os.environ.get('BEE_SESSION_SECRET', '')
COOKIE_SECURE = os.environ.get('BEE_COOKIE_SECURE', '0') == '1'
SESSION_COOKIE = 'bee_session'
SESSION_TTL = 60 * 60 * 12
REMEMBER_TTL = 60 * 60 * 24 * 30
LOGIN_WINDOW = 60 * 5
LOGIN_MAX_FAILURES = 5
LOGIN_LOCKOUT = 60 * 5
_login_attempts = {}
_login_lock = threading.Lock()
_time_sync_lock = threading.Lock()
_last_time_sync_request = 0.0
_power_lock = threading.Lock()
_power_pending = False
_bridge_instance = None


class SystemHealthMonitor:
    """低开销缓存板卡健康数据，避免每个网页请求启动外部进程。"""

    def __init__(self, interval_seconds=1.0):
        self._interval = max(0.5, float(interval_seconds))
        self._lock = threading.Lock()
        self._previous_cpu = None
        self._snapshot = {
            'ok': False,
            'timestamp': None,
            'cpu': {'usage_percent': None, 'temperature_c': None, 'thermal_zone': None},
            'memory': {'used_bytes': None, 'total_bytes': None, 'available_bytes': None, 'usage_percent': None},
            'disk': {'mount': '/', 'used_bytes': None, 'total_bytes': None, 'available_bytes': None, 'usage_percent': None},
            'uptime_seconds': None,
            'time': {'healthy': False, 'system_utc': None, 'rtc_utc': None, 'rtc_delta_seconds': None, 'source': 'unknown'},
            'errors': ['initializing'],
        }
        self._thread = threading.Thread(target=self._run, name='bee-system-health', daemon=True)
        self._thread.start()

    @staticmethod
    def _read_cpu_counters():
        fields = Path('/proc/stat').read_text(encoding='ascii').splitlines()[0].split()
        if not fields or fields[0] != 'cpu' or len(fields) < 5:
            raise ValueError('invalid_proc_stat')
        values = [int(value) for value in fields[1:]]
        idle = values[3] + (values[4] if len(values) > 4 else 0)
        return sum(values), idle

    @staticmethod
    def _read_temperature():
        candidates = []
        for zone in Path('/sys/class/thermal').glob('thermal_zone*'):
            try:
                zone_type = (zone / 'type').read_text(encoding='utf-8').strip()
                raw = float((zone / 'temp').read_text(encoding='ascii').strip())
                temperature = raw / 1000.0 if abs(raw) >= 1000 else raw
                if not math.isfinite(temperature) or temperature < -40 or temperature > 200:
                    continue
                normalized = zone_type.lower()
                priority = 0 if 'cpu' in normalized else 1 if 'soc' in normalized else 2
                candidates.append((priority, str(zone), zone_type, temperature))
            except (OSError, ValueError):
                continue
        if not candidates:
            raise FileNotFoundError('thermal_zone_unavailable')
        _, zone_path, zone_type, temperature = min(candidates, key=lambda item: (item[0], item[1]))
        return round(temperature, 1), {'path': zone_path, 'type': zone_type}

    @staticmethod
    def _read_memory():
        values = {}
        for line in Path('/proc/meminfo').read_text(encoding='ascii').splitlines():
            key, _, remainder = line.partition(':')
            if key in ('MemTotal', 'MemAvailable'):
                values[key] = int(remainder.strip().split()[0]) * 1024
        total = values.get('MemTotal')
        available = values.get('MemAvailable')
        if not total or available is None:
            raise ValueError('invalid_meminfo')
        used = max(0, total - available)
        return {
            'used_bytes': used,
            'total_bytes': total,
            'available_bytes': available,
            'usage_percent': round(used * 100.0 / total, 1),
        }

    @staticmethod
    def _read_disk():
        stats = os.statvfs('/')
        fragment_size = stats.f_frsize or stats.f_bsize
        total = stats.f_blocks * fragment_size
        free = stats.f_bfree * fragment_size
        available = stats.f_bavail * fragment_size
        used = max(0, total - free)
        usable_total = used + available
        return {
            'mount': '/',
            'used_bytes': used,
            'total_bytes': total,
            'available_bytes': available,
            'usage_percent': round(used * 100.0 / usable_total, 1) if usable_total else None,
        }

    @staticmethod
    def _read_uptime():
        return round(float(Path('/proc/uptime').read_text(encoding='ascii').split()[0]), 1)

    @staticmethod
    def _read_time_health():
        now = datetime.now(timezone.utc)
        result = {
            'healthy': False,
            'system_utc': now.isoformat(),
            'rtc_utc': None,
            'rtc_delta_seconds': None,
            'source': 'unknown',
            'checked_at_utc': None,
            'check_age_seconds': None,
        }
        if not TIME_GUARD_STATUS.is_file():
            raise FileNotFoundError('time_guard_status_unavailable')
        status = json.loads(TIME_GUARD_STATUS.read_text(encoding='utf-8'))
        checked_epoch = float(status.get('checked_epoch'))
        check_age = abs(now.timestamp() - checked_epoch)
        delta = float(status.get('rtc_delta_seconds'))
        result.update({
            'healthy': bool(status.get('healthy')) and check_age <= 300.0 and abs(delta) <= 5.0,
            'rtc_utc': status.get('rtc_utc'),
            'rtc_delta_seconds': round(delta, 3),
            'source': str(status.get('source') or 'unknown'),
            'checked_at_utc': status.get('checked_at_utc'),
            'check_age_seconds': round(check_age, 1),
        })
        return result

    def _sample(self):
        errors = []
        cpu_usage = None
        try:
            counters = self._read_cpu_counters()
            if self._previous_cpu is not None:
                total_delta = counters[0] - self._previous_cpu[0]
                idle_delta = counters[1] - self._previous_cpu[1]
                if total_delta > 0:
                    cpu_usage = round(max(0.0, min(100.0, (total_delta - idle_delta) * 100.0 / total_delta)), 1)
            self._previous_cpu = counters
        except (OSError, ValueError, IndexError) as error:
            errors.append(f'cpu:{error}')

        temperature = None
        thermal_zone = None
        try:
            temperature, thermal_zone = self._read_temperature()
        except (OSError, ValueError) as error:
            errors.append(f'temperature:{error}')

        try:
            memory = self._read_memory()
        except (OSError, ValueError) as error:
            memory = {'used_bytes': None, 'total_bytes': None, 'available_bytes': None, 'usage_percent': None}
            errors.append(f'memory:{error}')

        try:
            disk = self._read_disk()
        except OSError as error:
            disk = {'mount': '/', 'used_bytes': None, 'total_bytes': None, 'available_bytes': None, 'usage_percent': None}
            errors.append(f'disk:{error}')

        try:
            uptime = self._read_uptime()
        except (OSError, ValueError, IndexError) as error:
            uptime = None
            errors.append(f'uptime:{error}')

        try:
            time_health = self._read_time_health()
            if not time_health['healthy']:
                errors.append('time:unhealthy')
        except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError) as error:
            time_health = {
                'healthy': False,
                'system_utc': datetime.now(timezone.utc).isoformat(),
                'rtc_utc': None,
                'rtc_delta_seconds': None,
                'source': 'unknown',
                'checked_at_utc': None,
                'check_age_seconds': None,
            }
            errors.append(f'time:{error}')

        snapshot = {
            'ok': cpu_usage is not None and memory['usage_percent'] is not None and disk['usage_percent'] is not None and time_health['healthy'],
            'timestamp': datetime.now(timezone.utc).isoformat(),
            'cpu': {
                'usage_percent': cpu_usage,
                'temperature_c': temperature,
                'thermal_zone': thermal_zone,
            },
            'memory': memory,
            'disk': disk,
            'uptime_seconds': uptime,
            'time': time_health,
            'errors': errors,
        }
        with self._lock:
            self._snapshot = snapshot

    def _run(self):
        while True:
            started = time.monotonic()
            self._sample()
            time.sleep(max(0.1, self._interval - (time.monotonic() - started)))

    def snapshot(self):
        with self._lock:
            return json.loads(json.dumps(self._snapshot))


system_health_monitor = SystemHealthMonitor()


def _password_matches(password):
    if ADMIN_PASSWORD_HASH:
        try:
            encoded = base64.b64decode(ADMIN_PASSWORD_HASH.encode('ascii'), validate=True)
            salt, expected = encoded[:16], encoded[16:]
            actual = hashlib.pbkdf2_hmac('sha256', password.encode('utf-8'), salt, 180000)
            return hmac.compare_digest(actual, expected)
        except (ValueError, TypeError):
            return False
    return bool(ADMIN_PASSWORD) and hmac.compare_digest(password, ADMIN_PASSWORD)


def _session_signing_key():
    if SESSION_SECRET:
        return SESSION_SECRET.encode('utf-8')
    # Development fallback: sessions intentionally expire after a process restart.
    if not hasattr(_session_signing_key, '_ephemeral'):
        _session_signing_key._ephemeral = secrets.token_bytes(32)
        print('BEE_AUTH_AUDIT warning=ephemeral_session_secret', flush=True)
    return _session_signing_key._ephemeral


def _b64url_encode(value):
    return base64.urlsafe_b64encode(value).rstrip(b'=').decode('ascii')


def _b64url_decode(value):
    return base64.urlsafe_b64decode(value + '=' * (-len(value) % 4))


def _create_session(remember):
    expires = int(time.time()) + (REMEMBER_TTL if remember else SESSION_TTL)
    payload = json.dumps(
        {'u': ADMIN_USERNAME, 'exp': expires, 'r': bool(remember), 'n': secrets.token_urlsafe(12)},
        separators=(',', ':'),
    ).encode('utf-8')
    encoded = _b64url_encode(payload)
    signature = hmac.new(_session_signing_key(), encoded.encode('ascii'), hashlib.sha256).digest()
    return f'{encoded}.{_b64url_encode(signature)}'


def _session_valid(token):
    if not token:
        return False
    try:
        encoded, supplied_signature = token.split('.', 1)
        expected_signature = hmac.new(
            _session_signing_key(), encoded.encode('ascii'), hashlib.sha256
        ).digest()
        if not hmac.compare_digest(_b64url_decode(supplied_signature), expected_signature):
            return False
        payload = json.loads(_b64url_decode(encoded).decode('utf-8'))
        return payload.get('u') == ADMIN_USERNAME and int(payload.get('exp', 0)) > int(time.time())
    except (ValueError, TypeError, UnicodeDecodeError, json.JSONDecodeError):
        return False


def _cookie_token(cookie_header):
    try:
        cookies = SimpleCookie()
        cookies.load(cookie_header or '')
        morsel = cookies.get(SESSION_COOKIE)
        return morsel.value if morsel else ''
    except (CookieError, AttributeError):
        return ''


def _login_allowed(client_ip):
    now = time.time()
    with _login_lock:
        state = _login_attempts.get(client_ip, {'failures': [], 'locked_until': 0})
        if state['locked_until'] > now:
            return False, max(1, int(state['locked_until'] - now))
        state['failures'] = [stamp for stamp in state['failures'] if now - stamp < LOGIN_WINDOW]
        state['locked_until'] = 0
        _login_attempts[client_ip] = state
        return True, 0


def _record_login_failure(client_ip):
    now = time.time()
    with _login_lock:
        state = _login_attempts.setdefault(client_ip, {'failures': [], 'locked_until': 0})
        state['failures'] = [stamp for stamp in state['failures'] if now - stamp < LOGIN_WINDOW]
        state['failures'].append(now)
        if len(state['failures']) >= LOGIN_MAX_FAILURES:
            state['locked_until'] = now + LOGIN_LOCKOUT
            return LOGIN_LOCKOUT
        return 0


def _clear_login_failures(client_ip):
    with _login_lock:
        _login_attempts.pop(client_ip, None)


class BagManager:
    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._record_process = None
        self._record_id = None
        self._play_process = None
        self._play_id = None

    @staticmethod
    def _stop_process(process):
        if process is None or process.poll() is not None:
            return
        try:
            os.killpg(process.pid, signal.SIGINT)
            process.wait(timeout=10)
        except (ProcessLookupError, subprocess.TimeoutExpired):
            try:
                os.killpg(process.pid, signal.SIGTERM)
                process.wait(timeout=5)
            except (ProcessLookupError, subprocess.TimeoutExpired):
                process.kill()
                process.wait(timeout=3)

    def _refresh_locked(self):
        if self._record_process is not None and self._record_process.poll() is not None:
            self._record_process = None
            self._record_id = None
        if self._play_process is not None and self._play_process.poll() is not None:
            self._play_process = None
            self._play_id = None

    def status(self):
        with self._lock:
            self._refresh_locked()
            return {
                'recording': self._record_process is not None,
                'record_id': self._record_id,
                'playing': self._play_process is not None,
                'play_id': self._play_id,
            }

    def list_bags(self):
        result = []
        for path in sorted(self.root.iterdir(), reverse=True):
            if not path.is_dir() or not (path / 'metadata.yaml').exists():
                continue
            total = sum(item.stat().st_size for item in path.rglob('*') if item.is_file())
            result.append({'id': path.name, 'bytes': total, 'metadata': True})
        return result[:50]

    def start_recording(self):
        with self._lock:
            self._refresh_locked()
            if self._record_process is not None:
                raise RuntimeError('recording_active')
            bag_id = f'scan_{uuid.uuid4().hex[:10]}'
            output = self.root / bag_id
            command = [
                '/opt/ros/humble/bin/ros2', 'bag', 'record',
                '--storage', 'sqlite3', '--output', str(output), '/scan',
            ]
            self._record_process = subprocess.Popen(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            self._record_id = bag_id
            return bag_id

    def stop_recording(self):
        with self._lock:
            self._refresh_locked()
            process = self._record_process
            bag_id = self._record_id
            self._record_process = None
            self._record_id = None
        self._stop_process(process)
        return bag_id

    def start_playback(self, bag_id):
        if not bag_id or Path(bag_id).name != bag_id:
            raise ValueError('invalid_bag_id')
        bag_path = self.root / bag_id
        if not (bag_path / 'metadata.yaml').is_file():
            raise FileNotFoundError('bag_not_found')
        with self._lock:
            self._refresh_locked()
            if self._play_process is not None:
                raise RuntimeError('playback_active')
            command = [
                '/opt/ros/humble/bin/ros2', 'bag', 'play', str(bag_path),
                '--loop', '--disable-keyboard-controls',
                '--remap', '/scan:=/bee/replay/scan',
            ]
            self._play_process = subprocess.Popen(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            self._play_id = bag_id
            return bag_id

    def stop_playback(self):
        with self._lock:
            self._refresh_locked()
            process = self._play_process
            bag_id = self._play_id
            self._play_process = None
            self._play_id = None
        self._stop_process(process)
        return bag_id

    def shutdown(self):
        self.stop_recording()
        self.stop_playback()


bag_manager = BagManager(BAGS_ROOT)


class MapManager:
    def __init__(self, root):
        self.root = Path(root)
        self.root.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._save_process = None
        self._save_id = None
        self._load_process = None
        self._load_id = None

    def status(self):
        with self._lock:
            if self._save_process is not None and self._save_process.poll() is not None:
                self._save_process = None
                self._save_id = None
            if self._load_process is not None and self._load_process.poll() is not None:
                self._load_process = None
                self._load_id = None
            return {
                'saving': self._save_process is not None,
                'save_id': self._save_id,
                'loaded': self._load_process is not None,
                'load_id': self._load_id,
            }

    def list_maps(self):
        items = []
        for yaml_path in sorted(self.root.glob('*.yaml'), reverse=True):
            image_path = yaml_path.with_suffix('.pgm')
            metadata_path = yaml_path.with_suffix('.json')
            metadata = {}
            if metadata_path.is_file():
                try:
                    candidate = json.loads(metadata_path.read_text(encoding='utf-8'))
                    if isinstance(candidate, dict):
                        metadata = candidate
                except (OSError, ValueError, UnicodeError):
                    metadata = {}
            items.append({
                'id': yaml_path.stem,
                'name': str(metadata.get('name') or yaml_path.stem),
                'site': str(metadata.get('site') or '未填写场地'),
                'created_at': str(metadata.get('created_at') or datetime.fromtimestamp(yaml_path.stat().st_mtime, timezone.utc).isoformat()),
                'yaml_bytes': yaml_path.stat().st_size,
                'image_bytes': image_path.stat().st_size if image_path.exists() else 0,
                'complete': image_path.exists(),
            })
        return items[:50]

    @staticmethod
    def _metadata_text(value, field, fallback):
        if value is None:
            value = fallback
        if not isinstance(value, str):
            raise ValueError(f'invalid_map_{field}')
        value = value.strip()
        if not value or len(value) > 80 or any(ord(char) < 32 for char in value):
            raise ValueError(f'invalid_map_{field}')
        return value

    def start_save(self, name=None, site=None):
        with self._lock:
            if self._save_process is not None and self._save_process.poll() is not None:
                self._save_process = None
                self._save_id = None
            if self._save_process is not None:
                raise RuntimeError('map_save_active')
            map_id = f'map_{uuid.uuid4().hex[:10]}'
            map_name = self._metadata_text(name, 'name', map_id)
            map_site = self._metadata_text(site, 'site', '未填写场地')
            prefix = self.root / map_id
            prefix.with_suffix('.json').write_text(json.dumps({'id': map_id, 'name': map_name, 'site': map_site, 'created_at': datetime.now(timezone.utc).isoformat(), 'format': 'nav2_map_server_yaml_pgm'}, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
            command = [
                '/opt/ros/humble/bin/ros2', 'run', 'nav2_map_server', 'map_saver_cli',
                '-t', '/map', '-f', str(prefix), '--fmt', 'pgm', '--mode', 'trinary',
            ]
            self._save_process = subprocess.Popen(
                command,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            self._save_id = map_id
            return map_id

    def load(self, map_id):
        if not map_id or Path(map_id).name != map_id:
            raise ValueError('invalid_map_id')
        yaml_path = self.root / f'{map_id}.yaml'
        if not yaml_path.is_file() or not yaml_path.with_suffix('.pgm').is_file():
            raise FileNotFoundError('map_not_found')
        with self._lock:
            if self._load_process is not None and self._load_process.poll() is None:
                raise RuntimeError('map_already_loaded')
            command = [
                '/opt/ros/humble/bin/ros2', 'run', 'nav2_map_server', 'map_server',
                '--ros-args', '-r', '__node:=bee_managed_map_server',
                '-p', f'yaml_filename:={yaml_path}',
            ]
            process = subprocess.Popen(
                command, stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, start_new_session=True,
            )
            self._load_process = process
            self._load_id = map_id
        time.sleep(1.2)
        if process.poll() is not None:
            with self._lock:
                self._load_process = None
                self._load_id = None
            raise RuntimeError('map_server_start_failed')
        environment = os.environ.copy()
        for transition in ('configure', 'activate'):
            result = subprocess.run(
                ['/opt/ros/humble/bin/ros2', 'lifecycle', 'set', '--no-daemon', '/bee_managed_map_server', transition],
                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=8, env=environment,
            )
            if result.returncode != 0 or 'successful' not in result.stdout.lower():
                self.unload()
                raise RuntimeError(f'map_{transition}_failed')
        return map_id

    def unload(self):
        with self._lock:
            process = self._load_process
            map_id = self._load_id
            self._load_process = None
            self._load_id = None
        BagManager._stop_process(process)
        return map_id

    def shutdown(self):
        with self._lock:
            process = self._save_process
            self._save_process = None
            self._save_id = None
        BagManager._stop_process(process)
        self.unload()


map_manager = MapManager(MAPS_ROOT)


class PatrolDraftManager:
    """Persist validated patrol drafts without granting vehicle motion authority."""

    def __init__(self, root, maps_root):
        self.root = Path(root)
        self.maps_root = Path(maps_root)
        self.root.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()

    @staticmethod
    def _text(value, field):
        if not isinstance(value, str):
            raise ValueError(f'invalid_patrol_{field}')
        value = value.strip()
        if not value or len(value) > 80 or any(ord(char) < 32 for char in value):
            raise ValueError(f'invalid_patrol_{field}')
        return value

    @staticmethod
    def _map_id(value):
        if not isinstance(value, str) or not value or Path(value).name != value or len(value) > 100:
            raise ValueError('invalid_map_id')
        return value

    @staticmethod
    def _repeat_count(value):
        if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= 99:
            raise ValueError('invalid_repeat_count')
        return value

    @staticmethod
    def _waypoints(value):
        if not isinstance(value, list) or not 1 <= len(value) <= 100:
            raise ValueError('invalid_waypoints')
        validated = []
        for point in value:
            if not isinstance(point, dict) or set(point) != {'x', 'y', 'yaw'}:
                raise ValueError('invalid_waypoints')
            if not all(PatrolDraftManager._finite_number(point.get(key)) for key in ('x', 'y', 'yaw')):
                raise ValueError('invalid_waypoints')
            if abs(float(point['x'])) > 100000 or abs(float(point['y'])) > 100000 or abs(float(point['yaw'])) > math.pi:
                raise ValueError('invalid_waypoints')
            validated.append({key: float(point[key]) for key in ('x', 'y', 'yaw')})
        return validated

    @staticmethod
    def _finite_number(value):
        return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(float(value))

    def _assert_map_exists(self, map_id):
        if not (self.maps_root / f'{map_id}.yaml').is_file() or not (self.maps_root / f'{map_id}.pgm').is_file():
            raise ValueError('patrol_map_not_found')

    def list(self, map_id=None):
        requested_map = self._map_id(map_id) if map_id is not None else None
        items = []
        for path in sorted(self.root.glob('*.json'), reverse=True):
            try:
                item = json.loads(path.read_text(encoding='utf-8'))
                if not isinstance(item, dict) or item.get('id') != path.stem:
                    continue
                if requested_map is not None and item.get('map_id') != requested_map:
                    continue
                items.append(item)
            except (OSError, ValueError, UnicodeError):
                continue
        return items[:100]

    def save(self, request):
        name = self._text(request.get('name'), 'name')
        map_id = self._map_id(request.get('map_id'))
        self._assert_map_exists(map_id)
        repeat_count = self._repeat_count(request.get('repeat_count'))
        waypoints = self._waypoints(request.get('waypoints'))
        draft_id = request.get('id')
        if draft_id is None:
            draft_id = f'patrol_{uuid.uuid4().hex[:10]}'
        if not isinstance(draft_id, str) or Path(draft_id).name != draft_id or not draft_id.startswith('patrol_'):
            raise ValueError('invalid_patrol_id')
        now = datetime.now(timezone.utc).isoformat()
        path = self.root / f'{draft_id}.json'
        created_at = now
        if path.is_file():
            try:
                previous = json.loads(path.read_text(encoding='utf-8'))
                if previous.get('map_id') != map_id:
                    raise ValueError('patrol_map_mismatch')
                created_at = str(previous.get('created_at') or now)
            except (OSError, UnicodeError, json.JSONDecodeError):
                pass
        item = {
            'id': draft_id, 'name': name, 'map_id': map_id,
            'repeat_count': repeat_count, 'waypoints': waypoints,
            'created_at': created_at, 'updated_at': now,
        }
        temporary = path.with_suffix('.json.tmp')
        with self._lock:
            temporary.write_text(json.dumps(item, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
            temporary.replace(path)
        return item

    def delete(self, draft_id):
        if not isinstance(draft_id, str) or Path(draft_id).name != draft_id or not draft_id.startswith('patrol_'):
            raise ValueError('invalid_patrol_id')
        path = self.root / f'{draft_id}.json'
        with self._lock:
            if not path.is_file():
                raise FileNotFoundError('patrol_not_found')
            path.unlink()
        return draft_id


patrol_draft_manager = PatrolDraftManager(PATROLS_ROOT, MAPS_ROOT)


class TaskExecutorManager:
    SERVICE = 'bee-nav-task-executor.service'

    @staticmethod
    def _service_active():
        result = subprocess.run(
            ['/usr/bin/systemctl', 'is-active', TaskExecutorManager.SERVICE],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=3,
        )
        return result.stdout.strip() == 'active'

    def status(self):
        payload = None
        try:
            payload = json.loads(TASK_STATUS_FILE.read_text(encoding='utf-8'))
            timestamp = datetime.fromisoformat(str(payload.get('updated_at', '')).replace('Z', '+00:00'))
            age = max(0.0, (datetime.now(timezone.utc) - timestamp.astimezone(timezone.utc)).total_seconds())
            if age > 3.0:
                payload = None
        except (OSError, ValueError, TypeError, json.JSONDecodeError):
            payload = None
        return {
            'service_active': self._service_active(),
            'online': bool(payload and payload.get('online')),
            'status': payload,
        }

    def submit(self, request):
        TASK_COMMAND_FILE.parent.mkdir(parents=True, exist_ok=True)
        if TASK_COMMAND_FILE.exists() and time.time() - TASK_COMMAND_FILE.stat().st_mtime < 2.0:
            raise RuntimeError('task_executor_busy')
        request_id = uuid.uuid4().hex
        payload = dict(request)
        payload['request_id'] = request_id
        temporary = TASK_COMMAND_FILE.with_suffix('.tmp')
        temporary.write_text(json.dumps(payload, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
        temporary.replace(TASK_COMMAND_FILE)
        return request_id


task_executor_manager = TaskExecutorManager()


class ExperimentalSlamManager:
    SERVICE = 'bee-experimental-slam.service'

    @staticmethod
    def _systemctl(*arguments, timeout=15):
        return subprocess.run(
            ['/usr/bin/sudo', '-n', '/usr/bin/systemctl', *arguments],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
        )

    def status(self):
        result = subprocess.run(
            ['/usr/bin/systemctl', 'is-active', self.SERVICE],
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=4,
        )
        state = result.stdout.strip() or 'unknown'
        return {
            'active': state == 'active',
            'state': state,
            'experimental': True,
            'motion_enabled': False,
        }

    def action(self, action):
        if action not in ('start', 'stop'):
            raise ValueError('unsupported_action')
        before = self.status()
        if action == 'start' and before['active']:
            raise RuntimeError('experimental_slam_already_active')
        if action == 'stop' and not before['active']:
            return before
        result = self._systemctl(action, self.SERVICE, timeout=20)
        if result.returncode != 0:
            raise RuntimeError(f'experimental_slam_{action}_failed')
        deadline = time.monotonic() + 12
        expected = action == 'start'
        while time.monotonic() < deadline:
            current = self.status()
            if current['active'] == expected:
                return current
            time.sleep(0.25)
        raise RuntimeError(f'experimental_slam_{action}_timeout')


experimental_slam_manager = ExperimentalSlamManager()


class TaskSafetyManager:
    """Fail-closed task contract used before the physical chassis is available."""

    ACTION_KEYS = {
        'navigate': {'action', 'map_id', 'x', 'y', 'yaw'},
        'patrol_start': {'action', 'map_id', 'waypoints', 'repeat_count'},
        'pause': {'action'},
        'resume': {'action'},
        'cancel': {'action'},
        'patrol_stop': {'action'},
        'retry': {'action'},
        'skip': {'action'},
        'takeover': {'action'},
        'estop': {'action'},
    }

    def __init__(self):
        self._lock = threading.Lock()
        self._last_request = None

    @staticmethod
    def _service_active(name):
        result = subprocess.run(
            ['/usr/bin/systemctl', 'is-active', name],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=3,
        )
        return result.stdout.strip() == 'active'

    def status(self):
        chassis = self._service_active('bee-base-driver.service')
        nav2 = self._service_active('bee-nav2-stack.service')
        loaded_map = map_manager.status().get('loaded', False)
        scan_online = False
        if _bridge_instance is not None:
            try:
                scan_online = bool(_bridge_instance.diagnostics_snapshot().get('online'))
            except Exception:
                scan_online = False
        physical_verified = PHYSICAL_VERIFICATION_FILE.is_file()
        executor = task_executor_manager.status()
        executor_ready = bool(executor['service_active'] and executor['online'])
        prerequisites = {
            'physical_chassis_verified': physical_verified,
            'chassis_driver': chassis,
            'laser_scan': scan_online,
            'map_loaded': loaded_map,
            'nav2_stack': nav2,
            'task_executor': executor_ready,
        }
        reason_keys = {
            'physical_chassis_verified': 'physical_chassis_not_verified',
            'chassis_driver': 'chassis_driver_offline',
            'laser_scan': 'laser_scan_offline',
            'map_loaded': 'map_not_loaded',
            'nav2_stack': 'nav2_stack_offline',
            'task_executor': 'task_executor_not_ready',
        }
        failed_prerequisite = next((key for key, ready in prerequisites.items() if not ready), None)
        motion_enabled = bool(
            all(prerequisites.values())
            and MOTION_ENABLE_REQUESTED
            and executor.get('status', {}).get('motion_allowed')
        )
        reason = reason_keys.get(failed_prerequisite) if failed_prerequisite else None
        if reason is None and not motion_enabled:
            reason = 'motion_interlock_closed'
        executor_status = executor.get('status') or {}
        executor_state = str(executor_status.get('state') or 'locked')
        allowed_actions = []
        if motion_enabled:
            if executor_state == 'running':
                allowed_actions = ['pause', 'cancel', 'patrol_stop', 'takeover', 'estop']
            elif executor_state == 'paused':
                allowed_actions = ['resume', 'cancel', 'patrol_stop', 'skip', 'takeover', 'estop']
            elif executor_state == 'failed':
                allowed_actions = ['retry', 'cancel', 'skip', 'takeover', 'estop']
            else:
                allowed_actions = ['navigate', 'patrol_start', 'takeover', 'estop']
        with self._lock:
            last_request = None if self._last_request is None else dict(self._last_request)
        return {
            'ready': all(prerequisites.values()),
            'chassis': chassis,
            'nav2': nav2,
            'map_loaded': loaded_map,
            'scan_online': scan_online,
            'physical_verified': physical_verified,
            'executor_ready': executor_ready,
            'motion_enabled': motion_enabled,
            'motion_enable_requested': MOTION_ENABLE_REQUESTED,
            'state': executor_state if executor_ready else 'locked',
            'phase': 'executing' if executor_state in ('starting', 'running', 'canceling') else 'waiting_prerequisites',
            'progress_percent': executor_status.get('progress_percent', 0),
            'reason': reason or 'motion_interlock_closed',
            'allowed_actions': allowed_actions,
            'prerequisites': prerequisites,
            'ready_count': sum(1 for ready in prerequisites.values() if ready),
            'required_count': len(prerequisites),
            'last_request': last_request,
            'executor': executor,
        }

    @staticmethod
    def _finite(value):
        return not isinstance(value, bool) and isinstance(value, (int, float)) and math.isfinite(float(value))

    def validate(self, request):
        if not isinstance(request, dict):
            raise ValueError('invalid_request')
        action = request.get('action')
        expected = self.ACTION_KEYS.get(action)
        if expected is None or set(request) != expected:
            raise ValueError('unsupported_action')
        if action == 'navigate':
            if not isinstance(request['map_id'], str) or Path(request['map_id']).name != request['map_id']:
                raise ValueError('invalid_map_id')
            if not all(self._finite(request[key]) for key in ('x', 'y', 'yaw')):
                raise ValueError('invalid_pose')
            if abs(float(request['x'])) > 100000 or abs(float(request['y'])) > 100000 or abs(float(request['yaw'])) > math.pi:
                raise ValueError('invalid_pose')
        if action == 'patrol_start':
            waypoints = request.get('waypoints')
            if not isinstance(request['map_id'], str) or Path(request['map_id']).name != request['map_id']:
                raise ValueError('invalid_map_id')
            if not isinstance(waypoints, list) or not 1 <= len(waypoints) <= 100:
                raise ValueError('invalid_waypoints')
            for point in waypoints:
                if not isinstance(point, dict) or set(point) != {'x', 'y', 'yaw'}:
                    raise ValueError('invalid_waypoints')
                if not all(self._finite(point[key]) for key in ('x', 'y', 'yaw')):
                    raise ValueError('invalid_waypoints')
                if abs(float(point['x'])) > 100000 or abs(float(point['y'])) > 100000 or abs(float(point['yaw'])) > math.pi:
                    raise ValueError('invalid_waypoints')
            repeat_count = request.get('repeat_count')
            if isinstance(repeat_count, bool) or not isinstance(repeat_count, int) or not 1 <= repeat_count <= 99:
                raise ValueError('invalid_repeat_count')
        return action

    def execute(self, request):
        action = self.validate(request)
        status = self.status()
        if not status['motion_enabled'] or action not in status['allowed_actions']:
            with self._lock:
                self._last_request = {
                    'action': action,
                    'accepted': False,
                    'reason': status['reason'],
                    'timestamp': datetime.now(timezone.utc).isoformat(),
                }
            raise RuntimeError(f'task_rejected:{action}:{status["reason"]}')
        request_id = task_executor_manager.submit(request)
        with self._lock:
            self._last_request = {
                'action': action,
                'accepted': True,
                'request_id': request_id,
                'reason': None,
                'timestamp': datetime.now(timezone.utc).isoformat(),
            }
        return {'request_id': request_id, 'action': action}


task_safety_manager = TaskSafetyManager()


def execute_power_action(action):
    global _power_pending
    try:
        if POWER_DRY_RUN:
            print(f'BEE_POWER_AUDIT dry_run action={action}', flush=True)
            return
        print(f'BEE_POWER_AUDIT execute action={action}', flush=True)
        subprocess.run(POWER_ACTIONS[action], check=True, timeout=15)
    except Exception as error:
        print(f'BEE_POWER_AUDIT failed action={action} error={error}', flush=True)
    finally:
        with _power_lock:
            _power_pending = False


class CameraManager:
    def __init__(self, device):
        self.device = device
        self._condition = threading.Condition()
        self._clients = 0
        self._thread = None
        self._process = None
        self._latest_frame = None
        self._sequence = 0
        self._frame_times = deque()
        self._online = False
        self._error = None
        self._shutdown = False

    def _command(self):
        return [
            '/usr/bin/ffmpeg', '-nostdin', '-hide_banner', '-loglevel', 'error',
            '-f', 'v4l2', '-input_format', 'mjpeg', '-video_size', '1280x720',
            '-framerate', '30', '-i', self.device,
            '-vf', f'fps={CAMERA_OUTPUT_FPS},scale={CAMERA_OUTPUT_WIDTH}:{CAMERA_OUTPUT_HEIGHT}',
            '-q:v', '6', '-an', '-f', 'image2pipe', '-vcodec', 'mjpeg', 'pipe:1',
        ]

    def add_client(self):
        with self._condition:
            if self._shutdown:
                raise RuntimeError('camera_shutting_down')
            self._clients += 1
            if self._thread is None or not self._thread.is_alive():
                self._latest_frame = None
                self._online = False
                self._error = None
                self._thread = threading.Thread(target=self._run, name='bee-camera', daemon=True)
                self._thread.start()

    def remove_client(self):
        process = None
        with self._condition:
            self._clients = max(0, self._clients - 1)
            if self._clients == 0:
                self._latest_frame = None
                self._online = False
                process = self._process
                self._condition.notify_all()
        if process is not None and process.poll() is None:
            process.terminate()

    def _publish(self, frame):
        now = time.monotonic()
        with self._condition:
            if self._shutdown or self._clients == 0:
                return
            self._latest_frame = frame
            self._sequence += 1
            self._online = True
            self._error = None
            self._frame_times.append(now)
            while self._frame_times and now - self._frame_times[0] > 2.0:
                self._frame_times.popleft()
            self._condition.notify_all()

    def _run(self):
        while True:
            with self._condition:
                if self._shutdown or self._clients == 0:
                    self._thread = None
                    return
            process = None
            try:
                process = subprocess.Popen(
                    self._command(), stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                    bufsize=0, start_new_session=True,
                )
                with self._condition:
                    self._process = process
                buffer = bytearray()
                while True:
                    chunk = process.stdout.read(65536)
                    if not chunk:
                        break
                    buffer.extend(chunk)
                    while True:
                        start = buffer.find(b'\xff\xd8')
                        if start < 0:
                            if len(buffer) > 2:
                                del buffer[:-2]
                            break
                        end = buffer.find(b'\xff\xd9', start + 2)
                        if end < 0:
                            if start > 0:
                                del buffer[:start]
                            break
                        self._publish(bytes(buffer[start:end + 2]))
                        del buffer[:end + 2]
            except Exception as error:
                with self._condition:
                    self._error = f'{type(error).__name__}: {error}'
            finally:
                if process is not None and process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=2)
                with self._condition:
                    self._process = None
                    self._online = False
                    should_retry = not self._shutdown and self._clients > 0
                    if should_retry and self._error is None:
                        self._error = 'camera_stream_restarting'
                    self._condition.notify_all()
            if not should_retry:
                with self._condition:
                    self._thread = None
                return
            time.sleep(1.0)

    def wait_frame(self, previous_sequence, timeout=3.0):
        deadline = time.monotonic() + timeout
        with self._condition:
            while (
                not self._shutdown
                and (self._latest_frame is None or self._sequence == previous_sequence)
            ):
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None, previous_sequence
                self._condition.wait(remaining)
            if self._shutdown or self._latest_frame is None:
                return None, previous_sequence
            return self._latest_frame, self._sequence

    def status(self):
        with self._condition:
            now = time.monotonic()
            frame_age = None if not self._frame_times else round((now - self._frame_times[-1]) * 1000)
            measured_fps = 0.0
            if len(self._frame_times) > 1:
                measured_fps = (len(self._frame_times) - 1) / (self._frame_times[-1] - self._frame_times[0])
            return {
                'model': 'WHEELTEC C70',
                'sensor': 'JX-H62',
                'device': self.device,
                'present': Path(self.device).exists(),
                'online': self._online and frame_age is not None and frame_age <= 1500,
                'clients': self._clients,
                'source': {'width': 1280, 'height': 720, 'fps': 30, 'format': 'MJPEG'},
                'output': {
                    'width': CAMERA_OUTPUT_WIDTH, 'height': CAMERA_OUTPUT_HEIGHT,
                    'fps': CAMERA_OUTPUT_FPS, 'format': 'MJPEG',
                },
                'measured_fps': round(measured_fps, 1),
                'last_frame_age_ms': frame_age,
                'error': self._error,
            }

    def shutdown(self):
        process = None
        thread = None
        with self._condition:
            self._shutdown = True
            self._clients = 0
            process = self._process
            thread = self._thread
            self._condition.notify_all()
        if process is not None and process.poll() is None:
            process.terminate()
        if thread is not None:
            thread.join(timeout=3)


camera_manager = CameraManager(CAMERA_DEVICE)


class QuietHandler(SimpleHTTPRequestHandler):
    PUBLIC_PATHS = {
        '/login.html', '/login.css', '/login.js', '/bee-ros-console.svg',
        '/favicon.ico', '/api/auth/status', '/api/auth/login',
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(WEB_ROOT), **kwargs)

    def log_message(self, fmt, *args):
        return

    def end_headers(self):
        request_path = self.path.split('?', 1)[0]
        if request_path in ('/', '/index.html', '/login.html'):
            # 首页必须每次校验，避免新增按钮存在于HTML但浏览器仍运行旧脚本。
            self.send_header('Cache-Control', 'no-store, max-age=0')
            self.send_header('Pragma', 'no-cache')
            self.send_header('Expires', '0')
        elif request_path.endswith(('.js', '.css', '.svg')):
            # 静态资源URL带发布版本号，可长期缓存且不会混用不同发布版本。
            self.send_header('Cache-Control', 'public, max-age=31536000, immutable')
        self.send_header('X-Content-Type-Options', 'nosniff')
        self.send_header('X-Frame-Options', 'DENY')
        self.send_header('Referrer-Policy', 'no-referrer')
        self.send_header('Permissions-Policy', 'camera=(), microphone=(), geolocation=()')
        self.send_header(
            'Content-Security-Policy',
            "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline'; "
            "img-src 'self' data:; connect-src 'self' ws: wss:; object-src 'none'; "
            "base-uri 'none'; frame-ancestors 'none'; form-action 'self'",
        )
        super().end_headers()

    def _send_json(self, status, payload, headers=None):
        body = json.dumps(payload, separators=(',', ':'), ensure_ascii=False).encode('utf-8')
        self.send_response(status)
        self.send_header('Content-Type', 'application/json; charset=utf-8')
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        for name, value in headers or ():
            self.send_header(name, value)
        self.end_headers()
        self.wfile.write(body)

    def _redirect(self, location):
        self.send_response(302)
        self.send_header('Location', location)
        self.send_header('Cache-Control', 'no-store')
        self.send_header('Content-Length', '0')
        self.end_headers()

    def _authenticated(self):
        return _session_valid(_cookie_token(self.headers.get('Cookie', '')))

    def _require_auth(self, request_path):
        if self._authenticated():
            return True
        if request_path.startswith('/api/'):
            self._send_json(401, {'ok': False, 'error': 'authentication_required'})
        else:
            self._redirect('/login.html')
        return False

    @staticmethod
    def _session_cookie(token, remember):
        parts = [f'{SESSION_COOKIE}={token}', 'Path=/', 'HttpOnly', 'SameSite=Strict']
        if remember:
            parts.append(f'Max-Age={REMEMBER_TTL}')
        if COOKIE_SECURE:
            parts.append('Secure')
        return '; '.join(parts)

    @staticmethod
    def _expired_session_cookie():
        parts = [f'{SESSION_COOKIE}=', 'Path=/', 'HttpOnly', 'SameSite=Strict', 'Max-Age=0']
        if COOKIE_SECURE:
            parts.append('Secure')
        return '; '.join(parts)

    def _origin_allowed(self):
        host = self.headers.get('Host', '')
        origin = self.headers.get('Origin', '')
        return bool(origin and origin in (f'http://{host}', f'https://{host}'))

    def _read_json(self):
        if self.headers.get('Content-Type', '').split(';', 1)[0].strip() != 'application/json':
            raise TypeError('json_required')
        try:
            length = int(self.headers.get('Content-Length', '0'))
        except ValueError:
            length = 0
        if length <= 0 or length > 512:
            raise OverflowError('invalid_size')
        try:
            return json.loads(self.rfile.read(length).decode('utf-8'))
        except (UnicodeDecodeError, json.JSONDecodeError) as error:
            raise ValueError('invalid_json') from error

    def do_GET(self):
        request_path = self.path.split('?', 1)[0]
        if request_path == '/api/auth/status':
            authenticated = self._authenticated()
            self._send_json(200, {
                'ok': True,
                'authenticated': authenticated,
                'username': ADMIN_USERNAME if authenticated else None,
            })
            return
        if request_path == '/login.html' and self._authenticated():
            self._redirect('/')
            return
        if request_path in self.PUBLIC_PATHS:
            super().do_GET()
            return
        if not self._require_auth(request_path):
            return
        if request_path == '/api/camera/status':
            self._send_json(200, {'ok': True, **camera_manager.status()})
            return
        if request_path == '/api/camera/stream.mjpg':
            self._serve_camera_stream()
            return
        if request_path == '/api/diagnostics':
            diagnostics = _bridge_instance.diagnostics_snapshot() if _bridge_instance else {'online': False}
            diagnostics['bags'] = bag_manager.status()
            self._send_json(200, diagnostics)
            return
        if request_path == '/api/bags':
            self._send_json(200, {'items': bag_manager.list_bags(), 'status': bag_manager.status()})
            return
        if request_path == '/api/maps':
            self._send_json(200, {
                'items': map_manager.list_maps(),
                'status': map_manager.status(),
                'slam': experimental_slam_manager.status(),
            })
            return
        if request_path == '/api/slam':
            self._send_json(200, experimental_slam_manager.status())
            return
        if request_path == '/api/tasks':
            self._send_json(200, task_safety_manager.status())
            return
        if request_path == '/api/patrols':
            query = self.path.split('?', 1)[1] if '?' in self.path else ''
            map_id = None
            for part in query.split('&'):
                if part.startswith('map_id='):
                    from urllib.parse import unquote_plus
                    map_id = unquote_plus(part.split('=', 1)[1])
            try:
                items = patrol_draft_manager.list(map_id)
            except ValueError as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            self._send_json(200, {'ok': True, 'items': items})
            return
        if request_path == '/api/health':
            diagnostics = _bridge_instance.diagnostics_snapshot() if _bridge_instance else {'online': False}
            self._send_json(200, {
                'ok': True,
                'web': True,
                'scan_online': bool(diagnostics.get('online')),
                'slam': experimental_slam_manager.status(),
                'tasks': task_safety_manager.status(),
            })
            return
        if request_path == '/api/system/health':
            self._send_json(200, system_health_monitor.snapshot())
            return
        if request_path == '/api/commissioning':
            try:
                report = json.loads(STM32_READINESS_REPORT.read_text(encoding='utf-8'))
            except FileNotFoundError:
                self._send_json(404, {'ok': False, 'error': 'readiness_report_not_found'})
                return
            except (OSError, json.JSONDecodeError, ValueError) as error:
                self._send_json(500, {'ok': False, 'error': f'readiness_report_invalid:{error}'})
                return
            delivery_audit = None
            try:
                delivery_audit = json.loads(DELIVERY_AUDIT_REPORT.read_text(encoding='utf-8'))
            except (FileNotFoundError, OSError, json.JSONDecodeError, ValueError):
                pass
            self._send_json(200, {'ok': True, 'report': report, 'delivery_audit': delivery_audit})
            return
        super().do_GET()

    def _serve_camera_stream(self):
        try:
            camera_manager.add_client()
        except RuntimeError as error:
            self._send_json(503, {'ok': False, 'error': str(error)})
            return
        sequence = -1
        try:
            frame, sequence = camera_manager.wait_frame(sequence, timeout=6.0)
            if frame is None:
                self._send_json(503, {'ok': False, 'error': 'camera_frame_timeout'})
                return
            self.send_response(200)
            self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=bee-frame')
            self.send_header('Cache-Control', 'no-store, no-cache, must-revalidate')
            self.send_header('Pragma', 'no-cache')
            self.send_header('Connection', 'close')
            self.end_headers()
            while True:
                self.wfile.write(b'--bee-frame\r\n')
                self.wfile.write(b'Content-Type: image/jpeg\r\n')
                self.wfile.write(f'Content-Length: {len(frame)}\r\n\r\n'.encode('ascii'))
                self.wfile.write(frame)
                self.wfile.write(b'\r\n')
                self.wfile.flush()
                frame, sequence = camera_manager.wait_frame(sequence, timeout=3.0)
                if frame is None:
                    break
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass
        finally:
            camera_manager.remove_client()

    def do_POST(self):
        global _power_pending, _last_time_sync_request
        request_path = self.path.split('?', 1)[0]
        if request_path == '/api/auth/login':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            allowed, retry_after = _login_allowed(self.client_address[0])
            if not allowed:
                print(f'BEE_AUTH_AUDIT action=login result=rate_limited client={self.client_address[0]}', flush=True)
                self._send_json(
                    429,
                    {'ok': False, 'error': 'too_many_attempts', 'retry_after': retry_after},
                    [('Retry-After', str(retry_after))],
                )
                return
            if not ADMIN_PASSWORD and not ADMIN_PASSWORD_HASH:
                print('BEE_AUTH_AUDIT action=login result=not_configured', flush=True)
                self._send_json(503, {'ok': False, 'error': 'authentication_not_configured'})
                return
            try:
                request = self._read_json()
            except TypeError as error:
                self._send_json(415, {'ok': False, 'error': str(error)})
                return
            except OverflowError as error:
                self._send_json(413, {'ok': False, 'error': str(error)})
                return
            except ValueError as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            expected_keys = {'username', 'password', 'remember'}
            valid_shape = (
                isinstance(request, dict)
                and set(request) == expected_keys
                and isinstance(request.get('username'), str)
                and isinstance(request.get('password'), str)
                and isinstance(request.get('remember'), bool)
                and len(request['username']) <= 64
                and len(request['password']) <= 256
            )
            valid_credentials = (
                valid_shape
                and hmac.compare_digest(request['username'], ADMIN_USERNAME)
                and _password_matches(request['password'])
            )
            if not valid_credentials:
                lockout = _record_login_failure(self.client_address[0])
                print(
                    f'BEE_AUTH_AUDIT action=login result=denied client={self.client_address[0]} locked={bool(lockout)}',
                    flush=True,
                )
                self._send_json(401, {'ok': False, 'error': 'invalid_credentials'})
                return
            _clear_login_failures(self.client_address[0])
            remember = request['remember']
            token = _create_session(remember)
            print(f'BEE_AUTH_AUDIT action=login result=accepted client={self.client_address[0]} remember={remember}', flush=True)
            self._send_json(
                200,
                {'ok': True, 'username': ADMIN_USERNAME},
                [('Set-Cookie', self._session_cookie(token, remember))],
            )
            return

        if request_path == '/api/auth/logout':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            print(f'BEE_AUTH_AUDIT action=logout client={self.client_address[0]}', flush=True)
            self._send_json(
                200,
                {'ok': True},
                [('Set-Cookie', self._expired_session_cookie())],
            )
            return

        if not self._require_auth(request_path):
            return
        if self.path == '/api/system/time-sync':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            if not BROWSER_TIME_SYNC_ENABLED:
                self._send_json(503, {'ok': False, 'error': 'browser_time_sync_disabled'})
                return
            try:
                request = self._read_json()
            except TypeError as error:
                self._send_json(415, {'ok': False, 'error': str(error)})
                return
            except OverflowError as error:
                self._send_json(413, {'ok': False, 'error': str(error)})
                return
            except ValueError as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            valid_shape = (
                isinstance(request, dict)
                and set(request) == {'epoch_ms', 'timezone_offset_min'}
                and isinstance(request.get('epoch_ms'), int)
                and not isinstance(request.get('epoch_ms'), bool)
                and isinstance(request.get('timezone_offset_min'), int)
                and not isinstance(request.get('timezone_offset_min'), bool)
                and 1767225600000 <= request['epoch_ms'] < 2082758400000
                and -840 <= request['timezone_offset_min'] <= 840
            )
            if not valid_shape:
                self._send_json(400, {'ok': False, 'error': 'invalid_time_sync_request'})
                return
            with _time_sync_lock:
                now = time.monotonic()
                if now - _last_time_sync_request < 20.0:
                    try:
                        status = json.loads(TIME_GUARD_STATUS.read_text(encoding='utf-8'))
                    except (FileNotFoundError, OSError, ValueError, json.JSONDecodeError):
                        status = None
                    self._send_json(200, {'ok': True, 'action': 'rate_limited_noop', 'status': status})
                    return
                try:
                    result = subprocess.run(
                        [
                            '/usr/bin/sudo', '-n', TIME_SYNC_HELPER,
                            '--epoch-ms', str(request['epoch_ms']), '--source', 'browser',
                        ],
                        check=False,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT,
                        text=True,
                        timeout=10,
                    )
                except (OSError, subprocess.TimeoutExpired) as error:
                    self._send_json(500, {'ok': False, 'error': f'time_sync_failed:{error}'})
                    return
                if result.returncode != 0:
                    self._send_json(409, {'ok': False, 'error': 'time_sync_rejected', 'detail': result.stdout[-300:]})
                    return
                try:
                    status = json.loads(result.stdout.strip().splitlines()[-1])
                except (IndexError, ValueError, json.JSONDecodeError):
                    self._send_json(500, {'ok': False, 'error': 'time_sync_invalid_response'})
                    return
                _last_time_sync_request = time.monotonic()
            print(
                f'BEE_TIME_SYNC_AUDIT action={status.get("action")} source={status.get("source")} '
                f'client={self.client_address[0]} timezone_offset={request["timezone_offset_min"]}',
                flush=True,
            )
            self._send_json(200, {'ok': True, 'status': status})
            return
        if self.path == '/api/bags/action':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            try:
                request = self._read_json()
            except TypeError as error:
                self._send_json(415, {'ok': False, 'error': str(error)})
                return
            except OverflowError as error:
                self._send_json(413, {'ok': False, 'error': str(error)})
                return
            except ValueError as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            action = request.get('action') if isinstance(request, dict) else None
            allowed_keys = {'action'} if action in ('record_start', 'record_stop', 'play_stop') else {'action', 'bag_id'}
            if action not in ('record_start', 'record_stop', 'play_start', 'play_stop') or set(request) != allowed_keys:
                self._send_json(400, {'ok': False, 'error': 'unsupported_action'})
                return
            try:
                if action == 'record_start':
                    bag_id = bag_manager.start_recording()
                elif action == 'record_stop':
                    bag_id = bag_manager.stop_recording()
                elif action == 'play_start':
                    bag_id = bag_manager.start_playback(request.get('bag_id'))
                else:
                    bag_id = bag_manager.stop_playback()
            except (RuntimeError, ValueError, FileNotFoundError) as error:
                self._send_json(409, {'ok': False, 'error': str(error)})
                return
            print(f'BEE_BAG_AUDIT action={action} bag={bag_id} client={self.client_address[0]}', flush=True)
            self._send_json(200, {'ok': True, 'action': action, 'bag_id': bag_id, 'status': bag_manager.status()})
            return

        if self.path == '/api/maps/action':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            try:
                request = self._read_json()
            except TypeError as error:
                self._send_json(415, {'ok': False, 'error': str(error)})
                return
            except OverflowError as error:
                self._send_json(413, {'ok': False, 'error': str(error)})
                return
            except ValueError as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            action = request.get('action') if isinstance(request, dict) else None
            expected_keys = {'action', 'map_id'} if action == 'load' else {'action', 'name', 'site'} if action == 'save' else {'action'}
            if action not in ('save', 'load', 'unload') or set(request) != expected_keys:
                self._send_json(400, {'ok': False, 'error': 'unsupported_action'})
                return
            try:
                if action == 'save':
                    map_id = map_manager.start_save(request.get('name'), request.get('site'))
                elif action == 'load':
                    map_id = map_manager.load(request.get('map_id'))
                else:
                    map_id = map_manager.unload()
                    if _bridge_instance is not None:
                        _bridge_instance.clear_map()
            except (RuntimeError, ValueError, FileNotFoundError) as error:
                self._send_json(409, {'ok': False, 'error': str(error)})
                return
            print(f'BEE_MAP_AUDIT action={action} map={map_id} client={self.client_address[0]}', flush=True)
            self._send_json(202 if action == 'save' else 200, {'ok': True, 'action': action, 'map_id': map_id, 'status': map_manager.status()})
            return

        if self.path == '/api/slam/action':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            try:
                request = self._read_json()
            except TypeError as error:
                self._send_json(415, {'ok': False, 'error': str(error)})
                return
            except OverflowError as error:
                self._send_json(413, {'ok': False, 'error': str(error)})
                return
            except ValueError as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            action = request.get('action') if isinstance(request, dict) else None
            if action not in ('start', 'stop') or set(request) != {'action'}:
                self._send_json(400, {'ok': False, 'error': 'unsupported_action'})
                return
            try:
                status = experimental_slam_manager.action(action)
            except (RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
                self._send_json(409, {'ok': False, 'error': str(error)})
                return
            print(f'BEE_SLAM_AUDIT action={action} client={self.client_address[0]}', flush=True)
            self._send_json(200, {'ok': True, 'action': action, 'status': status})
            return

        if self.path == '/api/tasks/action':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            try:
                request = self._read_json()
                result = task_safety_manager.execute(request)
            except TypeError as error:
                self._send_json(415, {'ok': False, 'error': str(error)})
                return
            except OverflowError as error:
                self._send_json(413, {'ok': False, 'error': str(error)})
                return
            except ValueError as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            except RuntimeError as error:
                action = request.get('action') if isinstance(request, dict) else 'unknown'
                print(f'BEE_TASK_AUDIT rejected action={action} reason={error} client={self.client_address[0]}', flush=True)
                self._send_json(409, {'ok': False, 'error': str(error), 'status': task_safety_manager.status()})
                return
            print(f'BEE_TASK_AUDIT accepted action={result["action"]} request_id={result["request_id"]} client={self.client_address[0]}', flush=True)
            self._send_json(202, {'ok': True, **result, 'status': task_safety_manager.status()})
            return

        if self.path == '/api/patrols/action':
            if not self._origin_allowed():
                self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
                return
            try:
                request = self._read_json()
                action = request.get('action') if isinstance(request, dict) else None
                expected = {'action', 'id', 'name', 'map_id', 'repeat_count', 'waypoints'} if action == 'save' else {'action', 'id'}
                if action not in ('save', 'delete') or set(request) != expected:
                    raise ValueError('unsupported_action')
                if action == 'save':
                    item = patrol_draft_manager.save({key: value for key, value in request.items() if key != 'action'})
                    result = {'ok': True, 'action': action, 'item': item}
                else:
                    result = {'ok': True, 'action': action, 'id': patrol_draft_manager.delete(request.get('id'))}
            except TypeError as error:
                self._send_json(415, {'ok': False, 'error': str(error)})
                return
            except OverflowError as error:
                self._send_json(413, {'ok': False, 'error': str(error)})
                return
            except (ValueError, FileNotFoundError) as error:
                self._send_json(400, {'ok': False, 'error': str(error)})
                return
            print(f'BEE_PATROL_AUDIT action={action} id={result.get("id") or result.get("item", {}).get("id")} client={self.client_address[0]}', flush=True)
            self._send_json(200, result)
            return

        if self.path != '/api/system/power':
            self._send_json(404, {'ok': False, 'error': 'not_found'})
            return

        if not self._origin_allowed():
            self._send_json(403, {'ok': False, 'error': 'origin_rejected'})
            return
        if self.headers.get('Content-Type', '').split(';', 1)[0].strip() != 'application/json':
            self._send_json(415, {'ok': False, 'error': 'json_required'})
            return
        try:
            length = int(self.headers.get('Content-Length', '0'))
        except ValueError:
            length = 0
        if length <= 0 or length > 256:
            self._send_json(413, {'ok': False, 'error': 'invalid_size'})
            return
        try:
            request = json.loads(self.rfile.read(length).decode('utf-8'))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._send_json(400, {'ok': False, 'error': 'invalid_json'})
            return
        action = request.get('action') if isinstance(request, dict) else None
        if action not in POWER_ACTIONS or set(request) != {'action'}:
            self._send_json(400, {'ok': False, 'error': 'unsupported_action'})
            return
        with _power_lock:
            if _power_pending:
                self._send_json(409, {'ok': False, 'error': 'power_action_pending'})
                return
            _power_pending = True
        print(
            f'BEE_POWER_AUDIT accepted action={action} client={self.client_address[0]} dry_run={POWER_DRY_RUN}',
            flush=True,
        )
        timer = threading.Timer(2.0, execute_power_action, args=(action,))
        timer.daemon = True
        timer.start()
        self._send_json(202, {'ok': True, 'action': action, 'dry_run': POWER_DRY_RUN, 'delay_seconds': 2})


class RosWebBridge(Node):
    def __init__(self):
        super().__init__('bee_web_bridge')
        self._lock = threading.Lock()
        self._latest_scan = None
        self._latest_map = None
        self._latest_sim_layers = {}
        self._sim_version = 0
        self._map_version = 0
        self._map_signature = None
        self._last_monotonic = None
        self._hz = 0.0
        self._started_monotonic = time.monotonic()
        self._frames_total = 0
        self._valid_points_total = 0
        self._invalid_points_total = 0
        self._near_points = 0
        self._last_scan_age_ms = None
        self._last_min_range = None
        self._last_max_range = None
        self.create_subscription(LaserScan, '/scan', self._on_scan, qos_profile_sensor_data)
        map_qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self.create_subscription(OccupancyGrid, '/map', self._on_map, map_qos)
        self.create_subscription(PoseStamped, '/bee/sim/robot_pose', lambda msg: self._on_pose_layer('robot', msg), map_qos)
        self.create_subscription(NavPath, '/bee/sim/global_path', lambda msg: self._on_path_layer('global', msg), map_qos)
        self.create_subscription(NavPath, '/bee/sim/local_path', lambda msg: self._on_path_layer('local', msg), map_qos)
        self.create_subscription(PoseStamped, '/bee/sim/goal', lambda msg: self._on_pose_layer('goal', msg), map_qos)
        self.create_subscription(PoseArray, '/bee/sim/waypoints', lambda msg: self._on_pose_array_layer('waypoint', msg), map_qos)
        self.create_subscription(OccupancyGrid, '/bee/sim/costmap', self._on_costmap_layer, map_qos)
        self.create_subscription(NavPath, '/bee/sim/trail', lambda msg: self._on_path_layer('trail', msg), map_qos)
        self.create_subscription(PoseArray, '/bee/sim/obstacles', lambda msg: self._on_pose_array_layer('obstacle', msg), map_qos)

    @staticmethod
    def _yaw(orientation):
        return math.atan2(
            2.0 * (orientation.w * orientation.z + orientation.x * orientation.y),
            1.0 - 2.0 * (orientation.y * orientation.y + orientation.z * orientation.z),
        )

    def _store_sim_layer(self, name, payload):
        with self._lock:
            self._sim_version += 1
            payload.update({'type': 'sim_layer', 'layer': name, 'version': self._sim_version, 'is_test': True})
            self._latest_sim_layers[name] = payload

    def _on_pose_layer(self, name, msg):
        self._store_sim_layer(name, {
            'frame_id': msg.header.frame_id or 'bee_test_map',
            'pose': {
                'x': float(msg.pose.position.x),
                'y': float(msg.pose.position.y),
                'yaw': self._yaw(msg.pose.orientation),
            },
        })

    def _on_path_layer(self, name, msg):
        self._store_sim_layer(name, {
            'frame_id': msg.header.frame_id or 'bee_test_map',
            'points': [[float(item.pose.position.x), float(item.pose.position.y)] for item in msg.poses],
        })

    def _on_pose_array_layer(self, name, msg):
        self._store_sim_layer(name, {
            'frame_id': msg.header.frame_id or 'bee_test_map',
            'poses': [[float(item.position.x), float(item.position.y), self._yaw(item.orientation)] for item in msg.poses],
        })

    def _on_costmap_layer(self, msg):
        width = int(msg.info.width)
        height = int(msg.info.height)
        if width <= 0 or height <= 0 or len(msg.data) != width * height:
            return
        self._store_sim_layer('cost', {
            'frame_id': msg.header.frame_id or 'bee_test_map',
            'width': width,
            'height': height,
            'resolution': float(msg.info.resolution),
            'origin': {
                'x': float(msg.info.origin.position.x),
                'y': float(msg.info.origin.position.y),
                'yaw': self._yaw(msg.info.origin.orientation),
            },
            'data': [int(value) for value in msg.data],
        })

    def _on_scan(self, msg):
        now = time.monotonic()
        if self._last_monotonic is not None:
            instant = 1.0 / max(now - self._last_monotonic, 1e-6)
            self._hz = instant if self._hz == 0 else self._hz * 0.8 + instant * 0.2
        self._last_monotonic = now

        points = []
        finite = []
        near_points = 0
        for index, distance in enumerate(msg.ranges):
            if not math.isfinite(distance) or distance < msg.range_min or distance > msg.range_max:
                continue
            angle = msg.angle_min + index * msg.angle_increment
            intensity = msg.intensities[index] if index < len(msg.intensities) else 0.0
            points.append([round(angle, 5), round(float(distance), 3), round(float(intensity), 1)])
            finite.append(float(distance))
            if 0.15 <= distance < 0.30:
                near_points += 1

        # 丢弃启动阶段或异常的空帧，避免网页把雷达误报为在线。
        if len(finite) < 20:
            return
        # 浏览器无需绘制每个重复点，最多保留约360个均匀采样点。
        stride = max(1, len(points) // 360)
        payload = {
            'type': 'scan',
            'timestamp': time.time(),
            'hz': round(self._hz, 2),
            'total_points': len(msg.ranges),
            'valid_points': len(finite),
            'min_range': min(finite),
            'max_range': max(finite),
            'points': points[::stride],
        }
        with self._lock:
            self._latest_scan = payload
            self._frames_total += 1
            self._valid_points_total += len(finite)
            self._invalid_points_total += max(0, len(msg.ranges) - len(finite))
            self._near_points = near_points
            self._last_scan_age_ms = payload['age_ms'] if 'age_ms' in payload else 0
            self._last_min_range = min(finite)
            self._last_max_range = max(finite)

    def _on_map(self, msg):
        width = int(msg.info.width)
        height = int(msg.info.height)
        expected = width * height
        if width <= 0 or height <= 0 or len(msg.data) != expected:
            self.get_logger().warning(
                f'Ignoring invalid /map: {width}x{height}, data={len(msg.data)}'
            )
            return
        q = msg.info.origin.orientation
        yaw = math.atan2(
            2.0 * (q.w * q.z + q.x * q.y),
            1.0 - 2.0 * (q.y * q.y + q.z * q.z),
        )
        cells = [int(value) for value in msg.data]
        cell_bytes = array('b', cells).tobytes()
        signature = (
            width,
            height,
            float(msg.info.resolution),
            float(msg.info.origin.position.x),
            float(msg.info.origin.position.y),
            yaw,
            msg.header.frame_id,
            zlib.crc32(cell_bytes),
        )
        with self._lock:
            if signature == self._map_signature:
                return
            self._map_signature = signature
        free = sum(1 for value in cells if 0 <= value < 50)
        occupied = sum(1 for value in cells if value >= 50)
        unknown = expected - free - occupied
        with self._lock:
            self._map_version += 1
            self._latest_map = {
                'type': 'map',
                'version': self._map_version,
                'timestamp': time.time(),
                'frame_id': msg.header.frame_id or 'map',
                'is_test': (msg.header.frame_id or '').startswith('bee_test_'),
                'width': width,
                'height': height,
                'resolution': float(msg.info.resolution),
                'origin': {
                    'x': float(msg.info.origin.position.x),
                    'y': float(msg.info.origin.position.y),
                    'yaw': yaw,
                },
                'counts': {'free': free, 'occupied': occupied, 'unknown': unknown},
                'data': cells,
            }

    def scan_snapshot(self):
        with self._lock:
            if self._latest_scan is None:
                return None
            payload = dict(self._latest_scan)
        payload['age_ms'] = max(0, round((time.time() - payload['timestamp']) * 1000))
        if payload['age_ms'] > 1000:
            return None
        return payload

    def map_snapshot(self):
        with self._lock:
            return None if self._latest_map is None else dict(self._latest_map)

    def clear_map(self):
        """Broadcast an explicit map reset after a managed map is unloaded."""
        with self._lock:
            self._map_version += 1
            self._map_signature = None
            self._latest_map = {
                'type': 'map_clear',
                'version': self._map_version,
                'timestamp': time.time(),
            }

    def sim_layers_snapshot(self):
        with self._lock:
            return [dict(value) for value in self._latest_sim_layers.values()]

    def diagnostics_snapshot(self):
        with self._lock:
            total_points = self._valid_points_total + self._invalid_points_total
            latest_age = None
            if self._latest_scan is not None:
                latest_age = max(0, round((time.time() - self._latest_scan['timestamp']) * 1000))
            return {
                'online': latest_age is not None and latest_age <= 1500,
                'uptime_seconds': round(time.monotonic() - self._started_monotonic, 1),
                'frames_total': self._frames_total,
                'frequency_hz': round(self._hz, 2),
                'valid_points_total': self._valid_points_total,
                'invalid_points_total': self._invalid_points_total,
                'valid_ratio': round(self._valid_points_total / total_points, 4) if total_points else 0.0,
                'near_points_15_30cm': self._near_points,
                'last_age_ms': latest_age,
                'last_min_range': self._last_min_range,
                'last_max_range': self._last_max_range,
                'scan_topic': '/scan',
                'replay_topic': '/bee/replay/scan',
            }


async def websocket_handler(connection, bridge):
    request = getattr(connection, 'request', None)
    headers = getattr(request, 'headers', {})
    token = _cookie_token(headers.get('Cookie', ''))
    if not _session_valid(token):
        print('BEE_AUTH_AUDIT action=websocket result=denied', flush=True)
        await connection.close(code=1008, reason='authentication_required')
        return
    last_map_version = -1
    last_sim_versions = {}
    try:
        while True:
            map_payload = bridge.map_snapshot()
            if map_payload is not None and map_payload['version'] != last_map_version:
                await connection.send(json.dumps(map_payload, separators=(',', ':'), ensure_ascii=False))
                last_map_version = map_payload['version']
            for layer_payload in bridge.sim_layers_snapshot():
                layer_name = layer_payload['layer']
                if last_sim_versions.get(layer_name) == layer_payload['version']:
                    continue
                await connection.send(json.dumps(layer_payload, separators=(',', ':'), ensure_ascii=False))
                last_sim_versions[layer_name] = layer_payload['version']
            scan_payload = bridge.scan_snapshot()
            if scan_payload is not None:
                await connection.send(json.dumps(scan_payload, separators=(',', ':'), ensure_ascii=False))
            await asyncio.sleep(0.1)
    except Exception:
        return


def start_http_server():
    server = ThreadingHTTPServer(('0.0.0.0', HTTP_PORT), QuietHandler)
    thread = threading.Thread(target=server.serve_forever, name='bee-http', daemon=True)
    thread.start()
    return server


def spin_ros(bridge):
    try:
        rclpy.spin(bridge)
    except ExternalShutdownException:
        return


async def main():
    global _bridge_instance
    stop_event = asyncio.Event()
    loop = asyncio.get_running_loop()
    for stop_signal in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(stop_signal, stop_event.set)

    rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
    bridge = RosWebBridge()
    _bridge_instance = bridge
    ros_thread = threading.Thread(target=spin_ros, args=(bridge,), name='bee-ros', daemon=True)
    ros_thread.start()
    http_server = start_http_server()
    try:
        async with serve(lambda connection: websocket_handler(connection, bridge), '0.0.0.0', WS_PORT):
            await stop_event.wait()
    finally:
        http_server.shutdown()
        camera_manager.shutdown()
        http_server.server_close()
        bag_manager.shutdown()
        map_manager.shutdown()
        bridge.destroy_node()
        rclpy.shutdown()
        ros_thread.join(timeout=3)


if __name__ == '__main__':
    asyncio.run(main())
