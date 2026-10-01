#ifndef __ROS_PROTOCOL_H__                              // 防止ROS正式协议头文件被重复包含
#define __ROS_PROTOCOL_H__                              // 定义ROS正式协议头文件保护宏

#include "stm32f10x.h"                                 // 提供固定宽度整数类型定义

#define ROS_MESSAGE_CMD_VEL 0x10U                       // 定义连续速度命令消息类型
#define ROS_MESSAGE_HEARTBEAT 0x11U                     // 定义ROS心跳消息类型
#define ROS_MESSAGE_SET_MODE 0x12U                      // 定义模式切换消息类型
#define ROS_MESSAGE_ESTOP 0x13U                         // 定义软件急停消息类型
#define ROS_MESSAGE_TELEMETRY 0x80U                     // 定义STM32完整遥测消息类型
#define ROS_MESSAGE_ACK 0x90U                           // 定义命令确认消息类型

#define ROS_SOURCE_NONE 0U                              // 定义当前没有控制源
#define ROS_SOURCE_BLUETOOTH 1U                         // 定义USART2蓝牙人工控制源
#define ROS_SOURCE_MQTT 2U                              // 定义UART4 MQTT人工控制源
#define ROS_SOURCE_RDK 3U                               // 定义USART1 ROS自主控制源

#define ROS_MODE_MANUAL 0U                              // 定义ROS链释放控制的手动模式
#define ROS_MODE_AUTONOMOUS 1U                          // 定义ROS链申请自主控制模式

#define ROS_STATE_COMMAND_TIMEOUT 0x0001U               // 定义ROS速度命令超时状态位
#define ROS_STATE_ESTOP 0x0002U                         // 定义急停状态位
#define ROS_STATE_MANUAL_OVERRIDE 0x0004U               // 定义蓝牙或MQTT人工抢占状态位
#define ROS_STATE_IMU_CALIBRATING 0x0008U              // 定义上电IMU静止校零进行中状态位
#define ROS_STATE_IMU_NOT_READY 0x0010U                // 定义IMU校零未完成禁止运动状态位

#define ROS_FAULT_IMU_NOT_READY 0x0001U                 // 定义ICM42670未通过初始化故障位
#define ROS_FAULT_IMU_READ 0x0002U                      // 定义ICM42670运行期读取故障位
#define ROS_FAULT_CALIBRATION_PENDING 0x0004U           // 定义轮径轮距编码器或安装方向待标定故障位
#define ROS_FAULT_RX_OVERFLOW 0x0008U                   // 定义USART1接收环形缓冲区溢出故障位
#define ROS_FAULT_IMU_BIAS_PENDING 0x0010U             // 定义ICM42670陀螺仪零偏尚未完成故障位
#define ROS_FAULT_IMU_BIAS_FAILED 0x0020U              // 定义ICM42670校零采样异常故障位

void RosProtocol_Init(uint32_t NowMs);                   // 初始化正式协议解析器、IMU、控制仲裁和遥测状态
void RosProtocol_Process(uint32_t NowMs);                // 解析USART1帧并执行看门狗、IMU采样和遥测发送
uint16_t RosProtocol_Crc16(const uint8_t *Data, uint16_t Length); // 计算CRC-16/CCITT-FALSE用于测试和帧编码
uint8_t RosProtocol_GetControlSource(void);              // 取得当前无、蓝牙、MQTT或ROS控制源
uint16_t RosProtocol_GetStateFlags(void);                // 取得命令超时、急停和人工抢占状态位
uint16_t RosProtocol_GetFaultCode(void);                 // 取得IMU、标定和串口故障位
uint8_t RosProtocol_IsControlReady(void);                // 查询上电静止校零是否完成并允许底盘控制

#endif                                                  // 结束ROS正式协议头文件保护
