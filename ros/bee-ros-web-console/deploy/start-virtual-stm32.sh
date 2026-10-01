#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/humble/setup.bash
set -u
cd /opt/bee-ros-web-console/tools
exec /usr/bin/python3 ./virtual_stm32.py
