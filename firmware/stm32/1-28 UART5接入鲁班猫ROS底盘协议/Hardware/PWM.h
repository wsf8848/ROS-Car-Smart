#ifndef __PWM_H__                                      // 防止PWM头文件被重复包含
#define __PWM_H__                                      // 定义PWM头文件包含保护宏

#include "stm32f10x.h"                                // 提供STM32F1标准外设类型定义

void PWM_ABCD_Init(void);                              // 初始化TIM8四路电机PWM输出
void PWM_SetCompare1(uint16_t Compare);                // 更新TIM8通道1比较值
void PWM_SetCompare2(uint16_t Compare);                // 更新TIM8通道2比较值
void PWM_SetCompare3(uint16_t Compare);                // 更新TIM8通道3比较值
void PWM_SetCompare4(uint16_t Compare);                // 更新TIM8通道4比较值

#endif                                                // 结束PWM头文件包含保护
