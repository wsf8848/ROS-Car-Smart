#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "LED.h"                        //提供LED驱动接口声明

/**
  * 函    数：LED初始化
  * 参    数：无
  * 返 回 值：无
  */
void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义GPIO初始化结构体

    /*开启GPIO时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);              //开启LED所在GPIOB和GPIOC端口时钟

    /*配置红色LED引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;                                        //配置为推挽输出模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;                                                //选择红色LED对应的PC3引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //设置GPIO输出速度为50MHz
    GPIO_Init(GPIOC, &GPIO_InitStructure);                                                   //将红色LED配置写入GPIOC

    /*配置绿色和蓝色LED引脚*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12 | GPIO_Pin_13;                                 //选择绿色PB12和蓝色PB13引脚
    GPIO_Init(GPIOB, &GPIO_InitStructure);                                                   //将绿色和蓝色LED配置写入GPIOB

    /*设置上电默认状态*/
    GPIO_SetBits(GPIOC, GPIO_Pin_3);                                                         //PC3输出高电平并关闭红色LED
    GPIO_SetBits(GPIOB, GPIO_Pin_12 | GPIO_Pin_13);                                          //PB12和PB13输出高电平并关闭绿色与蓝色LED
}

/**
  * 函    数：红色LED开启
  * 参    数：无
  * 返 回 值：无
  */
void LED_R_ON(void)
{
    GPIO_ResetBits(GPIOC, GPIO_Pin_3);                                                       //PC3输出低电平并点亮红色LED
}

/**
  * 函    数：红色LED关闭
  * 参    数：无
  * 返 回 值：无
  */
void LED_R_OFF(void)
{
    GPIO_SetBits(GPIOC, GPIO_Pin_3);                                                         //PC3输出高电平并熄灭红色LED
}

/**
  * 函    数：红色LED状态翻转
  * 参    数：无
  * 返 回 值：无
  */
void LED_R_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOC, GPIO_Pin_3) == Bit_RESET)                              //判断红色LED当前是否点亮
    {
        LED_R_OFF();                                                                         //当前点亮时关闭红色LED
    }
    else                                                                                     //处理红色LED当前熄灭的情况
    {
        LED_R_ON();                                                                          //当前熄灭时点亮红色LED
    }
}

/**
  * 函    数：绿色LED开启
  * 参    数：无
  * 返 回 值：无
  */
void LED_G_ON(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);                                                      //PB12输出低电平并点亮绿色LED
}

/**
  * 函    数：绿色LED关闭
  * 参    数：无
  * 返 回 值：无
  */
void LED_G_OFF(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_12);                                                        //PB12输出高电平并熄灭绿色LED
}

/**
  * 函    数：绿色LED状态翻转
  * 参    数：无
  * 返 回 值：无
  */
void LED_G_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_12) == Bit_RESET)                             //判断绿色LED当前是否点亮
    {
        LED_G_OFF();                                                                         //当前点亮时关闭绿色LED
    }
    else                                                                                     //处理绿色LED当前熄灭的情况
    {
        LED_G_ON();                                                                          //当前熄灭时点亮绿色LED
    }
}

/**
  * 函    数：蓝色LED开启
  * 参    数：无
  * 返 回 值：无
  */
void LED_B_ON(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_13);                                                      //PB13输出低电平并点亮蓝色LED
}

/**
  * 函    数：蓝色LED关闭
  * 参    数：无
  * 返 回 值：无
  */
void LED_B_OFF(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_13);                                                        //PB13输出高电平并熄灭蓝色LED
}

/**
  * 函    数：蓝色LED状态翻转
  * 参    数：无
  * 返 回 值：无
  */
void LED_B_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_13) == Bit_RESET)                             //判断蓝色LED当前是否点亮
    {
        LED_B_OFF();                                                                         //当前点亮时关闭蓝色LED
    }
    else                                                                                     //处理蓝色LED当前熄灭的情况
    {
        LED_B_ON();                                                                          //当前熄灭时点亮蓝色LED
    }
}

/**
  * 函    数：兼容接口LED开启
  * 参    数：无
  * 返 回 值：无
  */
void LED_ON(void)
{
    LED_R_ON();                                                                              //兼容旧接口并开启红色LED
}

/**
  * 函    数：兼容接口LED关闭
  * 参    数：无
  * 返 回 值：无
  */
void LED_OFF(void)
{
    LED_R_OFF();                                                                             //兼容旧接口并关闭红色LED
}

/**
  * 函    数：兼容接口LED翻转
  * 参    数：无
  * 返 回 值：无
  */
void LED_Turn(void)
{
    LED_R_Turn();                                                                            //兼容旧接口并翻转红色LED
}

/**
  * 函    数：LED定时闪烁处理
  * 参    数：PeriodTicks 闪烁半周期对应的调用次数，0表示常亮
  * 返 回 值：无
  */
void Led_Flash(uint16_t PeriodTicks)
{
    static uint16_t TickCount = 0;                                                           //保存距离上次翻转的调用次数

    if (PeriodTicks == 0U)                                                                   //判断是否请求红色LED常亮
    {
        LED_R_ON();                                                                          //周期为0时保持红色LED点亮
        TickCount = 0U;                                                                      //清除闪烁累计值以便下次重新计时
    }
    else                                                                                     //处理正常的周期闪烁请求
    {
        TickCount++;                                                                         //累计一次定时调用
        if (TickCount >= PeriodTicks)                                                        //判断是否达到设定的闪烁半周期
        {
            TickCount = 0U;                                                                  //清零累计值并开始下一半周期
            LED_R_Turn();                                                                    //翻转红色LED状态形成闪烁效果
        }
    }
}

/**
  * 函    数：红色LED一秒双翻转处理
  * 参    数：无
  * 返 回 值：无
  */
void LED_Second_Flash(void)
{
    static uint8_t TickCount = 0;                                                            //累计10ms定时调用次数

    TickCount++;                                                                             //每次调用累计10ms
    if (TickCount == 90U)                                                                    //累计到900ms时进行第一次翻转
    {
        LED_R_Turn();                                                                        //翻转红色LED形成100ms脉冲
    }
    else if (TickCount >= 100U)                                                              //累计到1000ms时结束一个完整周期
    {
        LED_R_Turn();                                                                        //再次翻转红色LED恢复原状态
        TickCount = 0U;                                                                      //清零计数并开始下一个周期
    }
}
