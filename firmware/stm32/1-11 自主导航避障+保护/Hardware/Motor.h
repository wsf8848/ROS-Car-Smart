#ifndef __MOTOR_H__                                    // 防止电机头文件被重复包含
#define __MOTOR_H__                                    // 定义电机头文件包含保护宏

#include "stm32f10x.h"                                // 提供STM32F1标准外设类型定义

void MotorABCD_Init(void);                             // 初始化四个电机方向引脚和PWM输出
void MotorA_SetSpeed(int16_t Speed);                   // 设置左后轮方向与PWM速度
void MotorB_SetSpeed(int16_t Speed);                   // 设置左前轮方向与PWM速度
void MotorC_SetSpeed(int16_t Speed);                   // 设置右前轮方向与PWM速度
void MotorD_SetSpeed(int16_t Speed);                   // 设置右后轮方向与PWM速度
void Set_PWM(int16_t PWM1, int16_t PWM2, int16_t PWM3, int16_t PWM4); // 同时设置四个轮子的有符号PWM

#endif                                                // 结束电机头文件包含保护
