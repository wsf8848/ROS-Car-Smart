#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 
#include "Key.h" 
#include "Buzzer.h"  
#include "OLED.h" 
#include "My_USART1.h"   
#include "Encoder.h"   
#include "PWM.h"    
#include "Motor.h"  
#include "APP.h"

uint8_t KeyNum = 0;
extern uint8_t My_USART2_Data; 
int32_t MotorA_Speed,MotorB_Speed,MotorC_Speed,MotorD_Speed;

//上位机调试专用
unsigned char Send_Count; //串口需要发送的数据个数
unsigned char j;          //计数变量
static float a,b,c; //测试变量

int main(void)
{
	/*模块初始化*/
	LED_Init();
	Key_Init();
	Buzzer_Init();
	OLED_Init();		//OLED初始化
	My_USART1_Init(); 
	EncoderA_Init();
	EncoderB_Init();
	EncoderC_Init();
	EncoderD_Init();
	MotorABCD_Init();

	/*OLED显示*/
	
	while (1)
	{
	   a+=1;
	   if(a>100)  a=0; 
		
		b+=3;
	   if(b>100)  b=0; 
		
	    c+=5;
	   if(c>100)  c=0; 

		
		MotorA_Speed = -EncoderA_Get();
		MotorB_Speed = EncoderB_Get();
		MotorC_Speed = EncoderC_Get();
		MotorD_Speed = -EncoderD_Get();
				
		Set_PWM(1800,1800,1800,1800);  
		
		 //上位机发送
		DataScope_Get_Channel_Data(a, 1 );  //当前数据
		DataScope_Get_Channel_Data(b, 2 ); // 目标数据
		DataScope_Get_Channel_Data(c, 3 );  //当前数据
		DataScope_Get_Channel_Data(20, 4 ); // 目标数据

		//上位机调试专用
		Send_Count = DataScope_Data_Generate(10);
		for( j = 0 ; j < Send_Count; j++)
		{
		  My_USART1_SendByte( DataScope_OutPut_Buffer[j]); //发送到上位机
		}
		Delay_ms(50); //20HZ--一定要
		
		
	}
}
