# ESP32-C3 UART5语音模块波特率转换桥

## 功能

本固件将STM32 UART5 TX发出的原始二进制数据，以`230400-8-N-1`接收，经过ESP32-C3缓存后，保持字节顺序和内容不变，以`115200-8-N-1`发送给语音模块。

两个方向使用不同硬件UART，USB CDC只负责下载和调试日志：

```text
STM32 UART5_TX / 230400
        ↓
ESP32-C3 GPIO20 / UART0_RX
        ↓ 透明二进制缓存
ESP32-C3 GPIO21 / UART1_TX
        ↓
语音模块 RX / 115200
```

## 接线

| 设备 | ESP32-C3 Super Mini | 说明 |
| --- | --- | --- |
| STM32 UART5_TX | GPIO20 | ESP32输入，230400 |
| 语音模块RX | GPIO21 | ESP32输出，115200 |
| STM32 GND、语音模块GND | GND | 三方必须共地 |

只允许3.3V TTL串口电平。若语音模块RX不是3.3V兼容输入，必须先增加电平转换，不能直接连接。

## LED状态

- 正在接收数据或最后一次接收后500ms内：GPIO8板载LED快速闪烁，80ms翻转一次。
- 没有接收数据：每秒短闪一次。

## 带宽边界

输入为230400、输出为115200，因此持续满速输入的带宽大于输出能力，任何有限缓冲区最终都会溢出。本固件使用4096字节软件缓冲和2048字节UART RX缓冲，适用于语音模块常见的短小十六进制命令突发；STM32不得持续以230400满速发送长数据流。USB日志会报告`dropped`，正式验收要求始终为0。

## 构建与烧录

```powershell
$env:PYTHONUTF8='1'
python -m platformio run
python -m platformio run -t upload --upload-port COM17
python -m platformio device monitor -p COM17 -b 115200
```

当前基线：ESP32-C3 Super Mini、4MB Flash、Arduino-ESP32 2.0.17、PlatformIO espressif32 7.1.3。

## 2026-09-28实机状态

- 活动端口：COM17，Espressif USB-Serial/JTAG，VID/PID `303A:1001`。
- 实测芯片：ESP32-C3 QFN32 revision v0.4，4MB XMC嵌入式Flash，40MHz晶振。
- 烧录速度：460800，bootloader、分区表、boot_app0和应用区全部`Hash of data verified`。
- 启动日志已确认两个UART、GPIO与缓冲区配置，连续两个5秒周期为`rx=0, tx=0, dropped=0`。
- 当前未连接STM32和语音模块，GPIO20输入到GPIO21输出的外部电气闭环需接线后继续验证。

已归档固件：

| 文件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `已编译固件/V1.0.0/bootloader.bin` | 13248 | `30D47AB1F344CFA69F6B2F718FFA72FC7BAEA6DB047EAABEFEA162DB29E6821C` |
| `已编译固件/V1.0.0/partitions.bin` | 3072 | `148B959CBFF1C38AA8E1D5C0BA9D612C54997B945E56A63F41223EEF650653A1` |
| `已编译固件/V1.0.0/firmware.bin` | 279968 | `9D123E31D0AD7C3EC1800ECC4962E0563AF0BD342C9221FB66075E301BB1D69D` |
