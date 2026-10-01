#include "stm32f10x.h"
#include "WS2812.h"
#include "string.h"
#include "Delay.h"

// 存储WS2812的LED数据（包含所有LED的RGB信号和复位信号）
uint16_t LED_Data[WS2812_LED_DATA_LEN] = {0};
// WS2812 LED控制结构体（G/R/B颜色值和相位参数）
WS2812_LED_CTRL ws2812_led = {0xff,0,0,0};

/**
 * @brief  WS2812初始化函数
 * @note   配置GPIO、定时器、DMA用于驱动WS2812灯带
 */
void ws2812_Init(void)
{
#ifdef _USE_TIMER_CLK  // 使用定时器+DMA方式驱动WS2812
	
	GPIO_InitTypeDef  GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef		WS2812_TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  			WS2812_TIM_OCInitStructure;
	DMA_InitTypeDef 			WS2812_DMA_InitStructure;

	// 使能GPIO时钟和AFIO时钟
	RCC_APB2PeriphClockCmd(WS2812_GPIO_CLK, ENABLE);	 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);	 
	
	// 使能定时器和DMA时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3,ENABLE);
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1,ENABLE);

	// 配置WS2812数据引脚为复用推挽输出（用于定时器PWM）
	GPIO_InitStructure.GPIO_Pin = WS2812_GPIO_PIN;				 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; 		 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		
	GPIO_Init(WS2812_GPIO_PORT, &GPIO_InitStructure);					

	// 配置定时器时基（800KHz频率，用于WS2812的时序要求）
	WS2812_TIM_TimeBaseStructure.TIM_Prescaler		=	0;  // 不分频
	WS2812_TIM_TimeBaseStructure.TIM_CounterMode	=	TIM_CounterMode_Up;	// 向上计数
	WS2812_TIM_TimeBaseStructure.TIM_Period			=	(SystemCoreClock/(WS2812_Freq*1000))-1;	// 计算自动重装载值（800KHz）
	WS2812_TIM_TimeBaseStructure.TIM_ClockDivision	=	TIM_CKD_DIV1;
	TIM_TimeBaseInit(WS2812_TIM,&WS2812_TIM_TimeBaseStructure);
	
	// 配置定时器PWM模式（用于生成WS2812的0/1码信号）
	WS2812_TIM_OCInitStructure.TIM_OCMode			=	TIM_OCMode_PWM1;  // PWM模式1
	WS2812_TIM_OCInitStructure.TIM_OutputState		=	TIM_OutputState_Enable;	// 使能输出
	WS2812_TIM_OCInitStructure.TIM_Pulse			= 	0;	// 初始脉冲宽度
	WS2812_TIM_OCInitStructure.TIM_OCPolarity		=	TIM_OCPolarity_High;	// 输出极性高
	
	TIM_OC3Init(WS2812_TIM,&WS2812_TIM_OCInitStructure);	// 配置通道3
	TIM_OC3PreloadConfig(WS2812_TIM,TIM_OCPreload_Enable);	// 使能预装载
	
	TIM_Cmd(WS2812_TIM,DISABLE);	// 先关闭定时器
	
	// 使能定时器DMA（用于自动发送PWM数据）
	TIM_DMACmd(WS2812_TIM,TIM_DMA_CC3,ENABLE);		// 定时器3通道3 DMA使能
	
	// 配置DMA（用于将LED数据传输到定时器CCR寄存器）
	WS2812_DMA_InitStructure.DMA_PeripheralBaseAddr	=	(uint32_t)&(TIM3->CCR3);		// 外设地址（定时器3 CCR3）
	WS2812_DMA_InitStructure.DMA_MemoryBaseAddr		=	(uint32_t)LED_Data;	// 内存地址（LED数据数组）
	WS2812_DMA_InitStructure.DMA_DIR				=	DMA_DIR_PeripheralDST;			// 方向：内存到外设
	WS2812_DMA_InitStructure.DMA_BufferSize			=	WS2812_LED_DATA_LEN;			// 数据长度
	WS2812_DMA_InitStructure.DMA_PeripheralInc		=	DMA_PeripheralInc_Disable;		// 外设地址不递增
	WS2812_DMA_InitStructure.DMA_MemoryInc			=	DMA_MemoryInc_Enable;			// 内存地址递增
	WS2812_DMA_InitStructure.DMA_PeripheralDataSize	=	DMA_PeripheralDataSize_HalfWord;// 外设数据宽度16位
	WS2812_DMA_InitStructure.DMA_MemoryDataSize		=	DMA_MemoryDataSize_HalfWord;	// 内存数据宽度16位
	WS2812_DMA_InitStructure.DMA_Mode				=	DMA_Mode_Normal;				// 正常模式（非循环）
	WS2812_DMA_InitStructure.DMA_Priority			=	DMA_Priority_Medium;			// 中等优先级
	WS2812_DMA_InitStructure.DMA_M2M				=	DMA_M2M_Disable;				// 非内存到内存模式
	
	DMA_Init(DMA1_Channel2,&WS2812_DMA_InitStructure);	// 初始化DMA通道2
	DMA_Cmd(DMA1_Channel2,DISABLE);						// 先关闭DMA
