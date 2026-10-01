#include "stm32f10x.h"                  // Device header
#include "Buzzer.h"  

void Buzzer_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStrusture;	
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitStrusture);
	
	//上电禁止鸣叫--低电平
	GPIO_ResetBits(GPIOB,GPIO_Pin_14);
	
}

void Buzzer_ON(void)
{
	GPIO_SetBits(GPIOB,GPIO_Pin_14);
}

void Buzzer_OFF(void)
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_14);
}

void Buzzer_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_14)==0)
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_14);
	}
	else 
	{
		GPIO_ResetBits(GPIOB,GPIO_Pin_14);
	}
}