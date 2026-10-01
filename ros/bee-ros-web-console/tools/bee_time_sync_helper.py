#!/usr/bin/env python3
"""受限的板卡时间同步助手，只接受经过范围校验的浏览器UTC时间戳。"""

import argparse
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path


MIN_VALID_EPOCH = datetime(2026, 1, 1, tzinfo=timezone.utc).timestamp()
MAX_VALID_EPOCH = datetime(2036, 1, 1, tzinfo=timezone.utc).timestamp()
NO_CHANGE_THRESHOLD_SECONDS = 0.75
RECENT_SYNC_MAX_JUMP_SECONDS = 48 * 60 * 60
TRUSTED_SYNC_PATH = Path('/var/lib/bee-time-guard/last-trusted-sync.json')
STATUS_PATH = Path('/run/bee-time-guard/status.json')


def valid_epoch(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and MIN_VALID_EPOCH <= value < MAX_VALID_EPOCH


def read_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (FileNotFoundError, OSError, ValueError, json.JSONDecodeError):
        return None


def write_json(path, payload, mode):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    os.chmod(temporary, mode)
    temporary.replace(path)


def ntp_synchronized():
    try:
        result = subprocess.run(
            ['/usr/bin/timedatectl', 'show', '--property=NTPSynchronized', '--value'],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            timeout=4,
        )
        return result.returncode == 0 and result.stdout.strip().lower() == 'yes'
    except (OSError, subprocess.SubprocessError):
        return False


def read_rtc():
    try:
        result = subprocess.run(
            ['/usr/sbin/hwclock', '--show', '--utc'],
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=6,
        )
        parsed = datetime.fromisoformat(result.stdout.strip().splitlines()[-1].replace(' ', 'T', 1))
        if parsed.tzinfo is None:
            parsed = parsed.replace(tzinfo=timezone.utc)
        return parsed.astimezone(timezone.utc).timestamp()
    except (OSError, ValueError, subprocess.SubprocessError):
        return None


def main():
    parser = argparse.ArgumentParser(description='Bee浏览器UTC时间同步助手')
    parser.add_argument('--epoch-ms', required=True, type=int)
    parser.add_argument('--source', required=True, choices=('browser',))
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args()
    if os.geteuid() != 0 and not args.dry_run:
        raise SystemExit('root_required')

    requested_epoch = args.epoch_ms / 1000.0
    if not valid_epoch(requested_epoch):
        raise SystemExit('invalid_epoch')
    before_epoch = time.time()
    delta = requested_epoch - before_epoch
    previous = read_json(TRUSTED_SYNC_PATH) or {}
    previous_epoch = previous.get('trusted_epoch')
    if valid_epoch(previous_epoch) and abs(delta) > RECENT_SYNC_MAX_JUMP_SECONDS:
        raise SystemExit('jump_exceeds_recent_sync_limit')

    if ntp_synchronized():
        action = 'ntp_authoritative'
    elif abs(delta) < NO_CHANGE_THRESHOLD_SECONDS:
        action = 'already_synchronized'
    elif args.dry_run:
        action = 'dry_run_would_sync'
    else:
        time.clock_settime(time.CLOCK_REALTIME, requested_epoch)
        action = 'system_synced_from_browser'

    after_epoch = requested_epoch if action == 'system_synced_from_browser' else time.time()
    rtc_written = False
    if not args.dry_run and action != 'ntp_authoritative' and Path('/dev/rtc0').exists():
        result = subprocess.run(
            ['/usr/sbin/hwclock', '--systohc', '--utc'],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=8,
        )
        rtc_written = result.returncode == 0
    rtc_epoch = read_rtc()
    checked = time.time()
    payload = {
        'healthy': valid_epoch(checked),
        'source': 'browser_sync' if action != 'ntp_authoritative' else 'ntp',
        'action': action,
        'checked_epoch': checked,
        'checked_at_utc': datetime.fromtimestamp(checked, timezone.utc).isoformat(),
        'system_utc': datetime.fromtimestamp(checked, timezone.utc).isoformat(),
        'requested_utc': datetime.fromtimestamp(requested_epoch, timezone.utc).isoformat(),
        'browser_delta_seconds_before': round(delta, 3),
        'rtc_utc': datetime.fromtimestamp(rtc_epoch, timezone.utc).isoformat() if valid_epoch(rtc_epoch) else None,
        'rtc_delta_seconds': round(checked - rtc_epoch, 3) if valid_epoch(rtc_epoch) else None,
        'rtc_written': rtc_written,
    }
    if not args.dry_run:
        write_json(TRUSTED_SYNC_PATH, {
            'source': payload['source'],
            'trusted_epoch': requested_epoch if action != 'ntp_authoritative' else checked,
            'synced_at_epoch': checked,
            'synced_at_utc': payload['checked_at_utc'],
        }, 0o644)
        write_json(STATUS_PATH, payload, 0o644)
    print(json.dumps(payload, ensure_ascii=False))


if __name__ == '__main__':
    main()
