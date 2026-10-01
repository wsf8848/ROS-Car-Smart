#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/humble/setup.bash
set -u
exec /opt/bee_ros_ws/install_manual/lib/ld14/ld14_node \
  --ros-args \
  -p port_name:=/dev/LD14 \
  -p product:=LD14 \
  -p flag_parted:=false
