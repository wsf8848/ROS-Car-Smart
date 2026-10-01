# 2026-09-12 ESP32-C3 与 STM32 UART4 MQTT 桥接

## 任务目标

参考 STM32 工程 `1-13 UART4接入ESP32C3遥测`，编写并烧录 ESP32-C3 车端固件，实现 STM32 UART4 遥测上行至 MQTT、MQTT 控制下行至 STM32 UART4，为 Retroid 掌机远程遥控全链路做准备。本轮只修改 ESP32-C3 工程，不修改 APP 和 STM32 工程。

## 工作区保护

- APP 当前基线：v2.0.1 / versionCode 26。
- APP 工程在本轮开始前已有大量历史未提交及未跟踪改动，包括 README、CHANGELOG、Gradle、Manifest、蓝牙/控制/UI、MQTT、Web 资源、证据和多个 APK。
- 本轮不回退、不清理、不覆盖上述历史内容，仅新增本任务记忆并更新索引。

## 检查点 1：协议与硬件基线审计

- 状态：完成。
- ESP32-C3：Super Mini、4 MB Flash、板载 LED GPIO8 低有效、原生 USB 调试端口当前为 COM8。
- 购买资料引脚图确认 GPIO20 标注 RX、GPIO21 标注 TX，计划用 `Serial1` 映射为 RX=20、TX=21，USB CDC 继续用于 115200 调试日志。
- STM32 UART4：PC10/TX、PC11/RX，`115200-8-N-1`、3.3V TTL、无流控，必须共地。
- 接线：PC10 → ESP32 GPIO20；PC11 ← ESP32 GPIO21；GND ↔ GND。禁止接 5V TTL 或 RS-232 电平。
- STM32 遥测帧：`55 51 ... 5B` 9 字节雷达/车辆状态、`55 53 ... 5B` 5 字节电池、`55 52 ... 5B` 8 字节底盘状态，10 ms 节拍连续输出。
- STM32 控制命令当前为单字节：`0x41~0x48` 八方向、`0x49` 停车、`0x5B/0x5C/0x5D` 手动/避障/自主、`0x6A/0x6B` 减速/加速。
- 关键边界：STM32 1-13 已启用 UART4 RX 中断和 64 字节环形缓冲，但 `main.c` 尚未调用 `Esp32Link_ReadByte()` 接入控制解析。因此 ESP32 下发可完成，实际电机控制仍需要后续 STM32 软件适配，不能冒充全链路已控车。
- MQTT 当前只确认控制 Payload 固定 20 字节，字段未定稿。本轮 ESP32 将采用兼容安全策略：只从 20 字节中提取经过明确白名单验证的 STM32 单字节命令；不把任意 20 字节整体灌入 UART4。
- 未验证边界：STM32 尚未连接到 ESP32-C3，本轮开始时没有 UART4 实际波形或遥测字节证据。
- 下一步：设计可向后兼容的 20 字节桥接封装、实现非阻塞 UART 帧解析和双向 MQTT 转发。

## 检查点 2：ESP32-C3 双向桥接实现

- 状态：完成，待编译与烧录验证。
- 新建独立工程：`1-3 ESP32C3工程代码\1-3 ESP32C3_STM32_UART4_MQTT桥接`，保留此前纯 MQTT 测试工程不覆盖。
- GPIO20 配置为 STM32 遥测 RX，GPIO21 配置为 STM32 控制 TX；USB CDC 独立输出调试日志。
- 实现 `0x51/0x52/0x53` 三类 UART 帧的流式重同步、长度/包尾验证、最新帧缓存和错误计数。
- 三类最新帧合并为最多 22 字节，以 20 Hz 发布到既有 telemetry 主题；与掌机 `SmartCarTelemetry.feedRx()` 的连续帧解析兼容。
- 定义 20 字节控制封装：`55 43 01 序号 命令 保留区 CRC8 5B`，CRC-8/ATM 多项式 `0x07`。
- 只允许 STM32 源码已确认的方向、停车、模式和调速命令；拒绝错误长度、帧头、版本、包尾、CRC、保留区、未知命令和重复/旧序号。
- 运动期间 300 ms 无新方向帧、非法控制、Wi-Fi 或 MQTT 断线均触发 STM32 `0x49` 三连停车；上电也发送 `0x49` 三次。
- 状态主题改为 JSON，保留在线状态并增加 UART 帧数、UART 错误、控制接收数、控制错误、最后序号/命令和 RSSI；离线遗嘱为 JSON。
- 增加 `safe_control_probe.py`，只通过公网发送安全停车命令并等待 ESP32 状态回执，不在未接车条件下发送运动命令。
- 保留当前已验证的证书指纹前置校验：指纹不匹配时在发送车辆账号密码之前断开。其证书续期限制仍然存在，未伪装为量产 TLS 方案。
- 增加 `接线与协议说明.md`，记录接线、控制帧、遥测聚合和 STM32 当前未消费 UART4 RX 的边界。
- 下一步：使用 C3 专用中文路径构建脚本编译，处理所有编译错误后再识别端口并烧录。

