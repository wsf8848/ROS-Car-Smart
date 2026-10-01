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
#include "Timer.h"
#include "Pid.h"

uint8_t KeyNum = 0;
extern uint8_t My_USART2_Data;
int32_t MotorA_Speed, MotorB_Speed, MotorC_Speed, MotorD_Speed;  //电机速度变量
float PWM1;


//上位机调试专用
unsigned char Send_Count; //串口需要发送的数据个数
unsigned char j;          //计数变量

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
  Timer_Init(); //10ms

  /*OLED显示*/
  OLED_ShowString(1, 1, "Speed:");

  while (1)
  {


    //OLED显示数据
    OLED_ShowSignedNum(1, 7, MotorA_Speed, 2);


  }
}

/****************************************************************************
* 函数名：TIM6_IRQHandler
* 功  能：定时器6中断服务函数（10ms进入一次）
****************************************************************************/
void TIM6_IRQHandler(void)
{
  static uint8_t Count = 0;

  // 判断是否发生更新中断
  if(TIM_GetITStatus(TIM6, TIM_IT_Update) == SET)
  {
    //1.获取电机速度
    MotorA_Speed = -EncoderA_Get();
    //2.PID运算
	PWM1 = Left_FeedbackControl(MotorA_Speed,20);
    //3.加载到电机上
	MotorA_SetSpeed(PWM1);


    //定时发送数据
    Count++;

    if(Count >= 5) //5*10ms==50ms
    {
      //上位机发送
      DataScope_Get_Channel_Data(MotorA_Speed, 1); // 通道1发送电机实际运行速度
      DataScope_Get_Channel_Data(20, 2); // 通道2目标值

      //上位机调试专用
      Send_Count = DataScope_Data_Generate(10);

      for( j = 0 ; j < Send_Count; j++)
      {
        My_USART1_SendByte( DataScope_OutPut_Buffer[j]); //发送到上位机
      }

      Count = 0;
    }

    // 清除中断标志位
    TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
  }
}