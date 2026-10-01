#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/humble/setup.bash
set -u

ros2 run nav2_map_server map_server --ros-args \
  -r __node:=bee_nav2_test_map_server \
  -p yaml_filename:=/opt/bee-ros-web-console/data/maps/map_dd34a0db9b.yaml &
map_pid=$!

cleanup() {
  kill -TERM "$map_pid" 2>/dev/null || true
  wait "$map_pid" 2>/dev/null || true
}
terminate() {
  cleanup
  exit 0
}
trap cleanup EXIT
trap terminate INT TERM

for _ in $(seq 1 20); do
  if ros2 lifecycle get --no-daemon /bee_nav2_test_map_server >/dev/null 2>&1; then
    break
  fi
  sleep 0.5
done
ros2 lifecycle set --no-daemon /bee_nav2_test_map_server configure | grep -qi successful
ros2 lifecycle set --no-daemon /bee_nav2_test_map_server activate | grep -qi successful
wait "$map_pid" || status=$?
exit "${status:-0}"
