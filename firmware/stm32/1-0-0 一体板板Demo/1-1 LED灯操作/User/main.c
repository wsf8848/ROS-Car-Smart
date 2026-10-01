#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 

int main(void)
{
	/*模块初始化*/
	LED_Init();
	
	while (1)
	{
	  LED1_Turn();
	  Delay_ms(500);
	  LED2_Turn();
	  Delay_ms(500);
	  LED3_Turn();
	  Delay_ms(500);
		
	}
}
