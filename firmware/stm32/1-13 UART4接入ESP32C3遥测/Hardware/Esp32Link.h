#ifndef __ESP32_LINK_H__                                  //防止ESP32-C3串口链路头文件被重复包含
#define __ESP32_LINK_H__                                  //定义ESP32-C3串口链路头文件包含保护宏

#include "stm32f10x.h"                                   //提供固定宽度整数类型和STM32外设定义

#define ESP32_LINK_BAUD_RATE 115200U                       //定义STM32与ESP32-C3之间的UART4通信波特率
#define ESP32_LINK_RX_BUFFER_SIZE 64U                      //定义后续远程控制透传使用的接收环形缓冲区容量

void Esp32Link_Init(void);                                //初始化UART4的PC10发送和PC11接收链路
void Esp32Link_SendByte(uint8_t Byte);                     //通过UART4向ESP32-C3发送一个字节
void Esp32Link_SendArray(const uint8_t *Array, uint16_t Length); //通过UART4向ESP32-C3发送指定长度字节数组
uint8_t Esp32Link_ReadByte(uint8_t *Byte);                 //从UART4接收环形缓冲区读取一个字节
uint32_t Esp32Link_GetRxOverflowCount(void);               //取得UART4接收缓冲区累计溢出次数

#endif                                                     //结束ESP32-C3串口链路头文件包含保护
