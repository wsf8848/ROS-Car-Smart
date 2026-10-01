#include "stm32f10x.h"                  // Device header
#include "PWM.h"    

void PWM_ABCD_Init(void)
{
	//开启时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);
	
	//GPIO配置
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7|GPIO_Pin_8|GPIO_Pin_9;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOC,&GPIO_InitStrusture);
	
	//TIM配置
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStrusture;
	TIM_TimeBaseStrusture.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStrusture.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStrusture.TIM_Period = 3600-1;
	TIM_TimeBaseStrusture.TIM_Prescaler = 0;
	
	TIM_TimeBaseInit(TIM8,&TIM_TimeBaseStrusture);
	
	//PWM模式配置
	TIM_OCInitTypeDef TIM_OCInitStrusture;
	TIM_OCInitStrusture.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStrusture.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStrusture.TIM_Pulse = 0;
	TIM_OCInitStrusture.TIM_OCPolarity = TIM_OCPolarity_High;
	
	//PC6  CH1
	TIM_OC1Init(TIM8,&TIM_OCInitStrusture);
	TIM_OC1PreloadConfig(TIM8,TIM_OCPreload_Enable);
	
	//PC7  CH2
	TIM_OC2Init(TIM8,&TIM_OCInitStrusture);
	TIM_OC2PreloadConfig(TIM8,TIM_OCPreload_Enable);
	
	//PC8  CH3
	TIM_OC3Init(TIM8,&TIM_OCInitStrusture);
	TIM_OC3PreloadConfig(TIM8,TIM_OCPreload_Enable);
	
	
	//PC9  CH4
	TIM_OC4Init(TIM8,&TIM_OCInitStrusture);
	TIM_OC4PreloadConfig(TIM8,TIM_OCPreload_Enable);
	
	
	//使能TIM8预载值
	TIM_ARRPreloadConfig(TIM8,ENABLE);
	
	//启动定时器
	TIM_Cmd(TIM8,ENABLE);
	
	//使能MOE位
	TIM_CtrlPWMOutputs(TIM8,ENABLE);

}


void PWM_SetCompare1(uint16_t Compare)
{
	TIM_SetCompare1(TIM8,Compare);
}

void PWM_SetCompare2(uint16_t Compare)
{
	TIM_SetCompare2(TIM8,Compare);
}

void PWM_SetCompare3(uint16_t Compare)
{
	TIM_SetCompare3(TIM8,Compare);
}

void PWM_SetCompare4(uint16_t Compare)
{
	TIM_SetCompare4(TIM8,Compare);
}











