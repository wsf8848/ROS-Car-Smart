#ifndef __PORT_TEST_H__                                // 防止串口测试头文件被重复包含
#define __PORT_TEST_H__                                // 定义串口测试头文件包含保护宏

#include "stm32f10x.h"                                // 提供STM32F1标准外设类型定义

#define PORT_TEST_USART1_OK 0x01U                      // 定义USART1回环通过结果位
#define PORT_TEST_USART2_OK 0x02U                      // 定义USART2回环通过结果位
#define PORT_TEST_UART4_OK 0x04U                       // 定义UART4回环通过结果位
#define PORT_TEST_UART5_OK 0x08U                       // 定义UART5回环通过结果位

void PortTest_InitExternal(void);                      // 初始化USART1、UART4和UART5测试端口
void PortTest_DisableUSART1(void);                     // 临时关闭与S4和S5引脚冲突的USART1
uint8_t PortTest_RunLoopback(void);                    // 执行四个串口的TX到RX回环检测

#endif                                                // 结束串口测试头文件包含保护
