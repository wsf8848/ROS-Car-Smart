#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Timer.h"                      //提供定时器驱动接口声明

/**
  * 函    数：TIM2基础定时器初始化
  * 参    数：无
  * 返 回 值：无
  */
void Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;                                      //定义TIM2时基初始化结构体
    NVIC_InitTypeDef NVIC_InitStructure;                                                    //定义TIM2中断初始化结构体

    /*开启定时器时钟*/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);                                     //开启APB1总线上的TIM2外设时钟
    TIM_InternalClockConfig(TIM2);                                                           //选择TIM2内部时钟作为计数时钟源

    /*配置10ms更新周期*/
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;                              //配置定时器数字滤波采样时钟不分频
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;                          //配置TIM2向上计数模式
    TIM_TimeBaseInitStructure.TIM_Period = 1000U - 1U;                                      //设置自动重装载值使计数1000次更新
    TIM_TimeBaseInitStructure.TIM_Prescaler = 720U - 1U;                                    //将72MHz定时器时钟预分频为100kHz
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0U;                                   //通用定时器不使用重复计数功能
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);                                     //将10ms时基配置写入TIM2

    /*配置更新中断*/
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);                                                    //清除初始化过程中产生的更新标志
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);                                               //开启TIM2更新中断请求
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);                                          //使用两位抢占和两位响应优先级分组
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;                                          //选择TIM2全局中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                                          //使能TIM2中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0U;                               //设置TIM2抢占优先级为0
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0U;                                      //设置TIM2响应优先级为0
    NVIC_Init(&NVIC_InitStructure);                                                          //将TIM2中断配置写入NVIC

    /*保持原有启动方式*/
    TIM_Cmd(TIM2, DISABLE);                                                                  //初始化后保持停止并由原主函数显式启动TIM2
}
