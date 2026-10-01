#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 
#include "Key.h" 
#include "Buzzer.h"  
#include "OLED.h" 
#include "My_USART2.h"   

uint8_t KeyNum = 0;
extern uint8_t My_USART2_Data; 

int main(void)
{
	/*模块初始化*/
	LED_Init();
	Key_Init();
	Buzzer_Init();
	OLED_Init();		//OLED初始化
	My_USART2_Init();
	
	/*OLED显示*/
	OLED_ShowString(1,1,"R:");

	
	while (1)
	{
		My_USART2_Printf("hello bule");
		OLED_ShowHexNum(1,4,My_USART2_Data,2);
		
		Delay_ms(500);
	}
}
