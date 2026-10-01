#include "stm32f10x.h"                  // Device header
#include "Timer.h"                

/****************************************************************************
* 函数名：TIM6_Init
* 功  能：定时器6初始化，产生10ms中断
* 参  数：无
****************************************************************************/
void Timer_Init(void)
{
    // 1. 开启TIM6时钟（APB1总线外设）
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
    
    // 2. 配置定时器基础结构体
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    
    // 计数器计数方式：向上计数
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    // 分频系数：7199，时钟=72MHz/(7199+1)=10kHz
    TIM_TimeBaseInitStructure.TIM_Prescaler = 7199;
    // 自动重装载值：100-1，10kHz计数100次=10ms
    TIM_TimeBaseInitStructure.TIM_Period = 99;
    // 初始化TIM6
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseInitStructure);
    
    // 3. 使能TIM6更新中断
    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);
    
    // 4. 配置NVIC中断优先级
    NVIC_InitTypeDef NVIC_InitStructure;
    // 中断通道：TIM6全局中断
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_IRQn;
    // 抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    // 子优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    // 使能中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    // 初始化NVIC
    NVIC_Init(&NVIC_InitStructure);
    
    // 5. 开启定时器6
    TIM_Cmd(TIM6, ENABLE);
}

///****************************************************************************
//* 函数名：TIM6_IRQHandler
//* 功  能：定时器6中断服务函数（10ms进入一次）
//****************************************************************************/
//void TIM6_IRQHandler(void)
//{
//    // 判断是否发生更新中断
//    if(TIM_GetITStatus(TIM6, TIM_IT_Update) == SET)
//    {
//        // ======================
//        // 在这里写10ms执行一次的代码
//        // ======================
//        
//        // 清除中断标志位
//        TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
//    }
//}
