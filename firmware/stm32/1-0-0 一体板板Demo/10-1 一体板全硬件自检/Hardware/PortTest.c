#include "stm32f10x.h"                                // 提供STM32F1寄存器与标准外设库定义
#include "Delay.h"                                    // 提供串口回环等待所需微秒延时
#include "PortTest.h"                                 // 提供板载串口测试接口声明
#include "Serial.h"                                   // 提供USART2发送和接收缓冲接口

#define PORT_TEST_BAUD_RATE 9600U                      // 保持原Demo中USART1、UART4和UART5的波特率
#define PORT_TEST_WAIT_US 50000U                       // 定义单路回环响应最长等待五十毫秒

/**
  * 函    数：初始化单个外部串口
  * 参    数：Port 串口实例，BaudRate 波特率
  * 返 回 值：无
  */
static void PortTest_InitUsart(USART_TypeDef *Port, uint32_t BaudRate)
{
    USART_InitTypeDef UsartStructure;                   // 定义串口参数配置结构体

    UsartStructure.USART_BaudRate = BaudRate;           // 设置目标串口波特率
    UsartStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 关闭硬件流控
    UsartStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx; // 同时开启发送与接收
    UsartStructure.USART_Parity = USART_Parity_No;      // 设置无校验位
    UsartStructure.USART_StopBits = USART_StopBits_1;  // 设置一个停止位
    UsartStructure.USART_WordLength = USART_WordLength_8b; // 设置八位数据长度
    USART_Init(Port, &UsartStructure);                  // 将参数写入目标串口
    USART_Cmd(Port, ENABLE);                            // 启动目标串口
}

/**
  * 函    数：初始化USART1、UART4和UART5
  * 参    数：无
  * 返 回 值：无
  */
void PortTest_InitExternal(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                // 定义串口GPIO配置结构体

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD | RCC_APB2Periph_USART1, ENABLE); // 开启USART1及相关GPIO时钟
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4 | RCC_APB1Periph_UART5, ENABLE); // 开启UART4和UART5外设时钟
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;     // 配置发送引脚为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;           // 选择USART1_TX对应的PA9
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 设置发送引脚输出速度
    GPIO_Init(GPIOA, &GPIO_InitStructure);              // 写入USART1发送引脚配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;       // 配置接收引脚为内部上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;          // 选择USART1_RX对应的PA10
    GPIO_Init(GPIOA, &GPIO_InitStructure);              // 写入USART1接收引脚配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;     // 恢复发送引脚复用推挽配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;          // 选择UART4_TX对应的PC10
    GPIO_Init(GPIOC, &GPIO_InitStructure);              // 写入UART4发送引脚配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;       // 恢复接收引脚内部上拉配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;          // 选择UART4_RX对应的PC11
    GPIO_Init(GPIOC, &GPIO_InitStructure);              // 写入UART4接收引脚配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;     // 恢复发送引脚复用推挽配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;          // 选择UART5_TX对应的PC12
    GPIO_Init(GPIOC, &GPIO_InitStructure);              // 写入UART5发送引脚配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;       // 恢复接收引脚内部上拉配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;           // 选择UART5_RX对应的PD2
    GPIO_Init(GPIOD, &GPIO_InitStructure);              // 写入UART5接收引脚配置
    PortTest_InitUsart(USART1, PORT_TEST_BAUD_RATE);    // 初始化USART1为9600-8-N-1
    PortTest_InitUsart(UART4, PORT_TEST_BAUD_RATE);     // 初始化UART4为9600-8-N-1
    PortTest_InitUsart(UART5, PORT_TEST_BAUD_RATE);     // 初始化UART5为9600-8-N-1
}

/**
  * 函    数：关闭USART1并释放冲突引脚
  * 参    数：无
  * 返 回 值：无
  */
void PortTest_DisableUSART1(void)
{
    USART_Cmd(USART1, DISABLE);                         // 关闭USART1以便PA9和PA10切换到舵机PWM
}

/**
  * 函    数：对单个无中断串口执行回环检测
  * 参    数：Port 目标串口，Token 本次测试字节
  * 返 回 值：一表示收到相同字节，零表示超时或不一致
  */
static uint8_t PortTest_CheckPolling(USART_TypeDef *Port, uint8_t Token)
{
    uint32_t WaitCount;                                 // 保存回环接收等待的微秒计数

    while (USART_GetFlagStatus(Port, USART_FLAG_RXNE) == SET) // 清除测试前残留的接收字节
    {
        (void)USART_ReceiveData(Port);                  // 读取并丢弃旧数据
    }
    USART_SendData(Port, Token);                        // 发送当前端口专用测试字节
    while (USART_GetFlagStatus(Port, USART_FLAG_TC) == RESET) // 等待完整帧发送结束
    {
    }
    for (WaitCount = 0U; WaitCount < PORT_TEST_WAIT_US; WaitCount++) // 在限定时间内轮询回环字节
    {
        if (USART_GetFlagStatus(Port, USART_FLAG_RXNE) == SET) // 判断接收寄存器是否收到数据
        {
            return (uint8_t)(((uint8_t)USART_ReceiveData(Port) == Token) ? 1U : 0U); // 比较回环字节并返回结果
        }
        Delay_us(1U);                                  // 每次轮询间隔一微秒
    }
    return 0U;                                         // 超时未收到数据时报告失败
}

/**
  * 函    数：执行四串口回环检测
  * 参    数：无
  * 返 回 值：低四位分别表示USART1、USART2、UART4、UART5是否通过
  */
uint8_t PortTest_RunLoopback(void)
{
    uint8_t Result = 0U;                               // 初始化四路回环结果位图
    uint8_t ReceivedByte = 0U;                         // 保存USART2中断缓冲区回环字节

    PortTest_InitExternal();                           // 恢复可能被舵机测试占用的USART1引脚
    if (PortTest_CheckPolling(USART1, 0xA1U) != 0U)   // 检查USART1的PA9到PA10回环
    {
        Result |= PORT_TEST_USART1_OK;                 // 标记USART1回环通过
    }
    while (Serial_ReadByte(&ReceivedByte) != 0U)       // 清空USART2测试前的接收缓冲区
    {
    }
    Serial_SendByte(0xA2U);                            // 通过USART2发送专用回环字节
    Delay_ms(50U);                                     // 等待USART2中断接收回环数据
    if ((Serial_ReadByte(&ReceivedByte) != 0U) && (ReceivedByte == 0xA2U)) // 检查USART2是否收到相同字节
    {
        Result |= PORT_TEST_USART2_OK;                 // 标记USART2回环通过
    }
    if (PortTest_CheckPolling(UART4, 0xA4U) != 0U)    // 检查UART4的PC10到PC11回环
    {
        Result |= PORT_TEST_UART4_OK;                  // 标记UART4回环通过
    }
    if (PortTest_CheckPolling(UART5, 0xA5U) != 0U)    // 检查UART5的PC12到PD2回环
    {
        Result |= PORT_TEST_UART5_OK;                  // 标记UART5回环通过
    }
    return Result;                                     // 返回四个串口的综合测试位图
}
