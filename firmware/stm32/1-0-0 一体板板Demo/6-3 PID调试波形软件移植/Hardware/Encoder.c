#include "stm32f10x.h"                  // Device header
#include "Encoder.h"          

void EncoderA_Init(void)
{
	//开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	//GPIO初始化
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitStrusture);
	
	//定时器时基配置
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStrusture;
	TIM_TimeBaseStrusture.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStrusture.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStrusture.TIM_Period = 65536-1;
	TIM_TimeBaseStrusture.TIM_Prescaler = 1-1;
	TIM_TimeBaseStrusture.TIM_RepetitionCounter = 0;
	
	TIM_TimeBaseInit(TIM3,&TIM_TimeBaseStrusture);
	
	//输入捕获模式初始化
	TIM_ICInitTypeDef TIM_ICInitStrusture;
	TIM_ICStructInit(&TIM_ICInitStrusture); //赋初始值
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_1; //PA0
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM3,&TIM_ICInitStrusture);
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_2; //PA1
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM3,&TIM_ICInitStrusture);
	
	//编码器接口配置
	TIM_EncoderInterfaceConfig(TIM3,TIM_EncoderMode_TI1,TIM_ICPolarity_Rising,TIM_ICPolarity_Rising);
	
	//使能定时器
	TIM_Cmd(TIM3,ENABLE);
	
}

int16_t EncoderA_Get(void)
{
	int16_t Temp;
	Temp = TIM_GetCounter(TIM3);
	TIM_SetCounter(TIM3,0);

	return Temp;	
}

void EncoderB_Init(void)
{
	//开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB,ENABLE);
	
	//功能代码
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE); //AFIO开启
	GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2,ENABLE); //TIM2引脚重映射
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE); //禁用JTAG调试口
	
	
	//GPIO初始化
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitStrusture);
	
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitStrusture);
	
	//定时器时基配置
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStrusture;
	TIM_TimeBaseStrusture.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStrusture.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStrusture.TIM_Period = 65536-1;
	TIM_TimeBaseStrusture.TIM_Prescaler = 1-1;
	TIM_TimeBaseStrusture.TIM_RepetitionCounter = 0;
	
	TIM_TimeBaseInit(TIM2,&TIM_TimeBaseStrusture);
	
	//输入捕获模式初始化
	TIM_ICInitTypeDef TIM_ICInitStrusture;
	TIM_ICStructInit(&TIM_ICInitStrusture); //赋初始值
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_1; //PA0
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM2,&TIM_ICInitStrusture);
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_2; //PA1
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM2,&TIM_ICInitStrusture);
	
	//编码器接口配置
	TIM_EncoderInterfaceConfig(TIM2,TIM_EncoderMode_TI1,TIM_ICPolarity_Rising,TIM_ICPolarity_Rising);
	
	//使能定时器
	TIM_Cmd(TIM2,ENABLE);
	
}

int16_t EncoderB_Get(void)
{
	int16_t Temp;
	Temp = TIM_GetCounter(TIM2);
	TIM_SetCounter(TIM2,0);

	return Temp;	
}

void EncoderC_Init(void)
{
	//开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	
	//GPIO初始化
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOB,&GPIO_InitStrusture);
	
	//定时器时基配置
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStrusture;
	TIM_TimeBaseStrusture.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStrusture.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStrusture.TIM_Period = 65536-1;
	TIM_TimeBaseStrusture.TIM_Prescaler = 1-1;
	TIM_TimeBaseStrusture.TIM_RepetitionCounter = 0;
	
	TIM_TimeBaseInit(TIM4,&TIM_TimeBaseStrusture);
	
	//输入捕获模式初始化
	TIM_ICInitTypeDef TIM_ICInitStrusture;
	TIM_ICStructInit(&TIM_ICInitStrusture); //赋初始值
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_1; //PA0
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM4,&TIM_ICInitStrusture);
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_2; //PA1
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM4,&TIM_ICInitStrusture);
	
	//编码器接口配置
	TIM_EncoderInterfaceConfig(TIM4,TIM_EncoderMode_TI1,TIM_ICPolarity_Rising,TIM_ICPolarity_Rising);
	
	//使能定时器
	TIM_Cmd(TIM4,ENABLE);
	
}

int16_t EncoderC_Get(void)
{
	int16_t Temp;
	Temp = TIM_GetCounter(TIM4);
	TIM_SetCounter(TIM4,0);

	return Temp;	
}

void EncoderD_Init(void)
{
	//开启时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|RCC_APB2Periph_AFIO,ENABLE);
	
	//GPIO初始化
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOA,&GPIO_InitStrusture);
	
	//定时器时基配置
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStrusture;
	TIM_TimeBaseStrusture.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStrusture.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseStrusture.TIM_Period = 65536-1;
	TIM_TimeBaseStrusture.TIM_Prescaler = 1-1;
	TIM_TimeBaseStrusture.TIM_RepetitionCounter = 0;
	
	TIM_TimeBaseInit(TIM5,&TIM_TimeBaseStrusture);
	
	//输入捕获模式初始化
	TIM_ICInitTypeDef TIM_ICInitStrusture;
	TIM_ICStructInit(&TIM_ICInitStrusture); //赋初始值
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_1; //PA0
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM5,&TIM_ICInitStrusture);
	
	TIM_ICInitStrusture.TIM_Channel = TIM_Channel_2; //PA1
	TIM_ICInitStrusture.TIM_ICFilter = 0xF;
	TIM_ICInit(TIM5,&TIM_ICInitStrusture);
	
	//编码器接口配置
	TIM_EncoderInterfaceConfig(TIM5,TIM_EncoderMode_TI1,TIM_ICPolarity_Rising,TIM_ICPolarity_Rising);
	
	//使能定时器
	TIM_Cmd(TIM5,ENABLE);
	
}

int16_t EncoderD_Get(void)
{
	int16_t Temp;
	Temp = TIM_GetCounter(TIM5);
	TIM_SetCounter(TIM5,0);

	return Temp;	
}



