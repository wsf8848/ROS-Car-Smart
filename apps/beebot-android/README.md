<div align="center">

# 🐝 小蜜蜂智控系统

**Bee Intelligent Control System**

</div>

> 面向 **Retroid Pocket 4 安卓掌机** 的公网车辆智控 App。v2.6.3 在车辆主页接入萤石 EZOpenSDK 实时预览播放器，保留 MQTT、蓝牙、遥测、急停与语音控制的独立链路；同时保留离线关键词语音助手、中英双语实时识别、离线声纹归属与原有 HTTP/MJPEG/JPEG/RTSP 摄像头工具。

---

## 📑 目录

- [✨ 功能特性](#-功能特性)
- [🎮 硬件与输入信号](#-硬件与输入信号)
- [🏗️ 工程架构](#️-工程架构)
- [🧭 信号采集与协议](#-信号采集与协议)
- [🔐 登录与自动登录](#-登录与自动登录)
- [🛠️ 构建与安装](#️-构建与安装)
- [⚠️ 已知限制与坑](#️-已知限制与坑)
- [🗺️ 后续规划](#️-后续规划)
- [📦 版本](#-版本)

---

## ✨ 功能特性

| 模块 | 说明 |
|---|---|
| 🚙 全屏车辆主控台 | 摄像头背景 + 左上车辆/MQTT 状态及工具入口 + 中上遥测胶囊 + 右侧解锁/上锁/鸣笛/停车/通信切换/车辆模式 |
| 🔀 双层模式 | 通信方式独立切换 WiFi模块/蓝牙；车辆模式独立循环手动/避障/自主，任一车辆模式均可走两种链路 |
| 🔒 控制授权 | 默认上锁；上锁立即停车并阻断摇杆、调速和模式命令，解锁后恢复；停车始终可用 |
| 📣 连续鸣笛 | 一次点击开启非阻塞连续双响，再次点击关闭；APP 500ms 保活，STM32 1.5s 失联自动静音 |
| ☁️ MQTT 公网链路 | WSS 异步连接、自动重连、20Hz 原始二进制控制、状态/遥测订阅与 ESP32 命令回执；凭据由工程外 env 构建期注入 |
| 📊 遥测总览 | 电量/电压、四驱轮速、Pitch/Roll/Yaw、四向雷达、手动/避障/自主模式 |
| 🔊 中文语音 | 登录欢迎、动作反馈和模式切换语音提示 |
| 🎙️ 离线语音识别 | sherpa-onnx + 中英双语流式 Paraformer INT8；普通麦克风 16kHz 实时转文字，支持暂停、清空、复制、端点分句和电平显示 |
| 💬 离线语音助手 | 无大模型、无云端 API；按关键词/同义词识别意图，读取掌机电量、充电、温度、时间、网络及新鲜车辆遥测并用 Android TTS 回答 |
| 🕹️ 双摇杆监视 | 左右摇杆圆形可视化 + X/Y 归一化模拟量（-1.00 ~ +1.00），实时光点跟随 |
| 🎯 按键监视 | ABXY、L1/L2/L3、R1/R2/R3、SELECT/START、十字方向键，按下即点亮 |
| 🎚️ 模拟扳机 | L2/R2 扳机模拟量以百分比显示 |
| 📜 实时信号日志 | 事件驱动打印每一次按键 KEY_DOWN/KEY_UP（含安卓键码）与摇杆数值快照 |
| 🔐 登录鉴权 | 账号密码登录，可勾选"下次自动登录"，SharedPreferences 持久化 |
| 📶 状态顶栏 | 手柄接入状态、WiFi 连接状态 + 网关延迟(ms)、工具入口 |
| 📡 蓝牙调试 | 经典蓝牙 SPP 串口助手：设备列表、波特率、收发日志、HEX/UTF-8 编码切换、定时发送(10ms/500ms/1S)、接收频率(ms)、RX/TX 统计 |
| 🤖 智能小车控制 | 复用阿克曼 8 方向蓝牙遥控、速度档和急停；解析二进制遥测包显示 LD-14 前后左右距离、雷达/小车状态、电源电压/百分比、四电机速度与陀螺仪角度 |
| 📷 摄像头画面 | 通过掌机 WiFi 接入 ESP32-CAM 或其他局域网摄像头，支持 HTTP/MJPEG、JPEG 与 RTSP 地址、连接状态和全屏预览 |
| 🚗 阿克曼小车遥控 | 实体摇杆（左Y油门+右X转向）→ WHEELTEC 底盘 APP 单字节协议控制；解析 `{A}` 帧实时显示左右电机速度/电量/角度；紧急停车 |
| 🌐 ROS 网页控制台 | 内嵌 WebView 加载**内置于 APK** 的 ROS 控制台网页，经 WiFi 连树莓派 `rosbridge`/`web_video_server`：遥控/数据/雷达/地图/摄像头/（预留）一键建图导航一页搞定。可改树莓派 IP、刷新、全屏；库全部离线内置，无外网可用 |
| 📳 震动反馈 | 掌机马达触觉反馈，三档强度（轻 tick/中 click/强双震），紧急停车用双震；设置页可总开关 |
| ⚙️ 设置页 | Tab 页含显示交互（屏幕常亮/按键震动/摇杆死区）、机器人连接（预留）、关于、退出登录 |
| 🧭 工具页 | 点左下 ⋮ 打开左侧纵向导航：「掌机监视 / 蓝牙调试 / 智能小车 / 阿克曼小车 / ROS网页 / 摄像头画面 / 语音识别 / 系统设置」 |
| 🎨 视觉 | 科技深色 HUD 主题，横屏适配掌机；AI 设计的六边形蜂巢自适应图标 |

## 🎮 硬件与输入信号

| 项 | 值 |
|---|---|
| 目标设备 | **Retroid Pocket 4** |
| 系统 | **Android 11 (API 30)** |
| CPU | arm64-v8a |
| 输入接口 | 标准安卓 `InputDevice` / `MotionEvent` / `KeyEvent`（兼容性最佳，非厂商私有） |

采集的信号一览：

```
摇杆(模拟量 -1..+1)   左: LX LY      右: RX RY
扳机(模拟量 0..1)     L2            R2
肩键/摇杆键           L1 L3         R1 R3
功能键               ABXY  SELECT  START
方向键               ↑ ↓ ← →（键值/HAT 轴双通道兼容）
```

## 🏗️ 工程架构

分层设计，采集 / 界面 / 通讯解耦，方便后期插入机器人控制模块：

```
com.fyx.beebot
├── MainActivity.kt              入口：沉浸横屏 · 截获手柄事件 · 自动登录 · MQTT/TTS 初始化
├── mqtt/RemoteVehicleRepository.kt  WSS MQTT · 自动重试/重连 · 状态与遥测订阅
├── speech/SpeechRecognitionController.kt  离线模型 · 16kHz 麦克风 · 流式解码与端点分句
├── input/                       ▼ 采集层（单一数据源）
│   ├── GamepadModels.kt           GamepadButton(键码↔标签) · GamepadState 快照 · InputEvent
│   └── GamepadRepository.kt       MotionEvent/KeyEvent → 归一化状态 + 事件日志
├── bt/                          ▼ 蓝牙 SPP 层
│   ├── BluetoothModels.kt         设备 · 收发帧 · HEX/UTF-8 编解码 · 波特率表
│   ├── BluetoothRepository.kt     SPP 连接/读写线程 · RX 频率 · 收发统计 · 配对列表 · 原始字节回调
│   ├── AckermannController.kt     ⭐ 8方向遥控：摇杆→APP 单字节协议 · 解析 {A} 遥测
│   └── SmartCarTelemetry.kt       智能小车 0x55/0x51、0x52 二进制遥测流解析
├── ui/                          ▼ 界面层（科技深色 HUD）
│   ├── theme/Theme.kt             Hud 调色板 · BeeBotTheme
│   ├── ControlLoginScreen.kt      车辆主题动画登录 · 自动登录 · 持久化
│   ├── VehicleDashboardScreen.kt  全屏摄像头主控 · 遥测胶囊 · 车辆动作与模式
│   ├── MainScreen.kt              掌机监视：双摇杆 · 全按键 · 实时日志
│   ├── ToolsScreen.kt             ⋮ 工具中心 · 左侧纵向导航 · 旧功能保留
│   ├── BluetoothScreen.kt         蓝牙串口助手（设备/波特率/收发/编码/定时/统计）
│   ├── AckermannScreen.kt         阿克曼控制页（遥控源/转向油门可视化/遥测）
│   ├── SmartCarScreen.kt          智能小车页（四向雷达/8方向控制/底盘遥测）
│   ├── CameraStreamScreen.kt      WiFi 摄像头推流页（HTTP/MJPEG/JPEG/RTSP/全屏）
│   ├── SpeechRecognitionScreen.kt 离线语音对话（说话人/问题/真实状态回答）
│   └── SettingsScreen.kt          设置内容（显示/连接预留/关于/退出）
├── util/
│   ├── WifiStatus.kt             WiFi 名称 · 信号% · 网关 ping 延迟
│   ├── VoicePrompts.kt           Android TTS · 播报完成回调 · ASR 防回声接线
│   └── Haptics.kt                震动反馈 tick/click/heavy 三档 · 全局开关
├── speech/
│   ├── VoiceAssistantEngine.kt   固定意图解析 · 确定性回答
│   └── VoiceAssistantController.kt 真实状态快照 · 对话历史 · ASR/TTS 闭环
└── transport/RobotTransport.kt  ⭐ 原有通讯层接口继续保留
```

## 🧭 信号采集与协议

- **摇杆**：`AXIS_X/Y`（左）、`AXIS_Z/RZ`（右，回退 `RX/RY`）、`AXIS_LTRIGGER/RTRIGGER`（回退 `BRAKE/GAS`）、`AXIS_HAT_X/Y`（方向键）；用 `InputDevice.getMotionRange(axis).flat` 做死区。
- **按键**：`KeyEvent.KEYCODE_BUTTON_*` 映射到业务枚举 `GamepadButton`，后续控制协议直接引用枚举，无需再碰底层键码。
- **界面刷新**：采用 Compose 快照状态（`mutableStateOf`/`mutableStateListOf`），界面读即订阅、输入变即刷新。
- **按键面布局**：`X上 · Y左 · A右 · B下`。

> 💡 后续接机器人只需实现 `transport/RobotTransport` 接口（`connect / disconnect / sendControl(GamepadState)`），把手柄状态编码成对应运动协议帧下发即可，**界面与采集层零改动**。

## 🔐 登录与自动登录

- 首次进入为登录页；勾选"下次自动登录"后，凭据经校验存入 SharedPreferences，之后启动直达主页。
- 登录成功播放“小蜜蜂智控系统”欢迎语；退出登录会关闭自动登录。
- ⚠️ `install -r` 覆盖安装会保留 App 数据，故**升级后无需重新登录**。

## 🛠️ 构建与安装

**环境**：JDK 17 · Gradle 8.5 · AGP 8.1.1 · Kotlin 1.8.20 · Compose Compiler 1.4.6 · Build-Tools/targetSdk 34。

```bash
# 编译 Release
export JAVA_HOME="<JDK17>"; export PATH="$JAVA_HOME/bin:$PATH"
gradle :app:assembleRelease --init-script tools/gradle-cn-mirror.init.gradle

# 安装到掌机（-r 保留数据/自动登录）
adb -s <设备号> install -r dist/小蜜蜂智控系统_v<版本>.apk
adb -s <设备号> shell am start -n com.fyx.beebot/.MainActivity
```

> ⚠️ 签名密钥 `keystore/*.jks` 与 `release-signing.properties` 为敏感文件，**已被 .gitignore 排除、不在仓库内**；本地缺失时 `assembleRelease` 产出未签名包。请自行放置或生成签名配置。

## 📡 蓝牙调试（SPP 串口助手）

- 连接对象：小车上的**经典蓝牙 HC-05/HC-06** 串口透传模块。
- ⚠️ **必须先在系统蓝牙设置里配对** HC-05（PIN 一般 `1234`/`0000`），App 里"扫描"列出的是**已配对**设备，点选即连。
- 收 / 发均可独立切换 **HEX / UTF-8** 编码；发送区支持**定时发送**（10ms / 500ms / 1S）；顶栏显示**接收频率**（相邻两帧间隔 ms）与 RX/TX 字节统计。
- ⚠️ **波特率下拉仅 App 内保存/显示**：SPP 数据透传时安卓端无法直接改写模块 UART 波特率（波特率烧在 HC-05 里，需模块进 AT 模式发 `AT+UART` 才能改），故此处不强制、不影响收发。

## 🚗 阿克曼小车遥控（WHEELTEC 底盘协议）

参考 WHEELTEC(轮趣) 阿克曼底盘 STM32 源码逆向，蓝牙(HC-05)接底盘 **USART2@115200**，双向 ASCII/字节协议：

**① 控制（App → 小车，APP 单字节协议）**
```
进入遥控：连发两个 'A'(0x41) → STM32 置 APP_ON_Flag=1，再发 'I' 停车回中
方向 A~H(0x41~48) 设 Flag_Direction=1~8（车端 balance.c Get_RC Akm：Move_Z +PI/2=左 / -PI/2=右）
   'A'前直  'B'前+右  'C'停+右打舵  'D'后+右
   'E'后直  'F'后+左  'G'停+左打舵  'H'前+左
停车+回中：'I'(0x49) → Flag_Direction=0 → Move_X=0,Move_Z=0（并置 Turn_Flag=0 强制方向接口）
调速（阿克曼页）：'X'(0x58)=+100mm/s   'Y'(0x59)=-100mm/s   （RC_Velocity 档位）
智能小车页调速：L2 发送 `0x6A` 减速一档，R2 发送 `0x6B` 加速一档；按住每250ms一档，页面速度镜像每档 ±100mm/s
需以固定周期(100ms)持续发送，维持底盘失控保护(command_lost)不复位
```
摇杆映射：**左摇杆 Y = 油门（前/后）**，**右摇杆 X = 转向（左/右，无油门时原地打舵）**。智能小车页使用 L2/R2 调速；阿克曼页保留 R2/R1、L2/L1 兼容行为。
⚠️ 停车**必须**用 'I' 不能用 'C'——'C' 在固件里是"停+右打舵"，误用会导致松手舵机往右打（v1.3.1 已修）。

**② 遥测（小车 → App，`printf` 重定向到 USART2）**
```
{A<左轮*100>:<右轮*100>:<电量%>:<陀螺Z>}$   → 本页解析：左右电机速度(m/s)、电量、角度
{B<gx>:<gy>:<gz>}$   陀螺波形
{C<RC_Velocity>:<KP>:<KI>}$   参数
```

- ⚠️ APP 协议是 **8 方向离散**控制（非完全比例跟手），这是底盘蓝牙口(USART2)固件决定的；真·模拟量(Vx/Vz 比例+阿克曼转角自动解算)在固件里是 USART1/3/5 的 11 字节协议，蓝牙口不解析。
- ✅ 左右转极性已按固件约定并经真车"松手往右"症状印证修正（v1.3.1）；若某车相反，在 `AckermannController.mapCommand` 对调左右两组即可。
- ⚠️ 底盘**上电前 10 秒忽略指令**（CONTROL_DELAY）。
- 先在「蓝牙调试」页连上 HC-05，再进「阿克曼小车」页点“开始遥控”。

## ☁️ ESP32-C3 公网控制协议

主控台使用 `car/001/control` 发布原始 20 字节 Payload，QoS 0、Retain false：

```text
55 43 01 <seq> <cmd> 00 00 00 00 00 00 00 00 00 00 00 00 00 <crc8> 5B
```

- `seq` 每次成功发布递增并允许 `255 -> 0` 回绕；`crc8` 为 CRC-8/ATM（多项式 `0x07`），覆盖字节 0~17。
- 左摇杆 Y + 右摇杆 X 映射 `0x41~0x48` 八方向，50ms 周期刷新；回中、A 键、打开工具页或退出 App 主动三连 `0x49` 停车。
- 模式为 `0x5B/0x5C/0x5D`，L2/R2 为 `0x6A/0x6B`；自主模式不发送摇杆方向帧。
- ESP32-C3 将合法命令转成 UART4 单字节发给 STM32；APP 从状态主题解析 `last_seq/last_cmd/control_rx/control_errors/uart_frames` 回显真实桥接状态。
- 遥测 Payload 为 `0x51(9B) + 0x53(5B) + 0x52(8B)` 原始帧拼接；四轮字段按 int8 二补码显示十毫秒编码器脉冲。
- 解锁、上锁、鸣笛没有车端白名单命令，当前仍明确阻断，禁止猜测下发。

## ⚠️ 已知限制与坑

- ⚠️ **WiFi 名称在 Retroid 定制 ROM 上读不到**：即便已授予 `ACCESS_FINE_LOCATION` + 开启定位 + 放开 appop，系统仍屏蔽第三方 App 读取 SSID（返回 `<unknown ssid>`）。故顶栏在本掌机回退显示"WiFi已连接"；**在普通手机上可正常显示真实名称**。信号强度与延迟不受限。
- ⚠️ **版本锁死**：Kotlin `1.8.20` ↔ Compose Compiler `1.4.6` 必须配套，错配会"编译通过但运行闪退"。
- ⚠️ **中文工程路径**：`gradle.properties` 必须保留 `android.overridePathCheck=true`。
- ⚠️ **中文路径运行 JVM 单测**：若测试类已生成但 Gradle 报 `ClassNotFoundException`，从 ASCII 目录联接（如 `C:\Users\Bee\codex-work\BeeRobotController`）运行测试；这是测试 worker classpath 编码问题。
- ⚠️ **语音模型体积**：v2.4.0 把约 237MB 的 INT8 模型完整放入 APK，因此安装包约 279MB；当前仅交付 Retroid 所需的 `arm64-v8a`。
- ⚠️ **横屏高度有限**：掌机横屏可视高 ~750px，登录卡片用 `verticalScroll` + 紧凑间距，避免登录按钮被裁切。

## 🗺️ 后续规划

- [x] 小蜜蜂智控系统登录、自动登录、欢迎语与版本管理
- [x] 全屏摄像头主控台、遥测胶囊、车辆在线与 MQTT 状态
- [x] WSS MQTT 安全配置注入、自动重试、状态/遥测订阅
- [x] 原掌机监视和全部蓝牙/小车/ROS/摄像头功能迁入工具中心
- [x] 按 ESP32-C3 实际协议接通 20 字节公网控制发布、状态回执和遥测解析
- [x] STM32 UART4 消费 ESP32-C3 控制字节并复用 350ms 失控停车状态机（代码已完成，待 Keil 烧录与架空车轮实测）
- [ ] 完成 ESP32-C3/STM32 接线、STM32 新固件烧录及控制/遥测/遗嘱离线全链路实车验收
- [x] 蓝牙 SPP 串口助手（收发/编码/定时/统计）
- [x] 阿克曼小车摇杆遥控（APP 单字节协议）+ `{A}` 遥测显示
- [x] 智能小车 8 方向遥控 + LD-14 四向距离/状态/底盘二进制遥测
- [x] WiFi 摄像头推流页（ESP32-CAM/HTTP/MJPEG/JPEG/RTSP）
- [x] 完全离线的中英双语实时语音识别页面（sherpa-onnx + Paraformer）
- [x] 无大模型离线关键词语音助手（真实状态查询、说话人问答记录、TTS 防回声）
- [ ] 差速/麦轮/全向等更多车型控制页
- [ ] 真·模拟量比例控制（需底盘蓝牙改接 USART1/3/5 或改固件）
- [ ] 抽象 `RobotTransport`，统一 BLE / WiFi-UDP / 串口 通道
- [ ] 控制页：转向极性/死区/速度上限可视化配置
- [ ] 连接历史与一键重连

## 📦 版本

当前版本 **v2.6.3**。完整迭代记录见 [CHANGELOG.md](CHANGELOG.md)。

---

<div align="center">
<sub>🐝 小蜜蜂机器人 · 作者 FYX · GitHub <a href="https://github.com/wsf8848">@wsf8848</a></sub>
</div>
