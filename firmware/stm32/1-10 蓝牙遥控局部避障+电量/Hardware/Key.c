#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Delay.h"                      //提供按键消抖所需的毫秒延时函数
#include "Key.h"                        //提供按键驱动接口声明

/**
  * 函    数：按键初始化
  * 参    数：无
  * 返 回 值：无
  */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义GPIO初始化结构体

    /*开启GPIO时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);                                    //开启三个按键所在GPIOC端口时钟

    /*配置按键输入引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;                                           //配置为内部上拉输入以匹配按下接地电路
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2;                      //选择KEY1至KEY3对应的PC0、PC1和PC2
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //填写GPIO速度字段以保持结构体配置完整
    GPIO_Init(GPIOC, &GPIO_InitStructure);                                                   //将按键配置写入GPIOC
}

/**
  * 函    数：获取单次按键键值
  * 参    数：无
  * 返 回 值：0表示无按键，1至3分别表示PC0至PC2按键
  */
uint8_t Key_GetNum(void)
{
    uint8_t KeyNumber = 0U;                                                                 //默认返回无按键状态

    if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_0) == Bit_RESET)                              //判断KEY1是否被按下
    {
        Delay_ms(20U);                                                                       //等待20ms消除按下抖动
        while (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_0) == Bit_RESET)                        //等待KEY1释放以产生单次按键事件
        {
        }
        Delay_ms(20U);                                                                       //等待20ms消除释放抖动
        KeyNumber = 1U;                                                                      //记录KEY1键值
    }
    else if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_RESET)                         //判断KEY2是否被按下
    {
        Delay_ms(20U);                                                                       //等待20ms消除按下抖动
        while (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_1) == Bit_RESET)                        //等待KEY2释放以产生单次按键事件
        {
        }
        Delay_ms(20U);                                                                       //等待20ms消除释放抖动
        KeyNumber = 2U;                                                                      //记录KEY2键值
    }
    else if (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == Bit_RESET)                         //判断KEY3是否被按下
    {
        Delay_ms(20U);                                                                       //等待20ms消除按下抖动
        while (GPIO_ReadInputDataBit(GPIOC, GPIO_Pin_2) == Bit_RESET)                        //等待KEY3释放以产生单次按键事件
        {
        }
        Delay_ms(20U);                                                                       //等待20ms消除释放抖动
        KeyNumber = 3U;                                                                      //记录KEY3键值
    }
    else                                                                                     //处理没有按键被按下的情况
    {
        KeyNumber = 0U;                                                                      //保持无按键返回值
    }

    return KeyNumber;                                                                        //返回经过消抖后的单次键值
}
