#!/usr/bin/env bash
set -eo pipefail
source /opt/ros/humble/setup.bash
set -u
exec /usr/bin/python3 /opt/bee-ros-web-console/tools/bee_nav_task_executor.py

