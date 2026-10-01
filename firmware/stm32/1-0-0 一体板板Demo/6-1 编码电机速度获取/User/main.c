#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 
#include "Key.h" 
#include "Buzzer.h"  
#include "OLED.h" 
#include "My_USART2.h"   
#include "Encoder.h"   

uint8_t KeyNum = 0;
extern uint8_t My_USART2_Data; 
int32_t MotorA_Speed,MotorB_Speed,MotorC_Speed,MotorD_Speed;

int main(void)
{
	/*模块初始化*/
	LED_Init();
	Key_Init();
	Buzzer_Init();
	OLED_Init();		//OLED初始化
	My_USART2_Init();
	EncoderA_Init();
	EncoderB_Init();
	EncoderC_Init();
	EncoderD_Init();
	
	/*OLED显示*/
	OLED_ShowString(1,1,"A:");
	OLED_ShowString(1,8,"B:");
	OLED_ShowString(2,1,"C:");
	OLED_ShowString(2,8,"D:");

	
	while (1)
	{
		MotorA_Speed = EncoderA_Get();
		MotorB_Speed = EncoderB_Get();
		MotorC_Speed = EncoderC_Get();
		MotorD_Speed = EncoderD_Get();
		
		OLED_ShowSignedNum(1,3,MotorA_Speed,2);
		OLED_ShowSignedNum(1,10,MotorB_Speed,2);
		OLED_ShowSignedNum(2,3,MotorC_Speed,2);
		OLED_ShowSignedNum(2,10,MotorD_Speed,2);
	}
}
