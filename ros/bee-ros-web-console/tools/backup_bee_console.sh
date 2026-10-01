#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-$(id -u)} -ne 0 ]]; then
  echo 'backup_requires_root: run with sudo -n bash backup_bee_console.sh' >&2
  exit 1
fi

BACKUP_ROOT=${1:-/opt/bee-ros-web-console/backups/delivery}
STAMP=$(date +%Y%m%d-%H%M%S)
NAME="bee-console-${STAMP}"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

mkdir -p "$BACKUP_ROOT"
mkdir -p "$WORK/$NAME"

cp -a /opt/bee-ros-web-console/frontend "$WORK/$NAME/"
cp -a /opt/bee-ros-web-console/gateway "$WORK/$NAME/"
cp -a /opt/bee-ros-web-console/config "$WORK/$NAME/"
cp -a /opt/bee-ros-web-console/deploy "$WORK/$NAME/"
cp -a /opt/bee-ros-web-console/tools "$WORK/$NAME/"
cp -a /opt/bee_ros_ws "$WORK/$NAME/ros_ws" 2>/dev/null || true
mkdir -p "$WORK/$NAME/data"
# 当前data仅保存地图、巡逻草稿、诊断报告和受控rosbag；备份整个目录，避免新增报告被遗漏。
cp -a /opt/bee-ros-web-console/data/. "$WORK/$NAME/data/" 2>/dev/null || true
find /etc/systemd/system -maxdepth 1 -type f \( -name 'bee-*.service' -o -name 'bee-*.timer' \) \
  -exec cp -a {} "$WORK/$NAME/" \; 2>/dev/null || true

# 保留恢复所需的配置结构，但绝不把密码、会话签名密钥或原始Wi-Fi连接文件带入备份。
mkdir -p "$WORK/$NAME/host-config"
if [ -f /etc/bee-ros-web-console.env ]; then
  sed -E 's/^(BEE_ADMIN_PASSWORD_HASH|BEE_SESSION_SECRET|BEE_HOTSPOT_PSK)=.*/\1=REDACTED/' \
    /etc/bee-ros-web-console.env > "$WORK/$NAME/host-config/bee-ros-web-console.env.redacted"
fi
if [ -f /etc/NetworkManager/system-connections/bee-ros-hotspot.nmconnection ]; then
  sed -E 's/^(psk|password|wifi-sec\.psk)=.*/\1=REDACTED/' \
    /etc/NetworkManager/system-connections/bee-ros-hotspot.nmconnection > "$WORK/$NAME/host-config/bee-ros-hotspot.nmconnection.redacted"
fi
if [ -f /etc/sudoers.d/bee-time-sync ]; then
  cp -a /etc/sudoers.d/bee-time-sync "$WORK/$NAME/host-config/bee-time-sync.sudoers"
fi
if [ -f /var/lib/bee-time-guard/last-trusted-sync.json ]; then
  cp -a /var/lib/bee-time-guard/last-trusted-sync.json "$WORK/$NAME/host-config/last-trusted-sync.json"
fi
systemctl list-unit-files 'bee-*.service' 'bee-*.timer' --no-legend > "$WORK/$NAME/host-config/systemd-unit-files.txt" || true
systemctl list-units 'bee-*.service' 'bee-*.timer' --all --no-legend > "$WORK/$NAME/host-config/systemd-runtime.txt" || true
nmcli -t -f NAME,TYPE,DEVICE connection show > "$WORK/$NAME/host-config/network-connections.txt" 2>/dev/null || true
ip -4 addr show > "$WORK/$NAME/host-config/ip-addresses.txt" 2>/dev/null || true
cat > "$WORK/$NAME/host-config/README.md" <<'EOF'
# 板端环境恢复说明

1. 将 `bee-ros-web-console` 目录恢复到 `/opt/bee-ros-web-console`。
2. 将 `ros_ws` 恢复到 `/opt/bee_ros_ws`，按目标板ROS发行版重新source并构建。
3. 将根目录中的 `bee-*.service` 安装到 `/etc/systemd/system/`，执行 `systemctl daemon-reload`。
4. 复制脱敏环境文件为 `/etc/bee-ros-web-console.env`，重新生成管理员密码哈希和会话签名密钥，并设置 `600 root:root`。
5. 依据脱敏热点文件重建NetworkManager连接，重新注入WPA密码；不要直接把脱敏值当作真实密码。
6. 校验地图目录中的每组 `.yaml` 与 `.pgm` 文件配对后，再启动网页和ROS服务。
7. 将 `host-config/bee-time-sync.sudoers` 恢复到 `/etc/sudoers.d/`并设为`0440 root:root`，把时间同步助手安装为`/usr/local/sbin/bee-time-sync`。
8. `last-trusted-sync.json`只用于防止无RTC开机倒退；网页登录后仍应以浏览器UTC或NTP重新精确同步。

原始凭据、私钥和会话令牌不在本备份中，恢复时必须重新注入。
EOF

(
  cd "$WORK/$NAME"
  find . -type f ! -name SHA256SUMS -print0 | sort -z | xargs -0 sha256sum > SHA256SUMS
)
tar -C "$WORK" -czf "$BACKUP_ROOT/$NAME.tar.gz" "$NAME"
sha256sum "$BACKUP_ROOT/$NAME.tar.gz" > "$BACKUP_ROOT/$NAME.tar.gz.sha256"
printf '%s\n' "$BACKUP_ROOT/$NAME.tar.gz"
