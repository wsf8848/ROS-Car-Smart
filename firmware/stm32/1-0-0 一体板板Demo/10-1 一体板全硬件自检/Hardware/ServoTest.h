#ifndef __SERVO_TEST_H__                               // 防止舵机测试头文件被重复包含
#define __SERVO_TEST_H__                               // 定义舵机测试头文件包含保护宏

#include "stm32f10x.h"                                // 提供STM32F1标准外设类型定义

void ServoTest_Init(void);                             // 初始化TIM8和TIM1的六路50Hz舵机信号
void ServoTest_SetAngle(uint8_t Channel, uint16_t Angle); // 设置指定舵机通道的目标角度
void ServoTest_SetAll(uint16_t Angle);                 // 同时设置六路舵机的目标角度
void ServoTest_DeInit(void);                           // 关闭六路舵机PWM并释放复用引脚

#endif                                                // 结束舵机测试头文件包含保护
