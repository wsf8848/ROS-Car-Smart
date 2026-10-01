# RDK X5 板端环境备份

## 最新备份

- 文件：`bee-console-20260929-083318.tar.gz`
- 来源：RDK X5 `/opt/bee-ros-web-console/backups/delivery/`
- 内容：Bee ROS Web Console、ROS工作空间、配置、部署脚本、工具、地图三文件、巡逻草稿、LD14质量资料、受控rosbag、STM32预检、地图恢复和总验收报告、自动时间同步助手/规则/可信记录、Bee systemd service/timer、脱敏环境和网络配置。
- 归档SHA-256：`1F2D4519FD95C8756D1C87605F4EB653FDEAADC683CCFE07C5FA9E3F47CB184E`
- 校验文件：`bee-console-20260929-083318.tar.gz.sha256`
- 包内条目数：341

## 恢复原则

1. 在新板或重装系统后先安装匹配的RDK OS/ROS2依赖。
2. 解压归档，将网站恢复到 `/opt/bee-ros-web-console`，ROS工作空间恢复到 `/opt/bee_ros_ws`。
3. 将归档根目录的 `bee-*.service`和`bee-*.timer`恢复到 `/etc/systemd/system/`，执行 `systemctl daemon-reload`。
4. 依据 `host-config/bee-ros-web-console.env.redacted` 重新生成管理员密码哈希和会话签名密钥，权限设置为 `600 root:root`。
5. 依据 `host-config/bee-ros-hotspot.nmconnection.redacted` 重建NetworkManager热点，并重新注入热点密码。
6. 检查 `data/maps` 中每张地图的 `.yaml/.pgm/.json` 配对，确认地图目录后再启动服务。
7. 依次验证网页登录、地图列表、LD14、ROS话题、systemd状态、总自动验收和固定访问地址。

## 安全边界

本备份不含真实管理员密码、热点密码、会话密钥、SSH私钥或浏览器Cookie。恢复时必须由管理员重新注入这些凭据，不能把 `REDACTED` 当成可用值。
