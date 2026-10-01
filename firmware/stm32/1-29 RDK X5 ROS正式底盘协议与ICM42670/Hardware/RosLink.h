#ifndef __ROS_LINK_H__                                  // 防止地瓜派RDK X5ROS串口头文件被重复包含
#define __ROS_LINK_H__                                  // 定义地瓜派RDK X5ROS串口头文件保护宏

#include "stm32f10x.h"                                 // 提供USART1外设和固定宽度整数类型定义

#define ROS_LINK_BAUD_RATE 115200U                      // 定义STM32与地瓜派RDK X5ROS板之间的通信波特率
#define ROS_LINK_RX_BUFFER_SIZE 600U                    // 定义可容纳一个最大正式协议帧的接收环形缓冲区容量

void RosLink_Init(void);                                // 初始化USART1的PA9发送和PA10接收引脚
void RosLink_SendByte(uint8_t Byte);                    // 通过USART1发送一个字节
void RosLink_SendArray(const uint8_t *Array, uint16_t Length); // 旧遥测兼容空接口，不向USART1混发旧协议
uint8_t RosLink_ReadByte(uint8_t *Byte);                // 旧控制兼容空接口，不读取USART1正式协议字节
void RosLink_SendProtocolArray(const uint8_t *Array, uint16_t Length); // 通过USART1发送正式AA55协议帧
uint8_t RosLink_ReadProtocolByte(uint8_t *Byte);        // 从USART1读取正式协议解析器使用的一个字节
uint32_t RosLink_GetRxOverflowCount(void);              // 取得USART1接收缓冲区累计溢出次数

#endif                                                 // 结束地瓜派RDK X5ROS串口头文件保护