## 检查点 3：ESP32-C3 编译验证

- 状态：完成。
- 使用 `esp32c3_supermini.ps1 -Action build` 将中文路径工程镜像到 ASCII 暂存目录，并使用缓存工具链构建。
- PlatformIO：Espressif32 7.1.3；Arduino-ESP32 2.0.17；PubSubClient 2.8.0。
- 编译结果：`SUCCESS`，耗时 17.35 秒，无编译错误。
- RAM：40,460 / 327,680 字节（12.3%）。
- Flash：896,466 / 1,310,720 字节（68.4%）。
- `bootloader.bin`、`partitions.bin`、`firmware.bin` 已回拷到工程 `已编译固件`。
- 敏感配置加载日志只显示 values hidden，没有输出 Wi-Fi 或 MQTT 密码。
- 下一步：枚举 USB 端口，确认唯一 ESP32-C3 后烧录，不使用猜测端口。

## 检查点 4：COM8 烧录与公网安全控制验证

- 状态：ESP32 端完成；UART 接线与整车验证待后续。
- 端口枚举只有 `USB 串行设备 (COM8)`，USB ID 为 `VID_303A&PID_1001`，确认后烧录。
- 芯片识别：ESP32-C3 QFN32 revision v0.4；4 MB XMC Flash；USB-Serial/JTAG。
- 使用 460800 bit/s 写入 bootloader、分区表和 917,952 字节应用镜像；所有段均 `Hash of data verified`，Hard resetting 成功。
- 为避免运动风险，公网探针只发送正式 20 字节停车命令 `0x49`，序号 66，CRC-8 为 `0xDA`，Retain false。
- 控制端收到 ESP32 JSON 回执：`online=true`、`bridge=esp32c3-uart4`、`control_rx=1`、`control_errors=0`、`last_seq=66`、`last_cmd=73`，结果 `SAFE_STOP_ACK_PASS`。
- ESP32 USB 日志确认 `[UART TX] STM32 command=0x49` 和 `[MQTT RX] accepted seq=66 command=0x49 total=1`。
- 当前 `uart_frames=0`、`uart_errors=0` 与 STM32 尚未接线事实一致；未伪造 UART 遥测或车轮动作证据。
- 最终产物已保存在 `已编译固件`，完整 SHA-256 与接线验收步骤见 `烧录与联调报告.md`。
- 下一步：用户按说明接线；然后抓取三类 UART 遥测。要实现真实遥控，还需在 STM32 主循环消费 `Esp32Link_ReadByte()` 并复用控制白名单/失联停车逻辑，同时让 APP 按本轮确定的 20 字节格式发布控制帧。

## 本轮结论

- 用户要求的 ESP32-C3 端代码编写、编译、烧录和 MQTT 安全控制下行验证已经完成。
- 全链路尚缺两项明确工作：STM32 UART4 RX 控制解析、掌机正式控制帧发布。它们不属于“仅修改 ESP32 端”范围，本轮保持原工程不动并如实记录。

## 检查点 5：文档与 Skill 沉淀

- 状态：完成。
- 新增 ESP32 工程内 `烧录与联调报告.md` 和 `接线与协议说明.md`，保存可执行接线、协议、安全策略、验证证据、产物 SHA-256 与未验证边界。
- 更新 MQTT 远程通信基线，记录 C3 第一版正式 20 字节封装，同时保留 APP/STM32 尚未适配的事实。
- 将 C3/UART4 引脚、遥测聚合、控制封装、命令白名单、安全不变量、已验证与未验证边界同步进 `xiaomifeng-robot-controller` Skill；将购买资料标注的 GPIO20/RX、GPIO21/TX 和电气验证边界同步进 `esp32-esp8266-dev` 板型资料。
- 使用 `PYTHONUTF8=1` 分别运行 `skill-creator/scripts/quick_validate.py`，两个 Skill 均返回 `Skill is valid!`。
- 未写入 MQTT 账号密码、Wi-Fi 密码、私钥或签名密码。
