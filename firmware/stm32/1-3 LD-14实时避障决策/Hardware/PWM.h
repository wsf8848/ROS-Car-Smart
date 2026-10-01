#ifndef __PWM_H                         //防止PWM头文件被重复包含
#define __PWM_H                         //定义PWM头文件包含保护宏

#include "stm32f10x.h"                 //提供PWM接口所需的固定宽度整数类型

void PWM_ABCD_Init(void);               //初始化TIM8四通道PWM并连接PC6至PC9
void PWM_SetCompare1(uint16_t Compare); //设置左后电机PWM比较值
void PWM_SetCompare2(uint16_t Compare); //设置左前电机PWM比较值
void PWM_SetCompare3(uint16_t Compare); //设置右前电机PWM比较值
void PWM_SetCompare4(uint16_t Compare); //设置右后电机PWM比较值

#endif                                //结束PWM头文件包含保护
