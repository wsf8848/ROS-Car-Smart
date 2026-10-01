#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Buzzer.h"                     //提供蜂鸣器驱动接口声明
#include "Delay.h"                      //提供蜂鸣器提示音所需的毫秒延时函数

/**
  * 函    数：蜂鸣器初始化
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义GPIO初始化结构体

    /*开启GPIO时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);                                    //开启蜂鸣器控制引脚所在GPIOB端口时钟

    /*配置蜂鸣器控制引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;                                        //配置为推挽输出以驱动三极管
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;                                               //选择蜂鸣器对应的PB14引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //设置GPIO输出速度为50MHz
    GPIO_Init(GPIOB, &GPIO_InitStructure);                                                   //将蜂鸣器配置写入GPIOB

    /*设置上电默认状态*/
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);                                                      //PB14输出低电平并关闭蜂鸣器
}

/**
  * 函    数：蜂鸣器开启
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_ON(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_14);                                                        //PB14输出高电平并开启蜂鸣器
}

/**
  * 函    数：蜂鸣器关闭
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_OFF(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);                                                      //PB14输出低电平并关闭蜂鸣器
}

/**
  * 函    数：蜂鸣器状态翻转
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_14) == Bit_RESET)                             //判断蜂鸣器当前是否关闭
    {
        Buzzer_ON();                                                                         //当前关闭时开启蜂鸣器
    }
    else                                                                                     //处理蜂鸣器当前开启的情况
    {
        Buzzer_OFF();                                                                        //当前开启时关闭蜂鸣器
    }
}

/**
  * 函    数：蜂鸣器定时闪鸣处理
  * 参    数：PeriodTicks 闪鸣半周期对应的调用次数，0表示常鸣
  * 返 回 值：无
  */
void Buzzer_Flash(uint16_t PeriodTicks)
{
    static uint16_t TickCount = 0U;                                                          //保存距离上次蜂鸣器翻转的调用次数

    if (PeriodTicks == 0U)                                                                   //判断是否请求蜂鸣器常鸣
    {
        Buzzer_ON();                                                                         //周期为0时保持蜂鸣器开启
        TickCount = 0U;                                                                      //清除累计值以便下次重新计时
    }
    else                                                                                     //处理正常的周期闪鸣请求
    {
        TickCount++;                                                                         //累计一次定时调用
        if (TickCount >= PeriodTicks)                                                        //判断是否达到设定的闪鸣半周期
        {
            TickCount = 0U;                                                                  //清零累计值并开始下一半周期
            Buzzer_Turn();                                                                   //翻转蜂鸣器状态形成断续提示音
        }
    }
}

/**
  * 函    数：蜂鸣器启动提示音
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Start(void)
{
    Buzzer_ON();                                                                             //开启蜂鸣器产生第一声提示
    Delay_ms(100U);                                                                          //保持第一声提示100ms
    Buzzer_OFF();                                                                            //关闭蜂鸣器形成提示间隔
    Delay_ms(100U);                                                                          //保持静音间隔100ms
    Buzzer_ON();                                                                             //开启蜂鸣器产生第二声提示
    Delay_ms(100U);                                                                          //保持第二声提示100ms
    Buzzer_OFF();                                                                            //关闭蜂鸣器结束启动提示音
}

/**
  * 函    数：兼容旧命名的蜂鸣器启动提示音
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_start(void)
{
    Buzzer_Start();                                                                          //调用规范命名接口并保持旧工程兼容性
}
