#!/usr/bin/env python3
"""Bee ROS Web Console无STM32阶段总自动验收。

本脚本只做只读检查，不启动底盘、Nav2、仿真或地图服务，也不发送运动命令。
基线通过表示网站、地图资产和安全门处于预期状态，不表示实车已经通过。
"""

import argparse
import hashlib
import json
import os
import re
import shlex
import socket
import subprocess
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


ACTIVE_SERVICES = (
    'bee-web-console.service',
    'bee-ld14.service',
    'bee-time-guard.service',
    'bee-time-check.timer',
)

LOCKED_SERVICES = (
    'bee-base-driver.service',
    'bee-base-driver-sim.service',
    'bee-experimental-slam.service',
    'bee-nav-task-executor.service',
    'bee-nav-task-executor-sim.service',
    'bee-nav2-map.service',
    'bee-nav2-sim.service',
    'bee-nav2-stack.service',
    'bee-sim-layers.service',
    'bee-virtual-stm32.service',
)

PROTECTED_ENDPOINTS = (
    '/api/health',
    '/api/maps',
    '/api/tasks',
    '/api/commissioning',
    '/api/system/health',
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(chunk)
    return digest.hexdigest()


def command(*args, timeout=12, env=None):
    try:
        result = subprocess.run(
            args,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout,
            env=env,
        )
        return result.stdout.strip(), result.returncode
    except (OSError, subprocess.TimeoutExpired) as error:
        return str(error), 127


def systemctl(action, service):
    output, _ = command('systemctl', action, service, timeout=8)
    return output.splitlines()[0].strip() if output else 'unknown'


def tcp_open(host, port, timeout=1.5):
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, request, fp, code, message, headers, new_url):
        return None


def http_probe(url, follow_redirects=True):
    opener = urllib.request.build_opener() if follow_redirects else urllib.request.build_opener(NoRedirect)
    request = urllib.request.Request(url, headers={'User-Agent': 'BeeDeliveryAudit/1.0'})
    try:
        with opener.open(request, timeout=4) as response:
            body = response.read(4096)
            return {
                'status': response.status,
                'url': response.geturl(),
                'location': response.headers.get('Location'),
                'cache_control': response.headers.get('Cache-Control'),
                'content_type': response.headers.get('Content-Type'),
                'body_bytes_sampled': len(body),
            }
    except urllib.error.HTTPError as error:
        body = error.read(4096)
        return {
            'status': error.code,
            'url': error.geturl(),
            'location': error.headers.get('Location'),
            'cache_control': error.headers.get('Cache-Control'),
            'content_type': error.headers.get('Content-Type'),
            'body_bytes_sampled': len(body),
        }
    except OSError as error:
        return {'status': None, 'error': str(error)}


def unauthenticated_time_sync_probe(base_url):
    payload = json.dumps({'epoch_ms': int(time.time() * 1000), 'timezone_offset_min': -480}).encode('utf-8')
    request = urllib.request.Request(
        base_url + '/api/system/time-sync',
        data=payload,
        headers={'Content-Type': 'application/json', 'Origin': base_url},
        method='POST',
    )
    try:
        with urllib.request.urlopen(request, timeout=4) as response:
            return {'status': response.status, 'body': response.read(1024).decode('utf-8', errors='replace')}
    except urllib.error.HTTPError as error:
        return {'status': error.code, 'body': error.read(1024).decode('utf-8', errors='replace')}
    except OSError as error:
        return {'status': None, 'error': str(error)}


def yaml_image_name(path):
    for line in path.read_text(encoding='utf-8').splitlines():
        if line.strip().startswith('image:'):
            value = line.split(':', 1)[1].strip().strip('"\'')
            candidate = Path(value)
            if not value or candidate.is_absolute() or '..' in candidate.parts:
                raise ValueError('unsafe_yaml_image')
            return value
    raise ValueError('yaml_image_missing')


class Audit:
    def __init__(self):
        self.checks = []

    def add(self, name, passed, detail=None, severity='critical'):
        self.checks.append({
            'name': name,
            'status': 'pass' if passed else ('warning' if severity == 'warning' else 'fail'),
            'severity': severity,
            'detail': detail,
        })

    def result(self):
        failures = [item for item in self.checks if item['status'] == 'fail']
        warnings = [item for item in self.checks if item['status'] == 'warning']
        return {
            'pass': not failures,
            'failure_count': len(failures),
            'warning_count': len(warnings),
            'checks': self.checks,
        }