#else  // 不使用定时器，直接用GPIO模拟时序（此处未实现完整逻辑）
	GPIO_InitTypeDef  GPIO_InitStructure;

  RCC_APB2PeriphClockCmd(WS2812_GPIO_CLK, ENABLE);	  // 使能GPIO时钟

	// 配置为推挽输出
	GPIO_InitStructure.GPIO_Pin = WS2812_GPIO_PIN;				 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		 
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		
	GPIO_Init(WS2812_GPIO_PORT, &GPIO_InitStructure);		
	WS2812_LED_HIGH	();  // 初始高电平
#endif
	// 初始化所有LED为0（灭）
	for (uint8_t idx = 0; idx < LED_NUMS ;idx ++)
		LED_Data[idx] = WS2812_LED_DATA_0;
	Change_WS2812LED_Status();	// 更新LED状态
}

/**
 * @brief  填充WS2812单个LED的RGB数据
 * @param  G: 绿色分量（0-255）
 * @param  R: 红色分量（0-255）
 * @param  B: 蓝色分量（0-255）
 * @param  Pos: LED位置（0到LED_NUMS-1）
 */
void Fill_WS2812_LED_Data(uint8_t G,uint8_t R,uint8_t B,uint16_t Pos)
{
	uint32_t indexx=(Pos*WS2812_LED_BIT);  // 计算该LED在数据数组中的起始索引
 
	if (Pos > LED_NUMS)  // 位置超出范围时，所有LED灭
	{
		for (uint8_t idx = 0; idx < LED_NUMS ;idx ++)
			LED_Data[idx] = WS2812_LED_DATA_0;
	}
	else  // 正常填充RGB数据（按WS2812的GRB格式）
	{
		for (uint8_t i = 0;i < 8;i++)
		{
			// 绿色分量（高位在前）
			LED_Data[indexx+i]      = (G << i) & (0x80)?WS2812_LED_DATA_1:WS2812_LED_DATA_0;
			// 红色分量（高位在前）
			LED_Data[indexx+i + 8]  = (R << i) & (0x80)?WS2812_LED_DATA_1:WS2812_LED_DATA_0;
			// 蓝色分量（高位在前）
			LED_Data[indexx+i + 16] = (B << i) & (0x80)?WS2812_LED_DATA_1:WS2812_LED_DATA_0;
		}
	}
}

/**
 * @brief  关闭所有WS2812 LED
 */
void WS2812_LED_PowerOff(void)
{
	for (uint8_t idx = 0; idx < LED_NUMS ;idx ++)
		LED_Data[idx] = WS2812_LED_DATA_0;  // 所有数据位设为0
	Change_WS2812LED_Status();	// 更新状态
}

/**
 * @brief  WS2812流水灯效果（RGB颜色渐变循环）
 */
