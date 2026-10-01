#ifndef __PWM_H_
#define __PWM_H_
#include "stm32f10x.h"                  // Device header

void PWM_ABCD_Init(void);

void PWM_SetCompare1(uint16_t Compare);
void PWM_SetCompare2(uint16_t Compare);
void PWM_SetCompare3(uint16_t Compare);
void PWM_SetCompare4(uint16_t Compare);

#endif
