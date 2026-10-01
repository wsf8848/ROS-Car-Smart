#ifndef __ROS_LINK_H__                                  // 防止鲁班猫ROS串口头文件被重复包含
#define __ROS_LINK_H__                                  // 定义鲁班猫ROS串口头文件保护宏

#include "stm32f10x.h"                                 // 提供UART5外设和固定宽度整数类型定义

#define ROS_LINK_BAUD_RATE 115200U                      // 定义STM32与鲁班猫ROS板之间的通信波特率
#define ROS_LINK_RX_BUFFER_SIZE 128U                    // 定义ROS控制命令接收环形缓冲区容量

void RosLink_Init(void);                                // 初始化UART5的PC12发送和PD2接收引脚
void RosLink_SendByte(uint8_t Byte);                    // 通过UART5发送一个字节
void RosLink_SendArray(const uint8_t *Array, uint16_t Length); // 通过UART5发送指定长度的字节数组
uint8_t RosLink_ReadByte(uint8_t *Byte);                // 从UART5接收环形缓冲区读取一个字节
uint32_t RosLink_GetRxOverflowCount(void);              // 取得UART5接收缓冲区累计溢出次数

#endif                                                 // 结束鲁班猫ROS串口头文件保护