def check_python_sources(audit, root):
    failures = []
    sources = sorted((root / 'gateway').glob('*.py')) + sorted((root / 'tools').glob('*.py'))
    for path in sources:
        try:
            compile(path.read_text(encoding='utf-8'), str(path), 'exec')
        except Exception as error:
            failures.append({'file': str(path.relative_to(root)), 'error': str(error)})
    audit.add('python_syntax', not failures, {'files': len(sources), 'failures': failures})


def check_frontend(audit, root):
    frontend = root / 'frontend'
    index_path = frontend / 'index.html'
    required = ('index.html', 'login.html', 'live.js', 'live.css', 'login.js', 'login.css', 'bee-ros-console.svg')
    missing = [name for name in required if not (frontend / name).is_file()]
    audit.add('frontend_assets_present', not missing, {'missing': missing})
    if not index_path.is_file():
        return None
    index = index_path.read_text(encoding='utf-8')
    versions = {
        name: match.group(1) if match else None
        for name, pattern in {
            'live_js': r'/live\.js\?v=([^"\']+)',
            'live_css': r'/live\.css\?v=([^"\']+)',
            'favicon': r'/bee-ros-console\.svg\?v=([^"\']+)',
        }.items()
        for match in [re.search(pattern, index)]
    }
    unique = {value for value in versions.values() if value}
    audit.add(
        'frontend_resource_version',
        all(versions.values()) and len(unique) == 1,
        {'versions': versions, 'release': next(iter(unique)) if len(unique) == 1 else None},
    )
    return next(iter(unique)) if len(unique) == 1 else None


def check_http(audit, base_url):
    login = http_probe(base_url + '/', follow_redirects=True)
    audit.add(
        'login_entry',
        login.get('status') == 200 and login.get('url', '').endswith('/login.html'),
        login,
    )
    audit.add(
        'login_no_store',
        'no-store' in (login.get('cache_control') or '').lower(),
        {'cache_control': login.get('cache_control')},
    )
    endpoint_results = {path: http_probe(base_url + path, follow_redirects=False) for path in PROTECTED_ENDPOINTS}
    audit.add(
        'unauthenticated_api_401',
        all(item.get('status') == 401 for item in endpoint_results.values()),
        endpoint_results,
    )
    time_sync_result = unauthenticated_time_sync_probe(base_url)
    audit.add('unauthenticated_time_sync_401', time_sync_result.get('status') == 401, time_sync_result)


def check_time_sync(audit):
    status_path = Path('/run/bee-time-guard/status.json')
    trusted_path = Path('/var/lib/bee-time-guard/last-trusted-sync.json')
    helper_path = Path('/usr/local/sbin/bee-time-sync')
    try:
        status = json.loads(status_path.read_text(encoding='utf-8'))
    except (FileNotFoundError, OSError, ValueError, json.JSONDecodeError) as error:
        audit.add('runtime_time_sync', False, {'error': str(error)})
        return
    checked_epoch = status.get('checked_epoch')
    age = time.time() - checked_epoch if isinstance(checked_epoch, (int, float)) else None
    allowed_sources = ('ntp', 'browser_sync', 'rtc_offline', 'last_trusted_fallback')
    passed = (
        status.get('healthy') is True
        and status.get('source') in allowed_sources
        and age is not None
        and -5.0 <= age <= 300.0
        and helper_path.is_file()
        and os.access(helper_path, os.X_OK)
        and trusted_path.is_file()
    )
    audit.add('runtime_time_sync', passed, {
        'source': status.get('source'),
        'action': status.get('action'),
        'age_seconds': round(age, 3) if age is not None else None,
        'system_utc': status.get('system_utc'),
        'rtc_available': status.get('rtc_available'),
        'rtc_delta_seconds': status.get('rtc_delta_seconds'),
        'helper_installed': helper_path.is_file() and os.access(helper_path, os.X_OK),
        'trusted_sync_persisted': trusted_path.is_file(),
    })


def check_services(audit):
    expected_active = {}
    for service in ACTIVE_SERVICES:
        state = {
            'active': systemctl('is-active', service),
            'enabled': systemctl('is-enabled', service),
            'failed': systemctl('is-failed', service),
        }
        expected_active[service] = state
    audit.add(
        'required_services_active',
        all(item['active'] == 'active' and item['enabled'] == 'enabled' and item['failed'] != 'failed'
            for item in expected_active.values()),
        expected_active,
    )

    locked = {}
    for service in LOCKED_SERVICES:
        state = {
            'active': systemctl('is-active', service),
            'enabled': systemctl('is-enabled', service),
            'failed': systemctl('is-failed', service),
        }
        locked[service] = state
    audit.add(
        'motion_services_fail_closed',
        all(item['active'] == 'inactive' and item['enabled'] in ('disabled', 'static') and item['failed'] != 'failed'
            for item in locked.values()),
        locked,
    )


