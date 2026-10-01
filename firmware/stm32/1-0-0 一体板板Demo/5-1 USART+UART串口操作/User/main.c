#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 
#include "Key.h" 
#include "Buzzer.h"  
#include "OLED.h"
#include "My_USART1.h"   
#include "My_USART2.h"  
#include "My_UART4.h"  
#include "My_UART5.h"   

uint8_t KeyNum = 0;
extern uint8_t My_USART1_Data; 
extern uint8_t My_USART2_Data; 
extern uint8_t My_UART4_Data; 
extern uint8_t My_UART5_Data; 

int main(void)
{
	/*模块初始化*/
	LED_Init();
	Key_Init();
	Buzzer_Init();
	OLED_Init();		//OLED初始化
	My_USART1_Init();
	My_USART2_Init();
	My_UART4_Init();
	My_UART5_Init();
	
	/*OLED显示*/
	OLED_ShowString(1,1,"R1:");
	OLED_ShowString(2,1,"R2:");
	OLED_ShowString(3,1,"R4:");
	OLED_ShowString(4,1,"R5:");
	
	while (1)
	{
		My_USART1_SendByte(0X01);
		My_USART2_SendByte(0X02);
		My_UART4_SendByte(0X04);
		My_UART5_SendByte(0X05);
		
		
		
		OLED_ShowHexNum(1,4,My_USART1_Data,2);
		OLED_ShowHexNum(2,4,My_USART2_Data,2);
		OLED_ShowHexNum(3,4,My_UART4_Data,2);
		OLED_ShowHexNum(4,4,My_UART5_Data,2);
		
		
		Delay_ms(500);
	}
}
