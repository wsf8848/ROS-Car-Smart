#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/humble/setup.bash
set -u

PARAMS=/opt/bee-ros-web-console/config/slam_toolbox_online_async.yaml
pids=()
stopping=0

cleanup() {
  trap - TERM INT EXIT
  for pid in "${pids[@]:-}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill -TERM "$pid" 2>/dev/null || true
    fi
  done
  for pid in "${pids[@]:-}"; do
    wait "$pid" 2>/dev/null || true
  done
}

stop_cleanly() {
  stopping=1
  cleanup
  exit 0
}

trap stop_cleanly TERM INT
trap cleanup EXIT

# Experimental no-chassis chain only. These identity transforms are placeholders
# for validating real /scan -> SLAM -> /map data flow and must never be reused as
# the final vehicle calibration.
/opt/ros/humble/lib/tf2_ros/static_transform_publisher \
  --x 0 --y 0 --z 0 --roll 0 --pitch 0 --yaw 0 \
  --frame-id odom --child-frame-id base_link &
pids+=("$!")

/opt/ros/humble/lib/tf2_ros/static_transform_publisher \
  --x 0 --y 0 --z 0 --roll 0 --pitch 0 --yaw 0 \
  --frame-id base_link --child-frame-id laser &
pids+=("$!")

/opt/ros/humble/bin/ros2 launch slam_toolbox online_async_launch.py \
  use_sim_time:=false slam_params_file:="$PARAMS" &
pids+=("$!")

set +e
wait -n "${pids[@]}"
exit_code=$?
set -e
if (( stopping )); then
  exit 0
fi
exit "$exit_code"
