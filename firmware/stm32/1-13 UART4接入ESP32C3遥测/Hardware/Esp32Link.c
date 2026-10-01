#include "stm32f10x.h"                                   //提供STM32F103RCT6寄存器与标准外设库定义
#include "Esp32Link.h"                                    //提供ESP32-C3串口链路接口声明

static volatile uint8_t Esp32LinkRxBuffer[ESP32_LINK_RX_BUFFER_SIZE]; //保存UART4中断收到的ESP32-C3下发字节
static volatile uint8_t Esp32LinkRxWriteIndex = 0U;        //保存UART4接收缓冲区下一个写入位置
static volatile uint8_t Esp32LinkRxReadIndex = 0U;         //保存主循环下一个待读取位置
static volatile uint32_t Esp32LinkRxOverflowCount = 0UL;   //累计记录接收缓冲区已满时丢弃的字节数量

/**
  * 函    数：ESP32-C3串口链路初始化
  * 参    数：无
  * 返 回 值：无
  */
void Esp32Link_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                   //定义UART4引脚初始化结构体
    USART_InitTypeDef USART_InitStructure;                 //定义UART4通信参数结构体
    NVIC_InitTypeDef NVIC_InitStructure;                   //定义UART4中断控制结构体

    /*开启UART4和GPIOC时钟*/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);   //开启APB1总线上的UART4外设时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);   //开启UART4引脚所在GPIOC端口时钟

    /*配置UART4发送引脚PC10*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;        //配置PC10为复用推挽输出模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;             //选择UART4_TX对应的PC10引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;      //设置发送引脚输出速度为50MHz
    GPIO_Init(GPIOC, &GPIO_InitStructure);                  //将UART4发送引脚配置写入GPIOC

    /*配置UART4接收引脚PC11*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;          //配置PC11为内部上拉输入模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;             //选择UART4_RX对应的PC11引脚
    GPIO_Init(GPIOC, &GPIO_InitStructure);                  //将UART4接收引脚配置写入GPIOC

    /*配置UART4通信参数*/
    USART_InitStructure.USART_BaudRate = ESP32_LINK_BAUD_RATE; //设置STM32与ESP32-C3通信波特率为115200bit/s
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //关闭RTS和CTS硬件流控
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx; //同时使能遥测发送与后续控制接收
    USART_InitStructure.USART_Parity = USART_Parity_No;    //配置为无奇偶校验
    USART_InitStructure.USART_StopBits = USART_StopBits_1; //配置为一个停止位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b; //配置为八位数据长度
    USART_Init(UART4, &USART_InitStructure);                //将通信参数写入UART4寄存器

    /*配置UART4接收中断*/
    USART_ITConfig(UART4, USART_IT_RXNE, ENABLE);           //开启UART4接收数据寄存器非空中断
    NVIC_InitStructure.NVIC_IRQChannel = UART4_IRQn;        //选择UART4全局中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;         //使能UART4中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1U; //设置UART4抢占优先级为1
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2U;     //设置UART4响应优先级低于USART2蓝牙接收
    NVIC_Init(&NVIC_InitStructure);                         //将UART4中断配置写入NVIC

    /*清空软件接收状态并启动UART4*/
    Esp32LinkRxWriteIndex = 0U;                            //清除上电时的接收写入位置
    Esp32LinkRxReadIndex = 0U;                             //清除上电时的接收读取位置
    Esp32LinkRxOverflowCount = 0UL;                        //清除上电时的接收溢出统计
    USART_Cmd(UART4, ENABLE);                               //使能UART4并开始与ESP32-C3通信
}

/**
  * 函    数：向ESP32-C3发送一个字节
  * 参    数：Byte 待发送的八位数据
  * 返 回 值：无
  */
void Esp32Link_SendByte(uint8_t Byte)
{
    USART_SendData(UART4, Byte);                           //将待发送字节写入UART4数据寄存器
    while (USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET) //等待发送数据寄存器重新变为空
    {
    }
}

