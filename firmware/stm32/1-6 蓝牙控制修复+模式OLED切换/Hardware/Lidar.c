#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Lidar.h"                      //提供LD-14激光雷达驱动接口与数据结构
#include "Serial.h"                     //提供USART2调试信息发送接口

#define LIDAR_FRAME_HEADER 0x54U         //定义LD-14数据帧固定帧头
#define LIDAR_FRAME_VER_LEN 0x2CU        //定义LD-14十二点测量帧固定类型长度字节
#define LIDAR_CRC_POLYNOMIAL 0x4DU       //定义LD-14 CRC8计算使用的生成多项式
#define LIDAR_FULL_CIRCLE 36000UL        //定义放大一百倍后的完整圆周角度
#define LIDAR_ANGLE_INTERVALS 11UL       //定义十二个端点测量值之间包含十一个角度间隔

static volatile uint8_t LidarReceiveRaw[LIDAR_FRAME_SIZE];                                  //保存USART1中断当前正在组装的原始帧
static volatile uint8_t LidarLatestRaw[LIDAR_FRAME_SIZE];                                   //保存最近一个通过CRC检查的完整原始帧
static volatile uint8_t LidarReceiveIndex = 0U;                                             //保存当前原始帧接收位置
static volatile uint8_t LidarReceiveCrc = 0U;                                               //保存当前原始帧的滚动CRC8值
static volatile uint8_t LidarFrameReady = 0U;                                               //标记是否存在尚未读取的有效原始帧
static volatile uint8_t LidarPublishLock = 0U;                                              //防止主循环复制期间中断覆盖发布缓冲区
static volatile uint32_t LidarValidFrameCount = 0UL;                                       //累计CRC正确的LD-14数据帧数量
static volatile uint32_t LidarCrcErrorCount = 0UL;                                         //累计CRC错误的LD-14候选帧数量

/**
  * 函    数：计算单字节滚动CRC8
  * 参    数：CurrentCrc 当前CRC值，Data 新输入字节
  * 返 回 值：加入新字节后的CRC8值
  */
static uint8_t Lidar_Crc8Update(uint8_t CurrentCrc, uint8_t Data)
{
    uint8_t BitIndex;                                                                         //保存当前处理的数据位下标

    CurrentCrc ^= Data;                                                                       //将新字节异或进入当前CRC值
    for (BitIndex = 0U; BitIndex < 8U; BitIndex++)                                           //逐位执行八次CRC8多项式运算
    {
        if ((CurrentCrc & 0x80U) != 0U)                                                       //判断当前CRC最高位是否需要应用多项式
        {
            CurrentCrc = (uint8_t)((CurrentCrc << 1U) ^ LIDAR_CRC_POLYNOMIAL);               //左移并异或0x4D生成多项式
        }
        else                                                                                  //处理当前CRC最高位为零的情况
        {
            CurrentCrc = (uint8_t)(CurrentCrc << 1U);                                        //仅左移CRC并进入下一数据位
        }
    }

    return CurrentCrc;                                                                        //返回更新完成的CRC8值
}

/**
  * 函    数：重新同步LD-14帧接收状态
  * 参    数：Data 当前收到且可能为下一帧头的字节
  * 返 回 值：无
  */
static void Lidar_Resynchronize(uint8_t Data)
{
    LidarReceiveIndex = 0U;                                                                   //清除当前候选帧的接收位置
    LidarReceiveCrc = 0U;                                                                     //清除当前候选帧的CRC累计值
    if (Data == LIDAR_FRAME_HEADER)                                                           //判断当前字节能否直接作为下一候选帧帧头
    {
        LidarReceiveRaw[0] = Data;                                                            //保存重新同步得到的帧头字节
        LidarReceiveCrc = Lidar_Crc8Update(0U, Data);                                        //从帧头重新开始累计CRC8
        LidarReceiveIndex = 1U;                                                               //等待接收固定类型长度字节
    }
}

/**
  * 函    数：解析一个CRC正确的LD-14原始帧
  * 参    数：RawFrame 四十七字节原始帧，Frame 解析结果输出地址
  * 返 回 值：无
  */
