#ifndef __MOTOR_H
#define __MOTOR_H
#include "stm32f10x.h"                  // Device header

void MotorABCD_Init(void);

void MotorA_SetSpeed(int16_t Speed);
void MotorB_SetSpeed(int16_t Speed);
void MotorC_SetSpeed(int16_t Speed);
void MotorD_SetSpeed(int16_t Speed);

void Set_PWM(int16_t PWM1,int16_t PWM2,int16_t PWM3,int16_t PWM4);


#endif