#!/usr/bin/env bash
set -euo pipefail

STAGE=${1:-/tmp/bee-timesync1}
EPOCH_MS=${2:?Usage: deploy-time-sync.sh STAGE EPOCH_MS}
case "$EPOCH_MS" in
  ''|*[!0-9]*) echo 'invalid_epoch_ms' >&2; exit 2 ;;
esac

for file in bee_time_sync_helper.py bee_time_guard.py bee_gateway.py index.html live.js bee-time-sync.sudoers; do
  test -f "$STAGE/$file"
done

BACKUP=/opt/bee-ros-web-console/backups/pre-timesync1
sudo -n mkdir -p "$BACKUP/tools" "$BACKUP/gateway" "$BACKUP/frontend" "$BACKUP/deploy"
sudo -n cp -a /opt/bee-ros-web-console/tools/bee_time_guard.py "$BACKUP/tools/"
sudo -n cp -a /opt/bee-ros-web-console/gateway/bee_gateway.py "$BACKUP/gateway/"
sudo -n cp -a /opt/bee-ros-web-console/frontend/index.html /opt/bee-ros-web-console/frontend/live.js "$BACKUP/frontend/"

sudo -n install -m 0755 "$STAGE/bee_time_sync_helper.py" /opt/bee-ros-web-console/tools/bee_time_sync_helper.py
sudo -n install -m 0755 "$STAGE/bee_time_sync_helper.py" /usr/local/sbin/bee-time-sync
sudo -n install -m 0755 "$STAGE/bee_time_guard.py" /opt/bee-ros-web-console/tools/bee_time_guard.py
sudo -n install -m 0755 "$STAGE/bee_gateway.py" /opt/bee-ros-web-console/gateway/bee_gateway.py
sudo -n install -m 0644 "$STAGE/index.html" /opt/bee-ros-web-console/frontend/index.html
sudo -n install -m 0644 "$STAGE/live.js" /opt/bee-ros-web-console/frontend/live.js
sudo -n install -m 0440 "$STAGE/bee-time-sync.sudoers" /etc/sudoers.d/bee-time-sync
sudo -n install -m 0440 "$STAGE/bee-time-sync.sudoers" /opt/bee-ros-web-console/deploy/bee-time-sync.sudoers
sudo -n visudo -cf /etc/sudoers.d/bee-time-sync
rm -rf "$STAGE"

sudo -n /usr/local/sbin/bee-time-sync --epoch-ms "$EPOCH_MS" --source browser
sudo -n systemctl restart bee-time-guard.service

OLD_PID=$(systemctl show -p MainPID --value bee-web-console.service)
sudo -n systemctl restart bee-web-console.service
for _ in $(seq 1 20); do
  NEW_PID=$(systemctl show -p MainPID --value bee-web-console.service)
  STATE=$(systemctl is-active bee-web-console.service || true)
  if [[ "$STATE" == active && "$NEW_PID" != 0 && "$NEW_PID" != "$OLD_PID" ]] \
    && curl -fsS http://127.0.0.1:8080/login.html >/dev/null; then
    printf 'restart_ok old=%s new=%s\n' "$OLD_PID" "$NEW_PID"
    exit 0
  fi
  sleep 1
done

systemctl status bee-web-console.service --no-pager || true
exit 1
