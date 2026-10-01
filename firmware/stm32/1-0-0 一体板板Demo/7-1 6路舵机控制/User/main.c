#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 
#include "Key.h" 
#include "Buzzer.h"  
#include "OLED.h" 
#include "My_USART2.h"   
#include "Encoder.h"   
#include "Servo.h"   

uint8_t KeyNum = 0;
float Angle = 0;

int main(void)
{
	/*模块初始化*/
	Servo12_Init(); //放置位置
	Servo3456_Init();
	LED_Init();
	Key_Init();
	Buzzer_Init();
	OLED_Init();		//OLED初始化
	/*OLED显示*/
	OLED_ShowString(1,1,"Angle:");

	
	while (1)
	{
		for(Angle = 0;Angle<=180;Angle++) //0-180
		{
			Servo_SetAngle1(Angle);
			Servo_SetAngle2(Angle);
			Servo_SetAngle3(Angle);
			Servo_SetAngle4(Angle);
			Servo_SetAngle5(Angle);
			Servo_SetAngle6(Angle);
			Delay_ms(20);
			OLED_ShowNum(1,7,Angle,3);
		}
		for(Angle = 180;Angle>=0;Angle--) //0-180
		{
			Servo_SetAngle1(Angle);
			Servo_SetAngle2(Angle);
			Servo_SetAngle3(Angle);
			Servo_SetAngle4(Angle);
			Servo_SetAngle5(Angle);
			Servo_SetAngle6(Angle);
			Delay_ms(20);
			OLED_ShowNum(1,7,Angle,3);
		}
		
		
	}
}
