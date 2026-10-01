#!/usr/bin/env python3
"""STM32到货只读预检；默认不打开串口，显式probe时只接收不发送运动命令。"""

import argparse
import glob
import grp
import json
import math
import os
import pwd
import stat
import subprocess
import time
from pathlib import Path

from bee_uart5_protocol import Decoder, TELEMETRY, TELEMETRY_STRUCT, crc16_ccitt, encode_frame


def command_output(command, timeout=5):
    try:
        result = subprocess.run(
            command, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, timeout=timeout,
        )
        return result.returncode, result.stdout.strip()
    except (OSError, subprocess.TimeoutExpired):
        return 127, ''


def service_info(name):
    _, active = command_output(['systemctl', 'is-active', name])
    _, enabled = command_output(['systemctl', 'is-enabled', name])
    return {'active': active or 'unknown', 'enabled': enabled or 'unknown'}


def discover_serial_candidates():
    patterns = ('/dev/serial/by-id/*', '/dev/ttyUSB*', '/dev/ttyACM*', '/dev/ttyS*')
    items = []
    seen = set()
    for pattern in patterns:
        for value in sorted(glob.glob(pattern)):
            path = Path(value)
            try:
                resolved = str(path.resolve())
            except OSError:
                resolved = value
            key = (value, resolved)
            if key in seen:
                continue
            seen.add(key)
            items.append({'path': value, 'resolved': resolved, 'stable': '/dev/serial/by-id/' in value})
    return items


def device_info(path):
    target = Path(path)
    result = {
        'path': path, 'exists': target.exists(), 'resolved': None,
        'readable': False, 'writable': False, 'type': None,
        'owner': None, 'group': None, 'mode': None, 'stable_links': [],
    }
    if not target.exists():
        return result
    resolved = target.resolve()
    info = resolved.stat()
    result.update({
        'resolved': str(resolved),
        'type': 'character' if stat.S_ISCHR(info.st_mode) else 'other',
        'readable': os.access(target, os.R_OK),
        'writable': os.access(target, os.W_OK),
        'owner': pwd.getpwuid(info.st_uid).pw_name,
        'group': grp.getgrgid(info.st_gid).gr_name,
        'mode': oct(stat.S_IMODE(info.st_mode)),
    })
    for link in glob.glob('/dev/serial/by-id/*'):
        try:
            if Path(link).resolve() == resolved:
                result['stable_links'].append(link)
        except OSError:
            continue
    return result


def protocol_self_test():
    vector_ok = crc16_ccitt(b'123456789') == 0x29B1
    payload = b'\x01\x02\x03\x04'
    encoded = encode_frame(0x10, 7, 1234, payload)
    frames = Decoder().feed(encoded)
    round_trip = len(frames) == 1 and frames[0][3] == payload and frames[0][1] == 7
    corrupted = bytearray(encoded)
    corrupted[-1] ^= 0x55
    crc_rejected = len(Decoder().feed(bytes(corrupted))) == 0
    return {
        'crc_vector_0x29b1': vector_ok,
        'round_trip': round_trip,
        'corrupt_crc_rejected': crc_rejected,
        'telemetry_payload_bytes': TELEMETRY_STRUCT.size,
    }


def ros_packages():
    packages = {}
    for name in ('nav2_bringup', 'slam_toolbox', 'robot_localization'):
        code, prefix = command_output([
            '/usr/bin/bash', '-lc',
            f'source /opt/ros/humble/setup.bash && ros2 pkg prefix {name}',
        ], timeout=8)
        packages[name] = {'installed': code == 0, 'prefix': prefix or None}
    return packages


def serial_probe(path, baud, seconds):
    result = {
        'requested': seconds > 0, 'duration_seconds': seconds, 'opened': False,
        'bytes_received': 0, 'frames_total': 0, 'telemetry_frames': 0,
        'frequency_hz': 0.0, 'crc_errors': 0, 'version_errors': 0,
        'length_errors': 0, 'sequence_gaps': 0, 'timestamp_monotonic': True,
        'latest': None, 'error': None,
    }
    if seconds <= 0:
        return result
    try:
        import serial
        port = serial.Serial(path, baud, timeout=0.05, write_timeout=0.1, exclusive=True)
    except Exception as error:
        result['error'] = f'open_failed:{error}'
        return result
    result['opened'] = True
    decoder = Decoder()
    last_seq = None
    last_timestamp = None
    started = time.monotonic()
    first_telemetry_at = None
    last_telemetry_at = None
    try:
        while time.monotonic() - started < seconds:
            data = port.read(port.in_waiting or 1)
            if not data:
                continue
            result['bytes_received'] += len(data)
            for msg_type, seq, timestamp_ms, payload in decoder.feed(data):
                result['frames_total'] += 1
                if last_seq is not None:
                    expected = (last_seq + 1) & 0xFFFF
                    if seq != expected:
                        result['sequence_gaps'] += (seq - expected) & 0xFFFF
                last_seq = seq
                if last_timestamp is not None and ((timestamp_ms - last_timestamp) & 0xFFFFFFFF) > 0x7FFFFFFF:
                    result['timestamp_monotonic'] = False
                last_timestamp = timestamp_ms
                if msg_type != TELEMETRY or len(payload) != TELEMETRY_STRUCT.size:
                    continue
                now = time.monotonic()
                first_telemetry_at = first_telemetry_at or now
                last_telemetry_at = now
                result['telemetry_frames'] += 1
                values = TELEMETRY_STRUCT.unpack(payload)
                latest = {
                    'seq': seq, 'stm32_timestamp_ms': timestamp_ms,
                    'wheel_ticks': list(values[0:4]),
                    'wheel_speeds': [round(value, 5) for value in values[4:8]],
                    'accel_mps2': [round(value, 5) for value in values[8:11]],
                    'gyro_rad_s': [round(value, 6) for value in values[11:14]],
                    'temperature_c': round(values[14], 2),
                    'battery_voltage_v': round(values[15], 2),
                    'battery_percent': round(values[16], 3),
                    'control_source': values[17], 'state_flags': values[18],
                    'fault_code': values[19],
                }
                latest['finite_numeric_fields'] = all(math.isfinite(float(value)) for value in values[4:17])
                result['latest'] = latest
    except Exception as error:
        result['error'] = f'read_failed:{error}'
    finally:
        port.close()
    result['crc_errors'] = decoder.crc_errors
    result['version_errors'] = decoder.version_errors
    result['length_errors'] = decoder.length_errors
    if result['telemetry_frames'] > 1 and first_telemetry_at is not None and last_telemetry_at > first_telemetry_at:
        result['frequency_hz'] = round((result['telemetry_frames'] - 1) / (last_telemetry_at - first_telemetry_at), 2)
    return result


