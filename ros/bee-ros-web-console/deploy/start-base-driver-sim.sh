#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/humble/setup.bash
set -u
cd /opt/bee-ros-web-console/tools
exec /usr/bin/python3 ./bee_base_driver.py --ros-args -p port:=/tmp/bee_uart5_sim -p baud:=115200
