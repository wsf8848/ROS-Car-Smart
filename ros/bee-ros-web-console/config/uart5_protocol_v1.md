# Bee UART5底盘协议 v1

串口：115200-8-N-1。多字节字段均为小端。

## 通用帧

`AA 55 | version:u8 | type:u8 | payload_len:u16 | seq:u16 | stm32_time_ms:u32 | payload | crc16:u16`

- CRC：CRC-16/CCITT-FALSE，多项式`0x1021`、初值`0xFFFF`，覆盖`version`至`payload`，不含帧头和CRC本身。
- 最大payload：512字节。
- 序号：各发送端独立递增，接收端统计跳号；回绕按u16处理。
- STM32时间戳：上电单调毫秒计数，RDK不得把它直接当UTC。

## 类型

- `0x10 CMD_VEL`：`linear_x:f32, angular_z:f32, requested_source:u8`。
- `0x11 HEARTBEAT`：`source:u8`。
- `0x12 SET_MODE`：`mode:u8`，模式切换须先停车。
- `0x13 ESTOP`：`asserted:u8`。
- `0x80 TELEMETRY`：四轮ticks`4*i32`、四轮速度`4*f32`、加速度`3*f32`、角速度`3*f32`、温度/电压/电量`3*f32`、控制源`u8`、状态位`u16`、故障码`u16`。
- `0x90 ACK`：`acked_type:u8, result:u8`。

控制源：0无、1蓝牙、2 MQTT、3 ROS。状态位：bit0命令超时、bit1急停、bit2人工抢占。

## 安全约束

- STM32仅在控制权为ROS且帧CRC/版本/长度合法时采纳连续速度。
- 合法ROS速度超过350ms未刷新必须在STM32本地归零；RDK心跳不能替代该停车看门狗。
- 蓝牙/MQTT人工控制或安全保护取得控制权时，ROS非零速度不得生效。
- 轮径、轮距、编码器CPR、IMU安装外参不属于协议常量，到货后标定并放入参数文件。
