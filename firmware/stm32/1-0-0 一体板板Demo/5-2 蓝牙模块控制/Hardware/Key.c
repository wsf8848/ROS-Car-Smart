#include "stm32f10x.h"                  // Device header
#include "Key.h"                 
#include "Delay.h" 

void Key_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOC,&GPIO_InitStrusture);
	
}

uint8_t Key_Num(void)
{
	uint8_t KeyNum = 0;
	
	//PC0
	if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_0)==0)
	{
		Delay_ms(20);
		while(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_0)==0);
		Delay_ms(20);
		
		KeyNum = 1;
	}
	//PC1
	if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_1)==0)
	{
		Delay_ms(20);
		while(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_1)==0);
		Delay_ms(20);
		
		KeyNum = 2;
	}
	//PC2
	if(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_2)==0)
	{
		Delay_ms(20);
		while(GPIO_ReadInputDataBit(GPIOC,GPIO_Pin_2)==0);
		Delay_ms(20);
		
		KeyNum = 3;
	}
	
	return KeyNum;
}
