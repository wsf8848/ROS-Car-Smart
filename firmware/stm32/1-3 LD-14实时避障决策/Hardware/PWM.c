#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "PWM.h"                       //提供TIM8四通道PWM接口声明

#define MOTOR_PWM_PERIOD 3600U          //定义与验证工程一致的TIM8自动重装载周期

/**
  * 函    数：四路电机PWM初始化
  * 参    数：无
  * 返 回 值：无
  */
void PWM_ABCD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义PWM GPIO初始化结构体
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;                                      //定义TIM8时基初始化结构体
    TIM_OCInitTypeDef TIM_OCInitStructure;                                                   //定义TIM8输出比较初始化结构体

    /*开启PWM外设时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM8 | RCC_APB2Periph_AFIO, ENABLE); //开启GPIOC、TIM8和AFIO外设时钟

    /*配置四个PWM输出引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;                                         //配置PC6至PC9为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7 | GPIO_Pin_8 | GPIO_Pin_9;        //选择TIM8四个PWM通道对应引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //设置PWM引脚输出速度为五十兆赫兹
    GPIO_Init(GPIOC, &GPIO_InitStructure);                                                   //将四个PWM引脚配置写入GPIOC

    /*配置TIM8基础计数参数*/
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;                              //配置定时器采样时钟不分频
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;                          //配置TIM8向上计数模式
    TIM_TimeBaseInitStructure.TIM_Period = MOTOR_PWM_PERIOD - 1U;                           //设置PWM周期为三千六百个计数单位
    TIM_TimeBaseInitStructure.TIM_Prescaler = 0U;                                           //使用TIM8原始定时器时钟获得高频PWM
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0U;                                   //高级定时器不使用重复计数功能
    TIM_TimeBaseInit(TIM8, &TIM_TimeBaseInitStructure);                                     //将TIM8基础参数写入定时器

    /*配置四个PWM输出比较通道*/
    TIM_OCStructInit(&TIM_OCInitStructure);                                                 //将输出比较结构体填充为标准安全默认值
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;                                       //配置通道工作在PWM1模式
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;                            //使能四路主输出通道
    TIM_OCInitStructure.TIM_Pulse = 0U;                                                      //初始化四个通道占空比为零
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;                                //配置PWM有效电平为高电平
    TIM_OC1Init(TIM8, &TIM_OCInitStructure);                                                 //初始化TIM8通道一对应左后电机
    TIM_OC2Init(TIM8, &TIM_OCInitStructure);                                                 //初始化TIM8通道二对应左前电机
    TIM_OC3Init(TIM8, &TIM_OCInitStructure);                                                 //初始化TIM8通道三对应右前电机
    TIM_OC4Init(TIM8, &TIM_OCInitStructure);                                                 //初始化TIM8通道四对应右后电机
    TIM_OC1PreloadConfig(TIM8, TIM_OCPreload_Enable);                                       //开启通道一比较值预装载
    TIM_OC2PreloadConfig(TIM8, TIM_OCPreload_Enable);                                       //开启通道二比较值预装载
    TIM_OC3PreloadConfig(TIM8, TIM_OCPreload_Enable);                                       //开启通道三比较值预装载
    TIM_OC4PreloadConfig(TIM8, TIM_OCPreload_Enable);                                       //开启通道四比较值预装载
    TIM_ARRPreloadConfig(TIM8, ENABLE);                                                      //开启TIM8自动重装载预装载
    TIM_CtrlPWMOutputs(TIM8, ENABLE);                                                        //开启高级定时器主输出控制
    TIM_Cmd(TIM8, ENABLE);                                                                   //启动TIM8并输出四路PWM
}

/**
  * 函    数：设置左后电机PWM比较值
  * 参    数：Compare PWM比较值
  * 返 回 值：无
  */
void PWM_SetCompare1(uint16_t Compare)
{
    TIM_SetCompare1(TIM8, Compare);                                                          //将左后电机PWM比较值写入TIM8通道一
}

/**
  * 函    数：设置左前电机PWM比较值
  * 参    数：Compare PWM比较值
  * 返 回 值：无
  */
void PWM_SetCompare2(uint16_t Compare)
{
    TIM_SetCompare2(TIM8, Compare);                                                          //将左前电机PWM比较值写入TIM8通道二
}

/**
  * 函    数：设置右前电机PWM比较值
  * 参    数：Compare PWM比较值
  * 返 回 值：无
  */
void PWM_SetCompare3(uint16_t Compare)
{
    TIM_SetCompare3(TIM8, Compare);                                                          //将右前电机PWM比较值写入TIM8通道三
}

/**
  * 函    数：设置右后电机PWM比较值
  * 参    数：Compare PWM比较值
  * 返 回 值：无
  */
void PWM_SetCompare4(uint16_t Compare)
{
    TIM_SetCompare4(TIM8, Compare);                                                          //将右后电机PWM比较值写入TIM8通道四
}
