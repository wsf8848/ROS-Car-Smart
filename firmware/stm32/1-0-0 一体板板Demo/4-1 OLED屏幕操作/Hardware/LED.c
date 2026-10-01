#include "stm32f10x.h"                  // Device header
#include "LED.h"  

void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB|RCC_APB2Periph_GPIOC,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOC,&GPIO_InitStrusture);
	
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_12|GPIO_Pin_13;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitStrusture);
	
	//¸ßµçÆ½Ï¨Ãð
	GPIO_SetBits(GPIOC,GPIO_Pin_3);
	GPIO_SetBits(GPIOB,GPIO_Pin_12|GPIO_Pin_13);
	
}

void LED1_ON(void)
{
	GPIO_ResetBits(GPIOC,GPIO_Pin_3);
}

void LED1_OFF(void)
{
	GPIO_SetBits(GPIOC,GPIO_Pin_3);
}

void LED1_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOC,GPIO_Pin_3)==0)
	{
		GPIO_SetBits(GPIOC,GPIO_Pin_3);
	}
	else 
	{
		GPIO_ResetBits(GPIOC,GPIO_Pin_3);
	}
}

void LED2_ON(void)
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_12);
}

void LED2_OFF(void)
{
	GPIO_SetBits(GPIOB,GPIO_Pin_12);
}

void LED2_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_12)==0)
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_12);
	}
	else 
	{
		GPIO_ResetBits(GPIOB,GPIO_Pin_12);
	}
}

void LED3_ON(void)
{
	GPIO_ResetBits(GPIOB,GPIO_Pin_13);
}

void LED3_OFF(void)
{
	GPIO_SetBits(GPIOB,GPIO_Pin_13);
}

void LED3_Turn(void)
{
	if(GPIO_ReadOutputDataBit(GPIOB,GPIO_Pin_13)==0)
	{
		GPIO_SetBits(GPIOB,GPIO_Pin_13);
	}
	else 
	{
		GPIO_ResetBits(GPIOB,GPIO_Pin_13);
	}
}