void WS2812_LED_Circle(void)
{
	uint16_t idx,i;
	
	// 数据移位（实现流水效果）
	for (idx = LED_NUMS - 1; idx > 0; idx --)
	{
		for (i = 0; i < WS2812_LED_BIT; i ++)
			LED_Data[idx* WS2812_LED_BIT  + i] = LED_Data[(idx-1) * WS2812_LED_BIT + i];
	}
	// 颜色渐变逻辑（绿→红→蓝→绿循环）
	if (ws2812_led.phase < WS2812_LED_LEVEL)  // 绿色减弱，红色增强
	{
		ws2812_led.G -= (0xff / WS2812_LED_LEVEL);
		ws2812_led.R += (0xff / WS2812_LED_LEVEL);
		ws2812_led.phase ++;
		if (ws2812_led.phase == WS2812_LED_LEVEL)
		{
			ws2812_led.G = 0;
			ws2812_led.R = 0xff;
		}
	}
	else if (ws2812_led.phase < WS2812_LED_LEVEL * 2)  // 红色减弱，蓝色增强
	{
		ws2812_led.R -= (0xff / WS2812_LED_LEVEL);
		ws2812_led.B += (0xff / WS2812_LED_LEVEL);
		ws2812_led.phase ++;
		if (ws2812_led.phase == WS2812_LED_LEVEL * 2)
		{
			ws2812_led.R = 0;
			ws2812_led.B = 0xff;
		}
	}
	else if (ws2812_led.phase < WS2812_LED_LEVEL * 3)  // 蓝色减弱，绿色增强
	{
		ws2812_led.B -= (0xff / WS2812_LED_LEVEL);
		ws2812_led.G += (0xff / WS2812_LED_LEVEL);
		ws2812_led.phase ++;
		if (ws2812_led.phase == WS2812_LED_LEVEL * 3)
		{
			ws2812_led.B = 0;
			ws2812_led.G = 0xff;
			ws2812_led.phase = 0;  // 重置相位，循环
		}
	}
	// 填充第0个LED的颜色数据（作为流水灯的源头）
	Fill_WS2812_LED_Data(ws2812_led.G,ws2812_led.R,ws2812_led.B,0);
	Change_WS2812LED_Status();  // 更新所有LED状态
}

/**
 * @brief  WS2812复位信号（满足灯带的复位时序要求）
 */
void WS2812_LED_RST(void)
{
#ifdef _USE_TIMER_CLK	
	TIM_Cmd(WS2812_TIM,DISABLE);  // 关闭定时器
#endif	
	WS2812_LED_LOW();  // 拉低数据引脚
	Delay_us(WS2812_RESET_TIME);  // 保持复位时间（至少280us）
}

/**
 * @brief  更新WS2812 LED状态（发送数据到灯带）
 */
void Change_WS2812LED_Status(void)
{
#ifdef _USE_TIMER_CLK		// 定时器+DMA方式发送数据	
	DMA_SetCurrDataCounter(DMA1_Channel2,(WS2812_LED_DATA_LEN));  // 设置DMA传输长度
	DMA_Cmd(DMA1_Channel2,ENABLE);  // 使能DMA
	TIM_Cmd(WS2812_TIM,ENABLE);  // 启动定时器
	while(DMA_GetFlagStatus(DMA1_FLAG_TC2) != SET);  // 等待DMA传输完成
	DMA_Cmd(DMA1_Channel2,DISABLE);  // 关闭DMA
	DMA_ClearFlag(DMA1_FLAG_TC2);  // 清除完成标志
	TIM_Cmd(WS2812_TIM,DISABLE);  // 关闭定时器
#else  // GPIO模拟方式（未实现完整逻辑）
	int i;
	for(i = 0;i < LED_NUMS;i++)
	{
		Write_One_RGBLED(LED_Data[3 * i],LED_Data[3 * i + 1],LED_Data[3 * i + 2]);
	}
	WS2812_LED_RST();  // 发送复位信号
#endif	
}
