#!/usr/bin/env python3
"""隔离Nav2仿真中验收Bee任务执行器，不经过生产运动安全门。"""

import argparse
import json
import time
import uuid
from pathlib import Path


def read_status(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (OSError, json.JSONDecodeError):
        return None


def wait_status(path, states, timeout=120.0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        status = read_status(path)
        if status and status.get('state') in states:
            return status
        time.sleep(0.1)
    raise TimeoutError(f'timeout waiting for {sorted(states)}; last={read_status(path)}')


def submit(command_path, status_path, action, **fields):
    request_id = uuid.uuid4().hex
    payload = {'action': action, 'request_id': request_id, **fields}
    temporary = command_path.with_suffix('.tmp')
    temporary.write_text(json.dumps(payload, ensure_ascii=False, separators=(',', ':')), encoding='utf-8')
    temporary.replace(command_path)
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        status = read_status(status_path)
        if status and status.get('request_id') == request_id:
            return request_id
        time.sleep(0.05)
    raise TimeoutError(f'command not accepted: {action}')


def pose(x, y, yaw=0.0):
    return {'x': float(x), 'y': float(y), 'yaw': float(yaw)}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--root', default='/run/bee-nav-task-sim')
    parser.add_argument('--case', choices=['full', 'failure'], default='full')
    args = parser.parse_args()
    root = Path(args.root)
    command = root / 'command.json'
    status_file = root / 'status.json'
    initial = wait_status(status_file, {'idle', 'completed', 'canceled', 'failed'}, timeout=15.0)
    if not initial.get('motion_allowed'):
        raise AssertionError(f'simulation executor is not armed: {initial}')

    if args.case == 'failure':
        submit(command, status_file, 'navigate', **pose(-5.8, -3.8))
        failed = wait_status(status_file, {'completed', 'failed'}, timeout=45.0)
        assert failed['state'] == 'failed', failed
        print(json.dumps({'ok': True, 'failure_state': failed['state'], 'reason': failed['last_error']}, ensure_ascii=False), flush=True)
        return

    results = {}
    started = time.monotonic()
    submit(command, status_file, 'navigate', **pose(-2.5, -1.2))
    results['single'] = wait_status(status_file, {'completed', 'failed'}, timeout=90.0)
    assert results['single']['state'] == 'completed', results['single']
    results['single_seconds'] = round(time.monotonic() - started, 2)

    started = time.monotonic()
    submit(
        command,
        status_file,
        'patrol_start',
        waypoints=[pose(-3.2, -1.5), pose(-2.7, -1.0), pose(-3.4, -0.7)],
        repeat_count=2,
    )
    results['patrol'] = wait_status(status_file, {'completed', 'failed'}, timeout=180.0)
    assert results['patrol']['state'] == 'completed', results['patrol']
    assert results['patrol']['progress_percent'] == 100, results['patrol']
    assert results['patrol']['repeat_total'] == 2, results['patrol']
    results['patrol_seconds'] = round(time.monotonic() - started, 2)

    submit(command, status_file, 'navigate', **pose(2.5, 1.8))
    wait_status(status_file, {'running'}, timeout=15.0)
    submit(command, status_file, 'pause')
    results['paused'] = wait_status(status_file, {'paused'}, timeout=15.0)
    submit(command, status_file, 'resume')
    wait_status(status_file, {'running'}, timeout=15.0)
    submit(command, status_file, 'cancel')
    results['canceled'] = wait_status(status_file, {'canceled'}, timeout=15.0)

    summary = {
        'ok': True,
        'single_seconds': results['single_seconds'],
        'patrol_seconds': results['patrol_seconds'],
        'patrol_repeat_total': results['patrol']['repeat_total'],
        'pause_state': results['paused']['state'],
        'cancel_state': results['canceled']['state'],
    }
    print(json.dumps(summary, ensure_ascii=False), flush=True)


if __name__ == '__main__':
    main()
