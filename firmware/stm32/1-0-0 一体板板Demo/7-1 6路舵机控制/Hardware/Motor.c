#include "stm32f10x.h"                  // Device header
#include "Motor.h"              
#include "PWM.h"  

void MotorABCD_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_5|GPIO_Pin_8|GPIO_Pin_9|GPIO_Pin_10|GPIO_Pin_11;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitStrusture);
	
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_5;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOC,&GPIO_InitStrusture);
	
	PWM_ABCD_Init();
	
}

void MotorA_SetSpeed(int16_t Speed)
{
	if(Speed>=0)
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_0);
		GPIO_ResetBits(GPIOB,GPIO_Pin_1);
		PWM_SetCompare1(Speed);
	}
	else 
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_1);
		GPIO_ResetBits(GPIOB,GPIO_Pin_0);
		PWM_SetCompare1(-Speed);
	}
	
}

void MotorB_SetSpeed(int16_t Speed)
{
	if(Speed>=0)
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_9);
		GPIO_ResetBits(GPIOB,GPIO_Pin_8);
		PWM_SetCompare2(Speed);
	}
	else 
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_8);
		GPIO_ResetBits(GPIOB,GPIO_Pin_9);
		PWM_SetCompare2(-Speed);
	}
	
}

void MotorC_SetSpeed(int16_t Speed)
{
	if(Speed>=0)
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_11);
		GPIO_ResetBits(GPIOB,GPIO_Pin_10);
		PWM_SetCompare3(Speed);
	}
	else 
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_10);
		GPIO_ResetBits(GPIOB,GPIO_Pin_11);
		PWM_SetCompare3(-Speed);
	}
	
}

void MotorD_SetSpeed(int16_t Speed)
{
	if(Speed>=0)
	{
		GPIO_SetBits(GPIOC,GPIO_Pin_5);
		GPIO_ResetBits(GPIOB,GPIO_Pin_5);
		PWM_SetCompare4(Speed);
	}
	else 
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_5);
		GPIO_ResetBits(GPIOC,GPIO_Pin_5);
		PWM_SetCompare4(-Speed);
	}
	
}

void Set_PWM(int16_t PWM1,int16_t PWM2,int16_t PWM3,int16_t PWM4)
{
	MotorA_SetSpeed(PWM1);
	MotorB_SetSpeed(PWM2);
	MotorC_SetSpeed(PWM3);
	MotorD_SetSpeed(PWM4);
	
	
}

