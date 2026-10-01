#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 
#include "Key.h" 

uint8_t KeyNum = 0;

int main(void)
{
	/*模块初始化*/
	LED_Init();
	Key_Init();
	
	while (1)
	{
	  KeyNum = Key_Num();
	  
	  if(KeyNum==1)
	  {
		  LED1_Turn();
	  }
	  else if(KeyNum==2)
	  {
		  LED2_Turn();
	  }
	  else if(KeyNum==3)
	  {
		  LED3_Turn();
	  }
	}
}