def main():
    parser = argparse.ArgumentParser(description='Bee STM32 arrival readiness audit')
    parser.add_argument('--device', default=os.environ.get('BEE_STM32_DEVICE', '/dev/ttyS5'))
    parser.add_argument('--baud', type=int, default=115200)
    parser.add_argument('--probe-seconds', type=float, default=0.0)
    parser.add_argument('--output')
    args = parser.parse_args()

    services = {
        name: service_info(unit) for name, unit in {
            'bee_base_driver': 'bee-base-driver.service',
            'bee_virtual_stm32': 'bee-virtual-stm32.service',
            'bee_nav_task_executor': 'bee-nav-task-executor.service',
            'bee_nav2_stack': 'bee-nav2-stack.service',
            'bee_web_console': 'bee-web-console.service',
            'bee_ld14': 'bee-ld14.service',
        }.items()
    }
    protocol = protocol_self_test()
    serial_info = device_info(args.device)
    safe_idle = all(services[name]['active'] != 'active' for name in (
        'bee_base_driver', 'bee_virtual_stm32', 'bee_nav_task_executor', 'bee_nav2_stack',
    ))
    probe = serial_probe(args.device, args.baud, args.probe_seconds) if safe_idle else {
        'requested': args.probe_seconds > 0, 'opened': False,
        'error': 'unsafe_service_active', 'telemetry_frames': 0,
    }
    protocol_ok = all(protocol[key] for key in ('crc_vector_0x29b1', 'round_trip', 'corrupt_crc_rejected'))
    node_present = serial_info['exists'] and serial_info['type'] == 'character'
    telemetry_valid = bool(
        probe.get('telemetry_frames', 0) > 0
        and probe.get('crc_errors') == 0
        and probe.get('latest', {}).get('finite_numeric_fields')
    )
    blockers = []
    if not node_present:
        blockers.append('configured_serial_device_missing')
    if node_present and not serial_info['readable']:
        blockers.append('serial_device_not_readable')
    if not safe_idle:
        blockers.append('motion_or_simulation_service_active')
    if not telemetry_valid:
        blockers.append('no_valid_stm32_telemetry' if args.probe_seconds > 0 else 'stm32_telemetry_probe_not_run')
    if not Path('/etc/bee-ros-web-console/physical-chassis-verified').is_file():
        blockers.append('physical_chassis_verification_not_completed')

    checks = {
        'generated_at': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
        'configured_link': {'device': args.device, 'baud': args.baud, 'validated': telemetry_valid},
        'protocol': protocol, 'serial': serial_info,
        'serial_candidates': discover_serial_candidates(), 'probe': probe,
        'services': services, 'ros_packages': ros_packages(),
        'required_physical_inputs': [
            'wheel_radius_m', 'wheel_track_m', 'encoder_counts_per_revolution',
            'motor_polarity', 'encoder_polarity', 'imu_mounting_orientation',
        ],
        'required_physical_tests': [
            'confirm_rdk_stm32_tx_rx_gnd_and_voltage',
            'confirm_telemetry_frequency_crc_sequence_and_timestamp',
            'confirm_icm42670_calibration_ready_and_axis_direction',
            'confirm_four_encoder_direction_and_scale',
            'wheels_off_ground_cmd_vel_and_300_to_350ms_watchdog',
            'verify_bluetooth_mqtt_ros_arbitration_and_estop',
            'low_speed_straight_turn_and_odometry_calibration',
        ],
        'summary': {
            'protocol_ready': protocol_ok, 'safe_idle': safe_idle,
            'device_node_present': node_present, 'telemetry_validated': telemetry_valid,
            'can_begin_readonly_probe': bool(protocol_ok and safe_idle and node_present and serial_info['readable']),
            'can_begin_motion_test': False, 'blockers': blockers,
        },
    }
    rendered = json.dumps(checks, ensure_ascii=False, indent=2)
    print(rendered)
    if args.output:
        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        temporary = output.with_suffix(output.suffix + '.tmp')
        temporary.write_text(rendered + '\n', encoding='utf-8')
        temporary.replace(output)
    if not protocol_ok or not safe_idle:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