static void Lidar_ParseRawFrame(const uint8_t *RawFrame, LidarFrameTypeDef *Frame)
{
    uint8_t PointIndex;                                                                       //保存当前解析的测量点下标
    uint8_t RawIndex;                                                                         //保存当前测量点在原始帧中的起始下标
    uint32_t ExtendedEndAngle;                                                                //保存处理跨零度情况后的结束角度
    uint32_t AngleSpan;                                                                       //保存一帧首末测量点之间的角度跨度
    uint32_t PointAngle;                                                                      //保存当前测量点放大一百倍后的插值角度

    /*解析帧级字段*/
    Frame->SpeedDegreePerSecond = (uint16_t)((uint16_t)RawFrame[2] | ((uint16_t)RawFrame[3] << 8U)); //解析小端格式雷达转速
    Frame->StartAngleHundredth = (uint16_t)((uint16_t)RawFrame[4] | ((uint16_t)RawFrame[5] << 8U));  //解析小端格式起始角度
    Frame->EndAngleHundredth = (uint16_t)((uint16_t)RawFrame[42] | ((uint16_t)RawFrame[43] << 8U));  //解析小端格式结束角度
    Frame->TimestampMillisecond = (uint16_t)((uint16_t)RawFrame[44] | ((uint16_t)RawFrame[45] << 8U));//解析小端格式毫秒时间戳
    Frame->Crc8 = RawFrame[46];                                                               //保存原始帧携带的CRC8校验值

    /*计算十二点角度范围*/
    ExtendedEndAngle = Frame->EndAngleHundredth;                                              //默认使用原始结束角度参与插值
    if (ExtendedEndAngle < Frame->StartAngleHundredth)                                       //判断数据帧角度范围是否跨越零度
    {
        ExtendedEndAngle += LIDAR_FULL_CIRCLE;                                                //跨零度时将结束角度扩展到下一圆周
    }
    AngleSpan = ExtendedEndAngle - Frame->StartAngleHundredth;                                //计算首末测量点之间的总角度跨度

    /*解析十二个测量点*/
    for (PointIndex = 0U; PointIndex < LIDAR_POINT_COUNT; PointIndex++)                       //依次解析一帧中的十二个测量点
    {
        RawIndex = (uint8_t)(6U + PointIndex * 3U);                                           //计算当前测量点距离低字节所在位置
        Frame->Point[PointIndex].DistanceMillimeter = (uint16_t)((uint16_t)RawFrame[RawIndex] | ((uint16_t)RawFrame[RawIndex + 1U] << 8U)); //解析毫米距离
        Frame->Point[PointIndex].Confidence = RawFrame[RawIndex + 2U];                        //解析当前测量点置信度
        PointAngle = (uint32_t)Frame->StartAngleHundredth + (AngleSpan * PointIndex) / LIDAR_ANGLE_INTERVALS; //在首末端点之间线性插值角度
        if (PointAngle >= LIDAR_FULL_CIRCLE)                                                  //判断插值角度是否越过完整圆周
        {
            PointAngle -= LIDAR_FULL_CIRCLE;                                                  //将跨越圆周的角度归一化到零至359.99度
        }
        Frame->Point[PointIndex].AngleHundredth = (uint16_t)PointAngle;                       //保存当前测量点归一化角度
    }
}

/**
  * 函    数：USART1激光雷达接口初始化
  * 参    数：无
  * 返 回 值：无
  */
void Lidar_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                     //定义GPIO初始化结构体
    USART_InitTypeDef USART_InitStructure;                                                   //定义USART初始化结构体
    NVIC_InitTypeDef NVIC_InitStructure;                                                     //定义NVIC初始化结构体

    /*开启USART1与GPIOA时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);             //开启USART1和对应GPIOA端口时钟

    /*配置USART1发送引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;                                          //配置PA9为复用推挽输出模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;                                                 //选择USART1_TX对应的PA9引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                        //设置USART1发送引脚速度为50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                                                    //将USART1发送引脚配置写入GPIOA

    /*配置USART1接收引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;                                    //配置PA10为雷达数据浮空输入模式
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;                                                //选择USART1_RX对应的PA10引脚
    GPIO_Init(GPIOA, &GPIO_InitStructure);                                                    //将USART1接收引脚配置写入GPIOA

    /*配置230400-8-N-1通信参数*/
    USART_InitStructure.USART_BaudRate = 230400U;                                             //设置USART1雷达接收波特率为230400bit/s
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;            //关闭USART1硬件流量控制
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                           //同时使能USART1接收和发送能力
    USART_InitStructure.USART_Parity = USART_Parity_No;                                       //配置USART1为无奇偶校验
    USART_InitStructure.USART_StopBits = USART_StopBits_1;                                    //配置USART1为一个停止位
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;                               //配置USART1为八位数据长度
    USART_Init(USART1, &USART_InitStructure);                                                  //将雷达串口参数写入USART1寄存器

    /*配置最高优先级接收中断*/
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);                                           //使用两位抢占和两位响应优先级分组
    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;                                         //选择USART1全局中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                                           //使能USART1中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0U;                                //设置雷达接收抢占优先级为最高级零
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0U;                                       //设置雷达接收响应优先级为最高级零
    NVIC_Init(&NVIC_InitStructure);                                                            //将USART1中断参数写入NVIC
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);                                             //开启USART1接收数据寄存器非空中断
    USART_Cmd(USART1, ENABLE);                                                                 //使能USART1并开始接收雷达数据
}

