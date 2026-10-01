#!/usr/bin/env bash
set -euo pipefail

SSID=${1:-}
PASSWORD=${2:-}
ADDRESS=${3:-192.168.50.1/24}
CONNECTION=bee-ros-hotspot

if [[ "$PASSWORD" == "-" ]]; then
  IFS= read -r -s -p "Hotspot password: " PASSWORD
  echo
fi

if [[ -z "$SSID" || ${#PASSWORD} -lt 8 ]]; then
  echo "Usage: configure-hotspot.sh SSID PASSWORD|'-' [ADDRESS/CIDR]" >&2
  echo "Password must contain at least 8 characters." >&2
  exit 2
fi

if ! nmcli -f WIFI-PROPERTIES.AP device show wlan0 | grep -q 'yes'; then
  echo "wlan0 does not report AP capability" >&2
  exit 3
fi

sudo nmcli connection delete "$CONNECTION" >/dev/null 2>&1 || true
sudo nmcli connection add type wifi ifname wlan0 con-name "$CONNECTION" autoconnect yes ssid "$SSID"
sudo nmcli connection modify "$CONNECTION" \
  802-11-wireless.mode ap \
  802-11-wireless.band bg \
  ipv4.method shared \
  ipv4.addresses "$ADDRESS" \
  ipv6.method disabled \
  wifi-sec.key-mgmt wpa-psk \
  wifi-sec.psk "$PASSWORD"
sudo nmcli connection up "$CONNECTION"
nmcli -f connection.id,connection.type,connection.autoconnect connection show "$CONNECTION"
nmcli device status
ip -brief address show wlan0
echo "Bee ROS Web Console: http://${ADDRESS%/*}:8080/"
