#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/humble/setup.bash
set -u
exec ros2 launch nav2_bringup navigation_launch.py \
  params_file:=/opt/bee-ros-web-console/config/nav2_sim_params.yaml \
  autostart:=true \
  use_sim_time:=false
