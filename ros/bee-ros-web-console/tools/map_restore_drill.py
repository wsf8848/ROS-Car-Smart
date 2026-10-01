#!/usr/bin/env python3
"""非破坏性地图恢复演练：复制、哈希、隔离map_server加载与OccupancyGrid验证。"""

import argparse
import hashlib
import json
import os
import shutil
import signal
import subprocess
import tempfile
import time
from pathlib import Path

import rclpy
from nav_msgs.msg import OccupancyGrid
from rclpy.node import Node
from rclpy.qos import DurabilityPolicy, HistoryPolicy, QoSProfile, ReliabilityPolicy
from rclpy.signals import SignalHandlerOptions


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def yaml_image_name(path):
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.strip().startswith('image:'):
            value = line.split(':', 1)[1].strip().strip('"\'')
            if not value or Path(value).is_absolute() or '..' in Path(value).parts:
                raise ValueError('unsafe_yaml_image')
            return value
    raise ValueError('yaml_image_missing')


class MapProbe(Node):
    def __init__(self, topic):
        super().__init__('bee_map_restore_probe')
        self.message = None
        qos = QoSProfile(
            history=HistoryPolicy.KEEP_LAST,
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        self.create_subscription(OccupancyGrid, topic, self._receive, qos)

    def _receive(self, message):
        self.message = message


def run_cli(arguments, timeout=15):
    return subprocess.run(
        ['/opt/ros/humble/bin/ros2', *arguments],
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, timeout=timeout,
    )


def wait_lifecycle(node_name, timeout=12):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        result = run_cli(['lifecycle', 'get', '--no-daemon', node_name], timeout=4)
        if result.returncode == 0:
            return result.stdout.strip().lower()
        time.sleep(0.25)
    raise TimeoutError('map_server_lifecycle_unavailable')


def load_restored_map(yaml_path, topic, node_name):
    command = [
        '/opt/ros/humble/bin/ros2', 'run', 'nav2_map_server', 'map_server',
        '--ros-args', '-r', f'__node:={node_name}', '-r', f'map:={topic}',
        '-p', f'yaml_filename:={yaml_path}',
    ]
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, start_new_session=True,
    )
    try:
        lifecycle_state = wait_lifecycle(f'/{node_name}')
        transitions = []
        if 'unconfigured' in lifecycle_state:
            transitions = ['configure', 'activate']
        elif 'inactive' in lifecycle_state:
            transitions = ['activate']
        elif 'active' not in lifecycle_state:
            raise RuntimeError(f'unexpected_lifecycle_state:{lifecycle_state}')
        for transition in transitions:
            result = run_cli(['lifecycle', 'set', '--no-daemon', f'/{node_name}', transition], timeout=15)
            if result.returncode != 0 or 'successful' not in result.stdout.lower():
                raise RuntimeError(f'lifecycle_{transition}_failed:{result.stdout.strip()}')
        rclpy.init(signal_handler_options=SignalHandlerOptions.NO)
        probe = MapProbe(topic)
        try:
            deadline = time.monotonic() + 10
            while time.monotonic() < deadline and probe.message is None:
                rclpy.spin_once(probe, timeout_sec=0.2)
            if probe.message is None:
                raise TimeoutError('restored_map_topic_timeout')
            message = probe.message
            expected = int(message.info.width) * int(message.info.height)
            if expected <= 0 or len(message.data) != expected:
                raise ValueError('invalid_occupancy_grid')
            return {
                'topic': topic,
                'frame_id': message.header.frame_id,
                'width': int(message.info.width),
                'height': int(message.info.height),
                'resolution': float(message.info.resolution),
                'cells': len(message.data),
                'free': sum(1 for value in message.data if 0 <= value < 50),
                'occupied': sum(1 for value in message.data if value >= 50),
                'unknown': sum(1 for value in message.data if value < 0),
            }
        finally:
            probe.destroy_node()
            rclpy.try_shutdown()
    finally:
        try:
            os.killpg(process.pid, signal.SIGTERM)
        except ProcessLookupError:
            pass
        try:
            process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(process.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            process.wait(timeout=3)


def map_ids(root, requested):
    if requested:
        return requested
    return sorted(path.stem for path in root.glob('*.yaml'))


def main():
    parser = argparse.ArgumentParser(description='Bee地图灾备恢复演练')
    parser.add_argument('--maps-root', default='/opt/bee-ros-web-console/data/maps')
    parser.add_argument('--map-id', action='append', dest='map_ids')
    parser.add_argument('--output')
    args = parser.parse_args()
    source_root = Path(args.maps_root).resolve()
    if not source_root.is_dir():
        raise FileNotFoundError('maps_root_not_found')

    results = []
    with tempfile.TemporaryDirectory(prefix='bee-map-restore-') as temporary_root:
        restore_root = Path(temporary_root)
        for index, map_id in enumerate(map_ids(source_root, args.map_ids), start=1):
            if Path(map_id).name != map_id:
                raise ValueError('invalid_map_id')
            source_yaml = source_root / f'{map_id}.yaml'
            source_pgm = source_root / f'{map_id}.pgm'
            source_json = source_root / f'{map_id}.json'
            if not source_yaml.is_file() or not source_pgm.is_file():
                raise FileNotFoundError(f'incomplete_map:{map_id}')
            image_name = yaml_image_name(source_yaml)
            if image_name != source_pgm.name:
                raise ValueError(f'yaml_pgm_mismatch:{map_id}:{image_name}')
            restored_yaml = restore_root / source_yaml.name
            restored_pgm = restore_root / source_pgm.name
            shutil.copy2(source_yaml, restored_yaml)
            shutil.copy2(source_pgm, restored_pgm)
            restored_json = None
            if source_json.is_file():
                restored_json = restore_root / source_json.name
                shutil.copy2(source_json, restored_json)
            files = [
                {'name': source_yaml.name, 'sha256': sha256(source_yaml), 'restored_sha256': sha256(restored_yaml)},
                {'name': source_pgm.name, 'sha256': sha256(source_pgm), 'restored_sha256': sha256(restored_pgm)},
            ]
            if restored_json is not None:
                files.append({'name': source_json.name, 'sha256': sha256(source_json), 'restored_sha256': sha256(restored_json)})
            if any(item['sha256'] != item['restored_sha256'] for item in files):
                raise ValueError(f'restore_hash_mismatch:{map_id}')
            unique_node = f'bee_restore_test_map_server_{os.getpid()}_{index}'
            grid = load_restored_map(
                restored_yaml,
                f'/bee/restore_test/map_{index}',
                unique_node,
            )
            results.append({'map_id': map_id, 'files': files, 'grid': grid})

    report = {
        'ok': True,
        'generated_at': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
        'source_root': str(source_root),
        'map_count': len(results),
        'maps': results,
        'production_maps_modified': False,
    }
    rendered = json.dumps(report, ensure_ascii=False, indent=2)
    print(rendered)
    if args.output:
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + '.tmp')
        temporary.write_text(rendered + '\n', encoding='utf-8')
        temporary.replace(output)


if __name__ == '__main__':
    main()
