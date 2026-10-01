#include "stm32f10x.h"                  // Device header
#include "Buzzer.h"                 
#include "Delay.h"

void Buzzer_Init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
 	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	 //使能A端口时钟
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;	 
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 //推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;//速度50MHz
 	GPIO_Init(GPIOB, &GPIO_InitStructure);	  //初始化GPIOA
	
	GPIO_ResetBits(GPIOB,GPIO_Pin_12);//0
}

/**
  * 函    数：蜂鸣器开启
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_ON(void)
{
	GPIO_SetBits(GPIOB, GPIO_Pin_12);		//1
}

/**
  * 函    数：蜂鸣器关闭
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_OFF(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);		//0
}

/**
  * 函    数：蜂鸣器状态翻转
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Turn(void)
{
	if (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_12) == 0)		//获取输出寄存器的状态，如果当前引脚输出低电平
	{
		GPIO_SetBits(GPIOB, GPIO_Pin_12);						//则设置PB12引脚为高电平
	}
	else														//否则，即当前引脚输出高电平
	{
		GPIO_ResetBits(GPIOB, GPIO_Pin_12);						//则设置PB12引脚为低电平
	}
}

/**************************************************************************
函数功能：蜂鸣器闪烁
入口参数：闪烁频率 
Led_Flash(100); //呼吸模式
Led_Flash(10); //爆闪工作模式
**************************************************************************/

void Buzzer_Flash(u16 time)
{
	static int temp;
	if(0==time) Buzzer_ON();
	else if(++temp==time)	Buzzer_Turn(),temp=0;
	
}
void Buzzer_start(void)
{
  Buzzer_ON();
  Delay_ms(100);
  Buzzer_OFF();
  Delay_ms(100);
  Buzzer_ON();
  Delay_ms(100);
  Buzzer_OFF();	
}