/**
  * 函    数：取得并解析最近一个有效LD-14数据帧
  * 参    数：Frame 解析结果地址，RawFrame 原始帧输出地址
  * 返 回 值：1表示取得新帧，0表示当前没有新帧
  */
uint8_t Lidar_GetLatestFrame(LidarFrameTypeDef *Frame, uint8_t *RawFrame)
{
    uint8_t ByteIndex;                                                                         //保存复制原始帧时的字节下标
    uint8_t HasFrame = 0U;                                                                     //默认当前没有可读取的新帧

    LidarPublishLock = 1U;                                                                     //阻止USART1中断在复制期间发布另一帧
    if (LidarFrameReady != 0U)                                                                 //判断发布缓冲区是否包含尚未读取的新帧
    {
        for (ByteIndex = 0U; ByteIndex < LIDAR_FRAME_SIZE; ByteIndex++)                       //依次复制完整四十七字节原始帧
        {
            RawFrame[ByteIndex] = LidarLatestRaw[ByteIndex];                                  //将发布缓冲区当前字节复制到主循环缓冲区
        }
        LidarFrameReady = 0U;                                                                  //标记当前发布帧已被主循环取走
        HasFrame = 1U;                                                                         //记录本次成功取得一个新帧
    }
    LidarPublishLock = 0U;                                                                     //允许USART1中断继续发布后续有效帧

    if (HasFrame != 0U)                                                                        //判断是否需要解析刚取得的原始帧
    {
        Lidar_ParseRawFrame(RawFrame, Frame);                                                  //将原始字节解析为帧级和十二点测量数据
    }

    return HasFrame;                                                                           //返回本次是否取得新帧
}

/**
  * 函    数：读取有效帧累计数量
  * 参    数：无
  * 返 回 值：CRC正确的累计帧数量
  */
uint32_t Lidar_GetValidFrameCount(void)
{
    return LidarValidFrameCount;                                                               //返回USART1中断累计的有效帧数量
}

/**
  * 函    数：读取CRC错误帧累计数量
  * 参    数：无
  * 返 回 值：CRC错误的累计帧数量
  */
uint32_t Lidar_GetCrcErrorCount(void)
{
    return LidarCrcErrorCount;                                                                 //返回USART1中断累计的CRC错误帧数量
}

/**
  * 函    数：通过USART2打印LD-14原始帧和解析结果
  * 参    数：Frame 解析帧，RawFrame 原始帧，FrameNumber 有效帧累计编号
  * 返 回 值：无
  */
void Lidar_DebugPrint(const LidarFrameTypeDef *Frame, const uint8_t *RawFrame, uint32_t FrameNumber)
{
    uint8_t ByteIndex;                                                                         //保存当前打印的原始字节下标
    uint8_t PointIndex;                                                                        //保存当前打印的测量点下标
    uint16_t Angle;                                                                            //保存当前打印点放大一百倍后的角度

    /*打印四十七字节原始数据*/
    Serial_Printf("\r\n[LD14 RAW #%lu] ", FrameNumber);                                      //输出原始帧编号和数据标题
    for (ByteIndex = 0U; ByteIndex < LIDAR_FRAME_SIZE; ByteIndex++)                           //依次遍历完整原始帧的四十七个字节
    {
        Serial_Printf("%02X", RawFrame[ByteIndex]);                                           //以两位十六进制格式输出当前原始字节
        if (((ByteIndex + 1U) % 16U) == 0U)                                                   //判断是否已经打印十六个原始字节
        {
            Serial_SendString("\r\n                 ");                                       //换行并对齐继续打印下一组原始字节
        }
        else                                                                                  //处理当前行尚未达到十六字节的情况
        {
            Serial_SendByte(' ');                                                             //在相邻原始字节之间输出一个空格
        }
    }
    Serial_SendString("\r\n");                                                               //结束原始帧十六进制输出行

    /*打印帧级解析结果*/
    Serial_Printf("[LD14] speed=%u deg/s rpm=%u.%02u start=%u.%02u end=%u.%02u ts=%u crc=%02X\r\n", //输出帧级解析字段
                  Frame->SpeedDegreePerSecond,                                                //提供雷达每秒转动角度参数
                  Frame->SpeedDegreePerSecond / 6U,                                          //将每秒角度整数部分换算为每分钟转数
                  (uint16_t)(((Frame->SpeedDegreePerSecond % 6U) * 100U) / 6U),               //计算每分钟转数两位小数部分
                  Frame->StartAngleHundredth / 100U,                                         //提供起始角度整数部分
                  Frame->StartAngleHundredth % 100U,                                         //提供起始角度两位小数部分
                  Frame->EndAngleHundredth / 100U,                                           //提供结束角度整数部分
                  Frame->EndAngleHundredth % 100U,                                           //提供结束角度两位小数部分
                  Frame->TimestampMillisecond,                                                //提供雷达毫秒时间戳
                  Frame->Crc8);                                                               //提供通过验证的CRC8值

    /*打印十二个测量点*/
    for (PointIndex = 0U; PointIndex < LIDAR_POINT_COUNT; PointIndex++)                       //依次遍历一帧中的十二个解析点
    {
        Angle = Frame->Point[PointIndex].AngleHundredth;                                      //读取当前测量点放大一百倍后的角度
        Serial_Printf("  P%02u angle=%3u.%02u deg distance=%5u mm confidence=%3u\r\n",        //输出当前测量点完整解析结果
                      PointIndex,                                                             //提供当前测量点下标
                      Angle / 100U,                                                          //提供当前测量点角度整数部分
                      Angle % 100U,                                                          //提供当前测量点角度两位小数部分
                      Frame->Point[PointIndex].DistanceMillimeter,                            //提供当前测量点毫米距离
                      Frame->Point[PointIndex].Confidence);                                   //提供当前测量点信号置信度
    }
}

