#include "stm32f10x.h"                                 // 提供STM32F1寄存器与标准外设库定义
#include "RosLink.h"                                   // 提供地瓜派RDK X5ROS串口链路接口声明

static volatile uint8_t RosLinkRxBuffer[ROS_LINK_RX_BUFFER_SIZE]; // 保存USART1中断收到的ROS控制字节
static volatile uint16_t RosLinkRxWriteIndex = 0U;      // 保存USART1接收缓冲区下一个写入位置
static volatile uint16_t RosLinkRxReadIndex = 0U;       // 保存主循环下一个待读取位置
static volatile uint32_t RosLinkRxOverflowCount = 0UL;  // 累计记录接收缓冲区满时丢弃的字节数量

/**
  * 函    数：地瓜派RDK X5ROS串口初始化
  * 参    数：无
  * 返 回 值：无
  */
void RosLink_Init(void) //地瓜派RDK X5ROS串口初始化
{
    GPIO_InitTypeDef GPIO_InitStructure;                 // 定义USART1引脚初始化结构体
    USART_InitTypeDef USART_InitStructure;               // 定义USART1通信参数结构体
    NVIC_InitTypeDef NVIC_InitStructure;                 // 定义USART1中断控制结构体

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE); // 开启APB2总线上的USART1外设时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 开启PA9和PA10端口时钟
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;      // 配置PA9为复用推挽发送模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;            // 选择USART1_TX对应的PA9引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;     // 设置发送引脚输出速度为50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                // 将USART1发送引脚配置写入GPIOA
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;         // 配置PA10为内部上拉接收模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;             // 选择USART1_RX对应的PA10引脚
    GPIO_Init(GPIOA, &GPIO_InitStructure);                // 将USART1接收引脚配置写入GPIOA
    USART_InitStructure.USART_BaudRate = ROS_LINK_BAUD_RATE; // 设置ROS链路波特率为115200bit/s
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; // 关闭硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx; // 同时使能发送和接收
    USART_InitStructure.USART_Parity = USART_Parity_No;   // 配置无奇偶校验
    USART_InitStructure.USART_StopBits = USART_StopBits_1; // 配置一个停止位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; // 配置八位数据长度
    USART_Init(USART1, &USART_InitStructure);              // 将通信参数写入USART1寄存器
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);         // 开启USART1接收数据寄存器非空中断
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;      // 选择USART1全局中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;       // 使能USART1中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U; // 设置USART1高于系统时基的抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3U;   // 设置USART1响应子优先级
    NVIC_Init(&NVIC_InitStructure);                       // 将USART1中断参数写入NVIC
    RosLinkRxWriteIndex = 0U;                             // 清除接收写入位置
    RosLinkRxReadIndex = 0U;                              // 清除接收读取位置
    RosLinkRxOverflowCount = 0UL;                         // 清除接收溢出统计
    USART_Cmd(USART1, ENABLE);                             // 使能USART1开始与地瓜派RDK X5通信
}

/**
  * 函    数：通过USART1发送一个字节
  * 参    数：Byte 待发送的八位数据
  * 返 回 值：无
  */
void RosLink_SendByte(uint8_t Byte) //通过USART1发送一个字节
{
    USART_SendData(USART1, Byte);                          // 将待发送字节写入USART1数据寄存器
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) // 等待发送数据寄存器变为空
    {
    }
}

/**
  * 函    数：保留旧遥测发送兼容空接口
  * 参    数：Array 数组首地址，Length 数组长度
  * 返 回 值：无
  */
void RosLink_SendArray(const uint8_t *Array, uint16_t Length) //保留旧遥测发送兼容空接口
{
    (void)Array;                                         // 保留旧蓝牙遥测调用接口但不把旧协议混入正式ROS链路
    (void)Length;                                        // 保留旧调用参数以确保蓝牙和MQTT模块无需改写
}

/**
  * 函    数：保留旧控制接收兼容空接口
  * 参    数：Byte 用于保存读取结果的变量地址
  * 返 回 值：一表示成功读取，零表示当前没有待处理数据
  */
