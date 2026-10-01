/**
 * @user    55480（伍圣锋）
 * @核心功能 USART1接收解析LD-14原始帧，USART2限速打印调试数据并通过OLED显示雷达状态
 * @时间    2026-07-26
 * @版本    1.0
 * @修改历史 V1.0 基于1-0板级工程新增LD-14双串口解析与OLED状态显示
 */

#include "stm32f10x.h"                  //提供STM32F103RCT6寄存器与标准外设库定义
#include "LED.h"                        //提供板载红绿蓝LED状态指示接口
#include "OLED.h"                       //提供OLED初始化和雷达状态显示接口
#include "Buzzer.h"                     //提供系统启动提示音接口
#include "Serial.h"                     //提供USART2专用调试输出接口
#include "Lidar.h"                      //提供USART1 LD-14接收解析和调试打印接口

#define LIDAR_DEBUG_FRAME_DIVIDER 20UL   //定义每二十个有效帧输出一次USART2和OLED调试信息

int main(void)                           //程序从主函数开始执行
{                                       //进入主函数作用域
    LidarFrameTypeDef LidarFrame;        //保存最近一个LD-14数据帧的解析结果
    uint8_t LidarRawFrame[LIDAR_FRAME_SIZE]; //保存最近一个LD-14数据帧的四十七字节原始数据
    uint32_t ValidFrameCount = 0UL;      //保存当前CRC正确数据帧累计数量
    uint32_t CrcErrorCount = 0UL;        //保存当前CRC错误候选帧累计数量
    uint32_t LastDebugFrameCount = 0UL;  //保存上次输出调试信息时的有效帧编号

    /* ==================== 板载外设与双串口初始化 ==================== */
    OLED_Init();                         //初始化PA5和PA4软件I2C OLED显示屏
    LED_Init();                          //初始化PC3、PB12和PB13板载三色LED
    Buzzer_Init();                       //初始化PB14高电平驱动蜂鸣器
    Serial_Init();                       //初始化PA2和PA3对应USART2为115200专用调试口
    Lidar_Init();                        //初始化PA9和PA10对应USART1为230400雷达数据口

    /* ==================== 上电状态提示 ==================== */
    LED_R_ON();                          //点亮红色LED表示正在等待LD-14有效数据帧
    OLED_ShowString(1U, 1U, "LD14 U1 230400"); //显示LD-14接收串口和波特率
    OLED_ShowString(2U, 1U, "OK:00000 ER:000"); //显示有效帧与CRC错误帧计数占位符
    OLED_ShowString(3U, 1U, "S:0000 A:000.00"); //显示转速与起始角度占位符
    OLED_ShowString(4U, 1U, "D0:0000 Q:000");   //显示第一个点距离与置信度占位符
    Serial_SendString("\r\n[BOOT] LD-14 receiver started\r\n"); //通过USART2输出系统启动标识
    Serial_SendString("[UART1] LD-14 RX=PA10 baud=230400\r\n"); //通过USART2输出雷达串口配置
    Serial_SendString("[UART2] DEBUG TX=PA2 baud=115200\r\n"); //通过USART2输出专用调试串口配置
    Buzzer_Start();                      //输出两声短鸣提示系统初始化完成

    /* ==================== 雷达数据处理主循环 ==================== */
    while (1)                            //持续获取雷达帧并更新USART2与OLED调试信息
    {                                   //进入雷达数据处理主循环作用域
        if (Lidar_GetLatestFrame(&LidarFrame, LidarRawFrame) != 0U) //判断USART1是否发布新的CRC正确数据帧
        {                               //进入有效雷达帧处理分支
            ValidFrameCount = Lidar_GetValidFrameCount(); //读取中断累计的CRC正确帧数量
            CrcErrorCount = Lidar_GetCrcErrorCount(); //读取中断累计的CRC错误候选帧数量

            if (LastDebugFrameCount == 0UL) //判断当前是否为上电后的第一个有效数据帧
            {                           //进入首次有效数据提示分支
                LED_R_OFF();             //熄灭红色LED表示已经收到有效LD-14数据
                LED_G_ON();              //点亮绿色LED表示雷达接收与CRC检查正常
            }                           //结束首次有效数据提示分支

            if ((ValidFrameCount - LastDebugFrameCount) >= LIDAR_DEBUG_FRAME_DIVIDER || LastDebugFrameCount == 0UL) //判断是否达到限速调试输出周期
            {                           //进入USART2和OLED调试信息更新分支
                LastDebugFrameCount = ValidFrameCount; //记录本次已输出的有效帧编号

                /* ==================== USART2专用调试输出 ==================== */
                Lidar_DebugPrint(&LidarFrame, LidarRawFrame, ValidFrameCount); //打印四十七字节原始帧和十二点解析结果
                Serial_Printf("[LD14 STAT] valid=%lu crc_error=%lu\r\n", ValidFrameCount, CrcErrorCount); //打印累计接收统计信息

                /* ==================== OLED同步状态显示 ==================== */
                OLED_ShowNum(2U, 4U, ValidFrameCount % 100000UL, 5U); //更新OLED有效帧计数低五位
                OLED_ShowNum(2U, 13U, CrcErrorCount % 1000UL, 3U); //更新OLED CRC错误帧计数低三位
                OLED_ShowNum(3U, 3U, LidarFrame.SpeedDegreePerSecond % 10000U, 4U); //更新OLED雷达每秒转动角度
                OLED_ShowNum(3U, 10U, LidarFrame.StartAngleHundredth / 100U, 3U); //更新OLED起始角度整数部分
                OLED_ShowNum(3U, 14U, LidarFrame.StartAngleHundredth % 100U, 2U); //更新OLED起始角度小数部分
                OLED_ShowNum(4U, 4U, LidarFrame.Point[0].DistanceMillimeter % 10000U, 4U); //更新OLED第一个点毫米距离
                OLED_ShowNum(4U, 11U, LidarFrame.Point[0].Confidence, 3U); //更新OLED第一个点置信度
            }                           //结束USART2和OLED调试信息更新分支
        }                               //结束有效雷达帧处理分支
    }                                   //结束雷达数据处理主循环作用域
}                                       //结束主函数作用域