def check_maps(audit, root):
    maps_root = root / 'data' / 'maps'
    map_details = []
    if not maps_root.is_dir():
        audit.add('map_library', False, {'error': 'maps_root_missing'})
        return []
    map_ids = sorted(path.stem for path in maps_root.glob('*.yaml'))
    complete = True
    metadata_missing = []
    for map_id in map_ids:
        yaml_path = maps_root / f'{map_id}.yaml'
        pgm_path = maps_root / f'{map_id}.pgm'
        json_path = maps_root / f'{map_id}.json'
        item = {'map_id': map_id, 'yaml': True, 'pgm': pgm_path.is_file(), 'metadata': json_path.is_file()}
        try:
            item['yaml_image'] = yaml_image_name(yaml_path)
            item['yaml_matches_pgm'] = item['yaml_image'] == pgm_path.name
        except Exception as error:
            item['yaml_error'] = str(error)
            item['yaml_matches_pgm'] = False
        if json_path.is_file():
            try:
                metadata = json.loads(json_path.read_text(encoding='utf-8'))
                item['metadata_valid'] = (
                    metadata.get('id') == map_id
                    and isinstance(metadata.get('name'), str)
                    and isinstance(metadata.get('site'), str)
                    and isinstance(metadata.get('created_at'), str)
                )
            except Exception as error:
                item['metadata_valid'] = False
                item['metadata_error'] = str(error)
        else:
            metadata_missing.append(map_id)
            item['metadata_valid'] = None
        if not item['pgm'] or not item['yaml_matches_pgm'] or item.get('metadata_valid') is False:
            complete = False
        map_details.append(item)
    audit.add('map_library', complete and bool(map_ids), {'map_count': len(map_ids), 'maps': map_details})
    audit.add(
        'map_metadata_sidecars',
        not metadata_missing,
        {'missing_for_legacy_maps': metadata_missing},
        severity='warning',
    )
    return map_ids


def check_restore_report(audit, root, map_ids):
    report_path = root / 'data' / 'map-restore-drill.json'
    if not report_path.is_file():
        audit.add('map_restore_drill', False, {'error': 'report_missing'})
        return
    try:
        report = json.loads(report_path.read_text(encoding='utf-8'))
        reported_ids = sorted(item.get('map_id') for item in report.get('maps', []))
        hashes_match = all(
            file_item.get('sha256') == file_item.get('restored_sha256')
            for item in report.get('maps', [])
            for file_item in item.get('files', [])
        )
        grids_valid = all(
            int(item.get('grid', {}).get('cells', 0)) > 0
            and int(item.get('grid', {}).get('width', 0)) > 0
            and int(item.get('grid', {}).get('height', 0)) > 0
            for item in report.get('maps', [])
        )
        current_hashes_match = True
        for item in report.get('maps', []):
            for file_item in item.get('files', []):
                current = root / 'data' / 'maps' / file_item['name']
                if not current.is_file() or sha256(current) != file_item.get('sha256'):
                    current_hashes_match = False
        passed = (
            report.get('ok') is True
            and report.get('production_maps_modified') is False
            and reported_ids == sorted(map_ids)
            and hashes_match
            and grids_valid
            and current_hashes_match
        )
        audit.add('map_restore_drill', passed, {
            'generated_at': report.get('generated_at'),
            'map_count': report.get('map_count'),
            'reported_ids': reported_ids,
            'current_ids': sorted(map_ids),
            'hashes_match': hashes_match,
            'current_hashes_match': current_hashes_match,
            'grids_valid': grids_valid,
            'production_maps_modified': report.get('production_maps_modified'),
        })
    except Exception as error:
        audit.add('map_restore_drill', False, {'error': str(error)})


