#!/usr/bin/env python3
"""Bee板卡时间守护：NTP/浏览器可信时间优先，RTC可选，文件记录防止倒退。"""

import argparse
import json
import os
import subprocess
import time
from datetime import datetime, timezone
from pathlib import Path


STATUS_PATH = Path('/run/bee-time-guard/status.json')
TRUSTED_SYNC_PATH = Path('/var/lib/bee-time-guard/last-trusted-sync.json')
MIN_VALID_EPOCH = datetime(2026, 1, 1, tzinfo=timezone.utc).timestamp()
MAX_VALID_EPOCH = datetime(2036, 1, 1, tzinfo=timezone.utc).timestamp()
MAX_HEALTHY_DELTA_SECONDS = 5.0
RECENT_TRUST_SECONDS = 7 * 24 * 60 * 60


def valid_epoch(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and MIN_VALID_EPOCH <= value < MAX_VALID_EPOCH


def read_json(path):
    try:
        return json.loads(path.read_text(encoding='utf-8'))
    except (FileNotFoundError, OSError, ValueError, json.JSONDecodeError):
        return None


def read_rtc(retries=1):
    last_error = None
    for attempt in range(max(1, retries)):
        try:
            result = subprocess.run(
                ['/usr/sbin/hwclock', '--show', '--utc'],
                check=True,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=8,
            )
            value = result.stdout.strip().splitlines()[-1]
            parsed = datetime.fromisoformat(value.replace(' ', 'T', 1))
            if parsed.tzinfo is None:
                parsed = parsed.replace(tzinfo=timezone.utc)
            return parsed.astimezone(timezone.utc)
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            last_error = error
            if attempt + 1 < retries:
                time.sleep(1.0)
    raise RuntimeError(f'rtc_unavailable:{last_error}')


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


def write_status(payload):
    STATUS_PATH.parent.mkdir(parents=True, exist_ok=True)
    temporary = STATUS_PATH.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(payload, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    os.chmod(temporary, 0o644)
    temporary.replace(STATUS_PATH)


def set_system_epoch(epoch):
    time.clock_settime(time.CLOCK_REALTIME, epoch)


def sample(repair):
    system_epoch = time.time()
    trusted = read_json(TRUSTED_SYNC_PATH) or {}
    trusted_epoch = trusted.get('trusted_epoch')
    try:
        rtc = read_rtc(retries=20 if repair else 1)
        rtc_epoch = rtc.timestamp()
    except RuntimeError:
        rtc = None
        rtc_epoch = None
    action = 'checked'

    if repair:
        candidates = [value for value in (system_epoch, rtc_epoch, trusted_epoch) if valid_epoch(value)]
        if candidates:
            best_epoch = max(candidates)
            if not valid_epoch(system_epoch) or best_epoch - system_epoch > 1.0:
                set_system_epoch(best_epoch)
                action = 'system_restored_from_best_available'
            system_epoch = time.time()
            if rtc is not None and valid_epoch(system_epoch):
                subprocess.run(['/usr/sbin/hwclock', '--systohc', '--utc'], check=False, timeout=8)
                try:
                    rtc = read_rtc()
                    rtc_epoch = rtc.timestamp()
                except RuntimeError:
                    rtc = None
                    rtc_epoch = None

    system_epoch = time.time()
    ntp_ready = ntp_synchronized()
    trusted_age = system_epoch - trusted_epoch if valid_epoch(trusted_epoch) else None
    trusted_recent = trusted_age is not None and -5.0 <= trusted_age <= RECENT_TRUST_SECONDS
    rtc_delta = system_epoch - rtc_epoch if valid_epoch(rtc_epoch) else None
    rtc_close = rtc_delta is not None and abs(rtc_delta) <= MAX_HEALTHY_DELTA_SECONDS
    if ntp_ready:
        source = 'ntp'
    elif trusted_recent:
        source = trusted.get('source') or 'browser_sync'
    elif rtc_close:
        source = 'rtc_offline'
    elif valid_epoch(trusted_epoch):
        source = 'last_trusted_fallback'
    else:
        source = 'system_unverified'
    healthy = valid_epoch(system_epoch) and (ntp_ready or trusted_recent or rtc_close)
    now = datetime.fromtimestamp(system_epoch, timezone.utc)
    payload = {
        'healthy': healthy,
        'source': source,
        'action': action,
        'checked_epoch': system_epoch,
        'checked_at_utc': now.isoformat(),
        'system_utc': now.isoformat(),
        'rtc_utc': rtc.isoformat() if rtc is not None else None,
        'rtc_delta_seconds': round(rtc_delta, 3) if rtc_delta is not None else None,
        'rtc_available': rtc is not None,
        'ntp_synchronized': ntp_ready,
        'last_trusted_utc': datetime.fromtimestamp(trusted_epoch, timezone.utc).isoformat() if valid_epoch(trusted_epoch) else None,
        'last_trusted_age_seconds': round(trusted_age, 3) if trusted_age is not None else None,
        'min_valid_utc': datetime.fromtimestamp(MIN_VALID_EPOCH, timezone.utc).isoformat(),
        'max_valid_utc': datetime.fromtimestamp(MAX_VALID_EPOCH, timezone.utc).isoformat(),
    }
    write_status(payload)
    print(json.dumps(payload, ensure_ascii=False))
    return 0 if healthy else 1


def main():
    parser = argparse.ArgumentParser(description='Bee RDK time guard')
    parser.add_argument('--repair', action='store_true', help='use the newest valid RTC/file/system candidate at boot')
    arguments = parser.parse_args()
    raise SystemExit(sample(arguments.repair))


if __name__ == '__main__':
    main()