/**
  * 函    数：USART1激光雷达接收中断服务
  * 参    数：无
  * 返 回 值：无
  */
void USART1_IRQHandler(void)
{
    uint8_t Data;                                                                             //保存本次USART1中断接收到的雷达字节
    uint8_t ByteIndex;                                                                        //保存发布有效帧时的字节下标

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)                                    //判断USART1是否产生接收寄存器非空中断
    {
        Data = (uint8_t)USART_ReceiveData(USART1);                                             //读取雷达字节并自动清除RXNE硬件状态

        if (LidarReceiveIndex == 0U)                                                           //判断当前是否正在等待固定帧头
        {
            if (Data == LIDAR_FRAME_HEADER)                                                   //判断当前字节是否为0x54固定帧头
            {
                LidarReceiveRaw[0] = Data;                                                    //保存候选帧的帧头字节
                LidarReceiveCrc = Lidar_Crc8Update(0U, Data);                                 //从帧头开始累计CRC8
                LidarReceiveIndex = 1U;                                                       //进入固定类型长度字节接收状态
            }
        }
        else if (LidarReceiveIndex == 1U)                                                      //判断当前是否等待0x2C类型长度字节
        {
            if (Data == LIDAR_FRAME_VER_LEN)                                                  //判断当前字节是否匹配十二点数据帧类型
            {
                LidarReceiveRaw[1] = Data;                                                    //保存固定类型长度字节
                LidarReceiveCrc = Lidar_Crc8Update(LidarReceiveCrc, Data);                    //将类型长度字节加入CRC8
                LidarReceiveIndex = 2U;                                                       //进入剩余帧数据接收状态
            }
            else                                                                               //处理第二字节不是固定类型长度的情况
            {
                Lidar_Resynchronize(Data);                                                    //尝试把当前字节作为下一候选帧帧头
            }
        }
        else if (LidarReceiveIndex < (LIDAR_FRAME_SIZE - 1U))                                 //判断当前是否正在接收CRC之前的帧内容
        {
            LidarReceiveRaw[LidarReceiveIndex] = Data;                                       //保存当前帧内容字节
            LidarReceiveCrc = Lidar_Crc8Update(LidarReceiveCrc, Data);                        //将当前内容字节加入CRC8累计
            LidarReceiveIndex++;                                                              //移动到候选帧的下一个接收位置
        }
        else                                                                                  //处理候选帧最后一个CRC8字节
        {
            LidarReceiveRaw[LIDAR_FRAME_SIZE - 1U] = Data;                                   //保存雷达数据帧携带的CRC8字节
            if (Data == LidarReceiveCrc)                                                      //比较雷达CRC8与本地累计结果
            {
                LidarValidFrameCount++;                                                       //累计一个通过CRC检查的有效数据帧
                if (LidarPublishLock == 0U)                                                   //判断主循环是否正在复制发布缓冲区
                {
                    for (ByteIndex = 0U; ByteIndex < LIDAR_FRAME_SIZE; ByteIndex++)           //依次复制完整有效原始帧
                    {
                        LidarLatestRaw[ByteIndex] = LidarReceiveRaw[ByteIndex];                //把当前有效帧发布给主循环
                    }
                    LidarFrameReady = 1U;                                                     //通知主循环存在一个新的有效帧
                }
            }
            else                                                                               //处理候选帧CRC8校验失败的情况
            {
                LidarCrcErrorCount++;                                                         //累计一个CRC错误候选帧
            }
            LidarReceiveIndex = 0U;                                                           //复位接收位置并等待下一固定帧头
            LidarReceiveCrc = 0U;                                                             //清除CRC累计值并准备下一数据帧
        }
    }
}