def check_stm32_report(audit, root):
    path = root / 'data' / 'stm32-arrival-readiness.json'
    if not path.is_file():
        audit.add('stm32_arrival_readiness', False, {'error': 'report_missing'})
        return {'hardware_ready': False, 'blockers': ['report_missing']}
    try:
        report = json.loads(path.read_text(encoding='utf-8'))
        summary = report.get('summary', {})
        protocol = report.get('protocol', {})
        baseline_pass = (
            summary.get('protocol_ready') is True
            and summary.get('safe_idle') is True
            and protocol.get('crc_vector_0x29b1') is True
            and protocol.get('round_trip') is True
            and protocol.get('corrupt_crc_rejected') is True
            and protocol.get('telemetry_payload_bytes') == 73
        )
        hardware_ready = summary.get('telemetry_validated') is True and summary.get('can_begin_motion_test') is True
        audit.add('stm32_arrival_readiness', baseline_pass, {
            'generated_at': report.get('generated_at'),
            'protocol_ready': summary.get('protocol_ready'),
            'safe_idle': summary.get('safe_idle'),
            'device_node_present': summary.get('device_node_present'),
            'telemetry_validated': summary.get('telemetry_validated'),
            'can_begin_readonly_probe': summary.get('can_begin_readonly_probe'),
            'can_begin_motion_test': summary.get('can_begin_motion_test'),
            'blockers': summary.get('blockers', []),
        })
        return {'hardware_ready': hardware_ready, 'blockers': summary.get('blockers', [])}
    except Exception as error:
        audit.add('stm32_arrival_readiness', False, {'error': str(error)})
        return {'hardware_ready': False, 'blockers': ['invalid_report']}


def check_contract(audit, root):
    script = root / 'tools' / 'test_patrol_contract.py'
    if not script.is_file():
        audit.add('patrol_contract_fail_closed', False, {'error': 'test_script_missing'})
        return
    gateway_path = root / 'gateway' / 'bee_gateway.py'
    shell_command = (
        'source /opt/ros/humble/setup.bash && '
        f'BEE_GATEWAY_PATH={shlex.quote(str(gateway_path))} '
        f'{shlex.quote(sys.executable)} {shlex.quote(str(script))}'
    )
    output, code = command('/bin/bash', '-lc', shell_command, timeout=20)
    passed = code == 0
    if passed:
        try:
            passed = json.loads(output.splitlines()[-1]).get('ok') is True
        except Exception:
            passed = False
    audit.add('patrol_contract_fail_closed', passed, {'exit_code': code, 'output': output[-2000:]})


def check_residual_processes(audit):
    output, code = command('ps', '-eo', 'pid,ppid,pgid,stat,cmd', timeout=8)
    patterns = ('bee_restore_test_map_server', 'map_restore_drill.py', '/tmp/bee-map-restore-')
    residual = [line for line in output.splitlines() if any(pattern in line for pattern in patterns)] if code == 0 else []
    audit.add('restore_process_cleanup', code == 0 and not residual, {'residual': residual})


def write_report(path, report):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    temporary.replace(path)


def main():
    parser = argparse.ArgumentParser(description='Bee ROS Web Console总自动验收')
    parser.add_argument('--root', default='/opt/bee-ros-web-console')
    parser.add_argument('--base-url', default='http://127.0.0.1:8080')
    parser.add_argument('--output')
    args = parser.parse_args()
    root = Path(args.root).resolve()
    output = Path(args.output) if args.output else root / 'data' / 'delivery-audit.json'
    audit = Audit()

    audit.add('project_root', root.is_dir(), {'root': str(root)})
    if root.is_dir():
        check_python_sources(audit, root)
        release = check_frontend(audit, root)
        check_http(audit, args.base_url.rstrip('/'))
        check_services(audit)
        check_time_sync(audit)
        ports = {'8080': tcp_open('127.0.0.1', 8080), '8765': tcp_open('127.0.0.1', 8765)}
        audit.add('web_ports', all(ports.values()), ports)
        map_ids = check_maps(audit, root)
        check_restore_report(audit, root, map_ids)
        hardware = check_stm32_report(audit, root)
        check_contract(audit, root)
        check_residual_processes(audit)
    else:
        release = None
        hardware = {'hardware_ready': False, 'blockers': ['project_root_missing']}

    summary = audit.result()
    report = {
        'generated_at': time.strftime('%Y-%m-%dT%H:%M:%S%z'),
        'mode': 'no_stm32_safe_baseline',
        'release': release,
        'baseline_pass': summary['pass'],
        'hardware_ready': hardware['hardware_ready'],
        'hardware_blockers': hardware['blockers'],
        'failure_count': summary['failure_count'],
        'warning_count': summary['warning_count'],
        'checks': summary['checks'],
        'statement': '基线通过不等于实车通过；未验证STM32遥测、真实里程计、TF、SLAM、AMCL/Nav2和整车安全闭环。',
    }
    write_report(output, report)
    print(json.dumps(report, ensure_ascii=False, indent=2))
    if not report['baseline_pass']:
        raise SystemExit(2)


if __name__ == '__main__':
    main()
