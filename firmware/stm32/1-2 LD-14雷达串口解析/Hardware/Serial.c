#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Serial.h"                     //提供USART2串口驱动接口声明
#include <stdio.h>                       //提供printf重定向所需的FILE与格式化函数定义
#include <stdarg.h>                      //提供可变参数列表处理定义

#define SERIAL_FORMAT_BUFFER_SIZE 100U   //定义串口格式化发送缓冲区容量

uint8_t RxData = 0U;                     //保存USART2最近一次接收的数据并兼容原主函数变量名
volatile uint8_t Serial_RxFlag = 0U;     //标记USART2是否收到尚未处理的新数据

/**
  * 函    数：USART2串口初始化
  * 参    数：无
  * 返 回 值：无
  */
void Serial_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义GPIO初始化结构体
    USART_InitTypeDef USART_InitStructure;                                                  //定义USART初始化结构体
    NVIC_InitTypeDef NVIC_InitStructure;                                                    //定义NVIC初始化结构体

    /*开启外设时钟*/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);                                   //开启APB1总线上的USART2外设时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);                                    //开启USART2引脚所在GPIOA端口时钟

    /*配置USART2发送引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;                                         //配置PA2为复用推挽输出模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;                                                //选择USART2_TX对应的PA2引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //设置发送引脚输出速度为50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                                                   //将USART2发送引脚配置写入GPIOA

    /*配置USART2接收引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;                                           //配置PA3为内部上拉输入模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;                                                //选择USART2_RX对应的PA3引脚
    GPIO_Init(GPIOA, &GPIO_InitStructure);                                                   //将USART2接收引脚配置写入GPIOA

    /*配置USART2通信参数*/
    USART_InitStructure.USART_BaudRate = 115200U;                                            //设置串口波特率为115200bit/s
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;           //关闭RTS和CTS硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;                          //同时使能串口发送与接收功能
    USART_InitStructure.USART_Parity = USART_Parity_No;                                      //配置为无奇偶校验
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                                   //配置为一个停止位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                              //配置为八位数据长度
    USART_Init(USART2, &USART_InitStructure);                                                 //将通信参数写入USART2寄存器

    /*配置USART2接收中断*/
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);                                            //开启USART2接收数据寄存器非空中断
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);                                           //使用两位抢占和两位响应优先级分组
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;                                        //选择USART2全局中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                                          //使能USART2中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U;                               //设置USART2抢占优先级为1
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1U;                                      //设置USART2响应优先级为1
    NVIC_Init(&NVIC_InitStructure);                                                           //将USART2中断配置写入NVIC

    /*启动USART2*/
    USART_Cmd(USART2, ENABLE);                                                                //使能USART2并开始串口收发
}

/**
  * 函    数：串口发送一个字节
  * 参    数：Byte 待发送的八位数据
  * 返 回 值：无
  */
void Serial_SendByte(uint8_t Byte)
{
    USART_SendData(USART2, Byte);                                                            //将待发送字节写入USART2数据寄存器
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET)                             //等待发送数据寄存器重新变为空
    {
    }
}

/**
  * 函    数：串口发送字节数组
  * 参    数：Array 数组首地址，Length 数组长度
  * 返 回 值：无
  */
void Serial_SendArray(const uint8_t *Array, uint16_t Length)
{
    uint16_t Index;                                                                          //保存当前待发送元素下标

    for (Index = 0U; Index < Length; Index++)                                                //依次遍历数组中的每一个字节
    {
        Serial_SendByte(Array[Index]);                                                       //发送当前下标对应的数组字节
    }
}

/**
  * 函    数：串口发送字符串
  * 参    数：String 以结束符结尾的字符串首地址
  * 返 回 值：无
  */
void Serial_SendString(const char *String)
{
    uint16_t Index;                                                                          //保存当前待发送字符下标

    for (Index = 0U; String[Index] != '\0'; Index++)                                         //遍历字符串直至遇到结束符
    {
        Serial_SendByte((uint8_t)String[Index]);                                             //发送当前下标对应的字符
    }
}

/**
  * 函    数：计算无符号整数次方
  * 参    数：Base 底数，Exponent 指数
  * 返 回 值：Base的Exponent次方
  */
static uint32_t Serial_Pow(uint32_t Base, uint32_t Exponent)
{
    uint32_t Result = 1U;                                                                    //将乘方结果初始化为1

    while (Exponent > 0U)                                                                    //按指数次数循环执行乘法
    {
        Result *= Base;                                                                      //将底数累乘到结果中
        Exponent--;                                                                          //减少一次尚未执行的乘法次数
    }

    return Result;                                                                           //返回计算完成的整数乘方结果
}

/**
  * 函    数：串口发送固定长度十进制数字
  * 参    数：Number 待发送数字，Length 显示位数
  * 返 回 值：无
  */
void Serial_SendNumber(uint32_t Number, uint8_t Length)
{
    uint8_t Index;                                                                           //保存当前待发送数字位下标

    for (Index = 0U; Index < Length; Index++)                                                //按指定长度遍历每一位十进制数字
    {
        Serial_SendByte((uint8_t)(Number / Serial_Pow(10U, Length - Index - 1U) % 10U + '0'));//计算并发送当前十进制位字符
    }
}

/**
  * 函    数：重定向标准库单字符输出
  * 参    数：Character 待输出字符，Stream 标准库文件流
  * 返 回 值：已发送字符
  */
int fputc(int Character, FILE *Stream)
{
    (void)Stream;                                                                            //明确忽略未使用的文件流参数
    Serial_SendByte((uint8_t)Character);                                                     //将标准库输出字符转发到USART2
    return Character;                                                                        //按标准库约定返回已发送字符
}

/**
  * 函    数：串口格式化输出
  * 参    数：Format 格式字符串，后续为对应的可变参数
  * 返 回 值：无
  */
void Serial_Printf(const char *Format, ...)
{
    char Buffer[SERIAL_FORMAT_BUFFER_SIZE];                                                  //定义格式化字符串临时缓冲区
    va_list Arguments;                                                                       //定义可变参数列表对象

    va_start(Arguments, Format);                                                             //从格式字符串之后开始读取可变参数
    vsprintf(Buffer, Format, Arguments);                                                     //将格式化结果写入临时缓冲区
    va_end(Arguments);                                                                       //结束可变参数读取过程
    Serial_SendString(Buffer);                                                               //通过USART2发送格式化后的字符串
}

/**
  * 函    数：USART2接收中断服务
  * 参    数：无
  * 返 回 值：无
  */
void USART2_IRQHandler(void)
{
    if (USART_GetITStatus(USART2, USART_IT_RXNE) == SET)                                     //判断USART2是否产生接收寄存器非空中断
    {
        RxData = (uint8_t)USART_ReceiveData(USART2);                                         //读取接收数据并自动清除RXNE硬件状态
        Serial_RxFlag = 1U;                                                                  //通知主循环处理新收到的串口数据
    }
}