uint8_t RosLink_ReadByte(uint8_t *Byte) //保留旧控制接收兼容空接口
{
    (void)Byte;                                           // 保留旧蓝牙单字节解析接口但禁止其窃取正式ROS帧
    return 0U;                                            // USART1数据仅允许由正式AA55协议解析器读取
}

/**
  * 函    数：发送正式ROS协议字节数组
  * 参    数：Array 数组首地址，Length 数组长度
  * 返 回 值：无
  */
void RosLink_SendProtocolArray(const uint8_t *Array, uint16_t Length) //发送正式ROS协议字节数组
{
    uint16_t Index;                                      // 保存当前待发送字节下标
    if (Array == 0)                                      // 判断调用者是否提供有效数组地址
    {
        return;                                          // 数组地址无效时不访问数据
    }
    for (Index = 0U; Index < Length; Index++)            // 依次遍历正式协议帧全部字节
    {
        RosLink_SendByte(Array[Index]);                  // 通过USART1发送当前正式协议字节
    }
}

/**
  * 函    数：读取一个正式ROS协议字节
  * 参    数：Byte 用于保存读取结果的变量地址
  * 返 回 值：一表示成功读取，零表示当前没有待处理数据
  */
uint8_t RosLink_ReadProtocolByte(uint8_t *Byte) //读取一个正式ROS协议字节
{
    uint16_t ReadIndex;                                  // 保存本次读取位置
    if (Byte == 0)                                       // 判断结果地址是否有效
    {
        return 0U;                                       // 地址无效时返回读取失败
    }
    if (RosLinkRxReadIndex == RosLinkRxWriteIndex)       // 判断接收环形缓冲区是否为空
    {
        return 0U;                                       // 没有待处理字节时返回零
    }
    ReadIndex = RosLinkRxReadIndex;                      // 复制当前读取位置
    *Byte = RosLinkRxBuffer[ReadIndex];                  // 读取最早尚未处理的正式协议字节
    ReadIndex++;                                         // 移动到下一个读取位置
    if (ReadIndex >= ROS_LINK_RX_BUFFER_SIZE)            // 判断读取位置是否到达数组末尾
    {
        ReadIndex = 0U;                                  // 回绕到缓冲区起始位置
    }
    RosLinkRxReadIndex = ReadIndex;                      // 发布更新后的读取位置
    return 1U;                                           // 返回成功读取一个字节
}

/**
  * 函    数：取得USART1接收溢出次数
  * 参    数：无
  * 返 回 值：累计丢弃的字节数量
  */
uint32_t RosLink_GetRxOverflowCount(void) //取得USART1接收溢出次数
{
    return RosLinkRxOverflowCount;                         // 返回USART1接收溢出诊断计数
}

/**
  * 函    数：USART1接收中断服务
  * 参    数：无
  * 返 回 值：无
  */
void USART1_IRQHandler(void) //USART1接收中断服务
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) == SET)    // 判断USART1是否产生接收中断
    {
        uint8_t ReceivedByte;                              // 保存本次接收的控制字节
        uint16_t NextWriteIndex;                           // 保存下一个写入位置
        ReceivedByte = (uint8_t)USART_ReceiveData(USART1);  // 读取数据并清除RXNE状态
        NextWriteIndex = (uint16_t)(RosLinkRxWriteIndex + 1U); // 计算下一个环形写入位置
        if (NextWriteIndex >= ROS_LINK_RX_BUFFER_SIZE)     // 判断写入位置是否到达末尾
        {
            NextWriteIndex = 0U;                           // 回绕到缓冲区起始位置
        }
        if (NextWriteIndex != RosLinkRxReadIndex)          // 判断环形缓冲区是否有空闲位置
        {
            RosLinkRxBuffer[RosLinkRxWriteIndex] = ReceivedByte; // 保存当前ROS控制字节
            RosLinkRxWriteIndex = NextWriteIndex;           // 发布新的写入位置
        }
        else                                               // 处理接收缓冲区已满的情况
        {
            RosLinkRxOverflowCount++;                      // 记录一次丢弃事件
        }
    }
}
