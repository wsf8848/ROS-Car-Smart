# Bee ROS Web Console

RDK X5上的四驱自主导航小车网页控制台。真实LD14、地图管理、任务编辑、板卡健康、摄像头和认证已经接入；STM32、真实里程计/IMU/TF和实车Nav2仍明确标记为未验收。

## 当前链路

`LD14 -> CP2102 /dev/LD14 -> ROS2 /scan -> bee_gateway.py -> WebSocket :8765 -> 浏览器 :8080`

`SLAM/地图发布器 -> ROS2 /map (OccupancyGrid) -> bee_gateway.py -> 绿色/红色/深灰真实栅格`

`Linux内核遥测 -> /api/system/health -> 板卡健康弹窗（CPU温度/CPU/内存/磁盘/运行时长）`

`STM32到货预检JSON -> /api/commissioning -> 联调检查弹窗（协议/串口/遥测/ROS依赖/阻塞项）`

`网页受控任务API -> 双重运动互锁 -> bee_nav_task_executor -> Nav2 NavigateToPose`（生产默认锁定，当前仅隔离仿真通过）

## 板端目录

- 网站与网关：`/opt/bee-ros-web-console`
- 地图库：`/opt/bee-ros-web-console/data/maps`（固定板端目录，详见 `deploy/MAP_STORAGE.md`）
- ROS工作空间：`/opt/bee_ros_ws`
- 常驻服务：`bee-ld14.service`、`bee-web-console.service`
- 手动/待到货服务：`bee-base-driver.service`、`bee-nav-task-executor.service`、Nav2地图/栈和仿真服务（默认disabled）
- 访问地址：`http://192.168.127.10:8080/`

## 当前安全边界

- 网关只开放白名单地图、任务、诊断和系统接口，不向浏览器暴露任意ROS话题/服务/Action能力。
- 无STM32时禁用网页运动、急停、导航和巡逻执行；目标与巡逻航点仍可编辑保存，不伪造在线状态。
- 地图目标点、方向端点和巡逻航点保持稳定屏幕视觉尺寸；左键短点目标不改朝向，按住约180ms后拖动设置yaw，松手固定方向且不会移动目标坐标。
- 任务执行器生产默认disabled且`BEE_EXECUTOR_ALLOW_MOTION=0`；网页还需要`BEE_MOTION_ENABLE=1`、实体底盘验收标记、底盘驱动、LD14、地图和Nav2全部就绪才可能下发任务。
- 真实建图必须等待底盘里程计、IMU和TF链路完成。
- 新建地图通过网页保存到板端地图库，后续从地图管理直接加载；地图不保存到浏览器或Windows电脑。
- 板卡电源接口只接受固定`reboot`/`poweroff`动作，通过同源POST、二次确认、参数数组调用和重复请求锁定实现；不提供任意命令执行。
- 板卡健康接口为登录后只读接口；后台缓存内核数据，不为每次浏览器刷新启动外部系统命令。
- 正式客户交付前必须把重启/关机纳入登录后的管理员权限；当前仅用于隔离车载局域网开发。
- `/api/commissioning`只读取最近一次STM32到货预检报告。串口节点存在不代表STM32在线，必须通过显式只读probe取得合法73字节遥测、CRC、序号与频率证据。

## STM32到货预检

- Windows运行`tools/run-stm32-arrival-check.ps1`生成板端报告；默认`ProbeSeconds=0`，不打开串口。
- 车轮架空、确认接线和所有底盘/仿真服务停止后，才显式设置`-Device`、`-Baud 115200`和`-ProbeSeconds`执行只读遥测探测。
- 网站顶部“联调检查”显示设备节点、正式遥测、ROS依赖、安全空闲和当前阻塞项；报告通过仍不等于允许运动。

## 总自动验收

- Windows运行`tools/run-delivery-audit.ps1`，会先检查本地Python/JavaScript语法，再把最新版验收脚本同步到板端并执行只读验收。
- 板端报告固定保存为`/opt/bee-ros-web-console/data/delivery-audit.json`，覆盖网站资源版本、登录与未授权401、地图文件配对、灾备恢复报告、巡逻契约、STM32预检、时间守护、端口、systemd和运动服务fail-closed状态。
- 验收脚本不会启动底盘、Nav2、仿真或地图服务，不发送任何运动命令。`baseline_pass=true`只表示无STM32安全基线通过；`hardware_ready=false`和`hardware_blockers`继续明确列出实车缺口。
- “联调检查”弹窗同时显示网站基线版本、失败/警告数量和STM32/实车阻塞项；网站基线通过不会自动开放运动测试。

## 自动时间同步

- 板卡即使没有可断电保持的RTC，也可以在管理员登录网页后自动使用Windows浏览器UTC校时；页面可见时每10分钟复核一次。
- 时间优先级为：已同步NTP → 最近Windows浏览器可信时间 → 可用RTC → 上次可信时间文件。RTC只是可选回退，不再是运行前提。
- 浏览器只调用认证保护、同源限制的`POST /api/system/time-sync`；网关只能通过sudo执行固定`/usr/local/sbin/bee-time-sync`助手，助手拒绝超范围、错误形状和近期超过48小时的异常跳变。
- 开机时`bee-time-guard`选择系统、RTC和上次可信时间中的最新合法值，防止无后备RTC时系统时间倒退；网页板卡健康会明确显示“NTP自动同步”“Windows浏览器自动校时”“RTC离线回退”或“系统时间未验证”。

## Nav2任务执行器

- `tools/bee_nav_task_executor.py`支持单点、多航点多轮、暂停、恢复、取消、重试、失败和零速度收尾。
- `bee-nav-task-executor-sim.service`只用于手动隔离仿真，使用独立`/run/bee-nav-task-sim`目录；测试结束必须停止并确认inactive、非failed、disabled。
- 当前隔离仿真已通过单点、三航点×2轮、暂停/恢复/取消、不可达失败和取消后速度归零；实体车辆结果仍待硬件到货。

## `/map`测试

- 旧地图若只有`.yaml/.pgm`，可运行`tools/backfill_map_metadata.py`原子补齐名称、场地和创建时间侧车；脚本只创建缺失`.json`，不会覆盖用户已经保存的元数据。

- `tools/test_map_publisher.py`发布明确标记为`bee_test_map`的测试OccupancyGrid，只用于验证颜色和坐标换算。
- `bee-test-map.service`默认不启用；开始真实SLAM前必须停止测试服务，避免两个节点同时发布`/map`。
- 测试地图原点为`(-4.0,-2.5)`、分辨率`0.20m`、原点偏航`10°`，用于验证网页不是直接把像素比例当作地图坐标。

## 前端构建

- `frontend/index-fragment.html` 是已确认UI基准的主体源码。
- 运行 `tools/build-frontend.ps1` 生成可直接部署的 `frontend/index.html`。
- Lucide图标固定使用 `frontend/vendor/lucide.min.js`，板端离线运行不依赖CDN。