/**
  * 函    数：向ESP32-C3发送字节数组
  * 参    数：Array 数组首地址，Length 数组长度
  * 返 回 值：无
  */
void Esp32Link_SendArray(const uint8_t *Array, uint16_t Length)
{
    uint16_t Index;                                        //保存当前待发送字节下标

    if (Array == 0)                                        //判断调用者是否提供有效数组地址
    {
        return;                                            //数组地址无效时不访问数据
    }
    for (Index = 0U; Index < Length; Index++)              //依次遍历需要发送的全部字节
    {
        Esp32Link_SendByte(Array[Index]);                  //通过UART4发送当前数组字节
    }
}

/**
  * 函    数：读取一个ESP32-C3下发字节
  * 参    数：Byte 用于保存读取结果的变量地址
  * 返 回 值：一表示成功读取，零表示当前没有待处理数据
  */
uint8_t Esp32Link_ReadByte(uint8_t *Byte)
{
    uint8_t ReadIndex;                                     //保存本次准备读取的环形缓冲区位置

    if (Byte == 0)                                         //判断调用者是否提供有效结果地址
    {
        return 0U;                                         //地址无效时不访问接收缓冲区
    }
    if (Esp32LinkRxReadIndex == Esp32LinkRxWriteIndex)     //判断UART4接收环形缓冲区是否为空
    {
        return 0U;                                         //没有待处理字节时返回零
    }
    ReadIndex = Esp32LinkRxReadIndex;                      //复制当前读取位置以取得稳定数组下标
    *Byte = Esp32LinkRxBuffer[ReadIndex];                  //读取最早一个尚未处理的ESP32-C3字节
    ReadIndex++;                                           //移动到环形缓冲区下一个读取位置
    if (ReadIndex >= ESP32_LINK_RX_BUFFER_SIZE)            //判断读取位置是否到达数组末尾
    {
        ReadIndex = 0U;                                    //回绕到接收环形缓冲区起始位置
    }
    Esp32LinkRxReadIndex = ReadIndex;                      //发布更新后的主循环读取位置
    return 1U;                                             //返回成功读取一个接收字节
}

/**
  * 函    数：取得UART4接收缓冲区溢出次数
  * 参    数：无
  * 返 回 值：因缓冲区已满而丢弃的累计字节数量
  */
uint32_t Esp32Link_GetRxOverflowCount(void)
{
    return Esp32LinkRxOverflowCount;                       //返回可供后续通信诊断使用的溢出统计
}

/**
  * 函    数：UART4接收中断服务
  * 参    数：无
  * 返 回 值：无
  */
void UART4_IRQHandler(void)
{
    if (USART_GetITStatus(UART4, USART_IT_RXNE) == SET)    //判断UART4是否产生接收寄存器非空中断
    {
        uint8_t ReceivedByte;                              //保存本次从UART4读取的ESP32-C3字节
        uint8_t NextWriteIndex;                            //保存写入当前字节后的下一个缓冲区位置

        ReceivedByte = (uint8_t)USART_ReceiveData(UART4);  //读取接收数据并清除RXNE硬件状态
        NextWriteIndex = (uint8_t)(Esp32LinkRxWriteIndex + 1U); //计算环形缓冲区下一个写入位置
        if (NextWriteIndex >= ESP32_LINK_RX_BUFFER_SIZE)   //判断写入位置是否到达数组末尾
        {
            NextWriteIndex = 0U;                           //回绕到接收环形缓冲区起始位置
        }
        if (NextWriteIndex != Esp32LinkRxReadIndex)        //判断接收环形缓冲区是否仍有空闲位置
        {
            Esp32LinkRxBuffer[Esp32LinkRxWriteIndex] = ReceivedByte; //保存当前ESP32-C3下发字节
            Esp32LinkRxWriteIndex = NextWriteIndex;        //发布写入完成后的新位置
        }
        else                                               //处理主循环尚未读取且缓冲区已经占满的情况
        {
            Esp32LinkRxOverflowCount++;                    //累计一次丢弃事件供后续诊断链路负载
        }
    }
}
