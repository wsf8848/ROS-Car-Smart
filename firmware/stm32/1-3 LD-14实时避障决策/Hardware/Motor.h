#ifndef __MOTOR_H                        //防止电机头文件被重复包含
#define __MOTOR_H                        //定义电机头文件包含保护宏

#include "stm32f10x.h"                 //提供电机接口所需的固定宽度整数类型

void MotorABCD_Init(void);              //初始化四个电机方向引脚和TIM8 PWM
void MotorA_SetSpeed(int16_t Speed);    //设置左后电机方向和PWM速度
void MotorB_SetSpeed(int16_t Speed);    //设置左前电机方向和PWM速度
void MotorC_SetSpeed(int16_t Speed);    //设置右前电机方向和PWM速度
void MotorD_SetSpeed(int16_t Speed);    //设置右后电机方向和PWM速度
void Set_PWM(int16_t MotorA, int16_t MotorB, int16_t MotorC, int16_t MotorD);//同时设置四轮有符号速度

#endif                                //结束电机头文件包含保护
