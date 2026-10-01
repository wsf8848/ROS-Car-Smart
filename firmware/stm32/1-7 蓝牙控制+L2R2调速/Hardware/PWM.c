#include "stm32f10x.h"                                // 提供STM32F1寄存器与标准外设库定义
#include "PWM.h"                                      // 提供四路PWM驱动接口声明

#define PWM_PERIOD_COUNTS 3600U                        // 定义电机PWM自动重装值对应的周期计数

/**
  * 函    数：四路电机PWM初始化
  * 参    数：无
  * 返 回 值：无
  */
void PWM_ABCD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;               // 定义PWM GPIO初始化结构体
    TIM_TimeBaseInitTypeDef TimeBaseStructure;         // 定义TIM8时基初始化结构体
    TIM_OCInitTypeDef OutputCompareStructure;          // 定义TIM8输出比较初始化结构体

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM8 | RCC_APB2Periph_AFIO, ENABLE); // 开启PWM端口、TIM8和复用功能时钟

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;     // 配置PC6至PC9为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9; // 选择TIM8四个通道引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 设置PWM引脚输出速度为五十兆赫兹
    GPIO_Init(GPIOC, &GPIO_InitStructure);              // 写入GPIOC四路PWM引脚配置

    TIM_TimeBaseStructInit(&TimeBaseStructure);         // 填充TIM8时基结构体默认值
    TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 设置定时器时钟不再分频
    TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 设置TIM8向上计数
    TimeBaseStructure.TIM_Period = PWM_PERIOD_COUNTS - 1U; // 设置PWM周期计数上限
    TimeBaseStructure.TIM_Prescaler = 0U;               // 使用系统定时器时钟直接计数
    TIM_TimeBaseInit(TIM8, &TimeBaseStructure);         // 将TIM8时基配置写入外设

    TIM_OCStructInit(&OutputCompareStructure);          // 填充输出比较结构体默认值
    OutputCompareStructure.TIM_OCMode = TIM_OCMode_PWM1; // 选择PWM1输出模式
    OutputCompareStructure.TIM_OutputState = TIM_OutputState_Enable; // 使能四路主输出
    OutputCompareStructure.TIM_Pulse = 0U;              // 上电默认输出零占空比
    OutputCompareStructure.TIM_OCPolarity = TIM_OCPolarity_High; // 设置高电平有效极性
    TIM_OC1Init(TIM8, &OutputCompareStructure);         // 初始化TIM8通道1对应左后轮PWM
    TIM_OC2Init(TIM8, &OutputCompareStructure);         // 初始化TIM8通道2对应左前轮PWM
    TIM_OC3Init(TIM8, &OutputCompareStructure);         // 初始化TIM8通道3对应右前轮PWM
    TIM_OC4Init(TIM8, &OutputCompareStructure);         // 初始化TIM8通道4对应右后轮PWM
    TIM_OC1PreloadConfig(TIM8, TIM_OCPreload_Enable);   // 使能通道1比较值预装载
    TIM_OC2PreloadConfig(TIM8, TIM_OCPreload_Enable);   // 使能通道2比较值预装载
    TIM_OC3PreloadConfig(TIM8, TIM_OCPreload_Enable);   // 使能通道3比较值预装载
    TIM_OC4PreloadConfig(TIM8, TIM_OCPreload_Enable);   // 使能通道4比较值预装载
    TIM_ARRPreloadConfig(TIM8, ENABLE);                 // 使能TIM8自动重装载预装载
    TIM_Cmd(TIM8, ENABLE);                              // 启动TIM8计数器
    TIM_CtrlPWMOutputs(TIM8, ENABLE);                   // 使能高级定时器主输出
}

/**
  * 函    数：设置左后轮PWM比较值
  * 参    数：Compare 比较值，范围为零至PWM周期计数
  * 返 回 值：无
  */
void PWM_SetCompare1(uint16_t Compare)
{
    TIM_SetCompare1(TIM8, Compare);                     // 写入TIM8通道1比较寄存器
}

/**
  * 函    数：设置左前轮PWM比较值
  * 参    数：Compare 比较值，范围为零至PWM周期计数
  * 返 回 值：无
  */
void PWM_SetCompare2(uint16_t Compare)
{
    TIM_SetCompare2(TIM8, Compare);                     // 写入TIM8通道2比较寄存器
}

/**
  * 函    数：设置右前轮PWM比较值
  * 参    数：Compare 比较值，范围为零至PWM周期计数
  * 返 回 值：无
  */
void PWM_SetCompare3(uint16_t Compare)
{
    TIM_SetCompare3(TIM8, Compare);                     // 写入TIM8通道3比较寄存器
}

/**
  * 函    数：设置右后轮PWM比较值
  * 参    数：Compare 比较值，范围为零至PWM周期计数
  * 返 回 值：无
  */
void PWM_SetCompare4(uint16_t Compare)
{
    TIM_SetCompare4(TIM8, Compare);                     // 写入TIM8通道4比较寄存器
}
