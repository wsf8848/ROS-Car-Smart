/**
 * @user    55480（伍圣锋）
 * @核心功能 基于LD-14实时避障与四驱控制，通过蓝牙、ESP32和UART5 ROS三路串口完成控制与遥测
 * @时间    2026-09-16
 * @版本    2.9
 * @修改历史 V2.7 恢复避障与自主模式授权，增加远程连续双响鸣笛和失联自动静音
 *             V2.8 参考已验证工程完善八灯WS2812彩色流水、绿色闪烁和DMA复位时序
 *             V2.9 增加UART5鲁班猫ROS底盘协议链路（PC12发送、PD2接收）
 */

#include "stm32f10x.h"                  //提供STM32F103RCT6寄存器与标准外设库定义
#include "LED.h"                        //提供板载红绿蓝LED状态指示接口
#include "OLED.h"                       //提供OLED初始化接口
#include "Buzzer.h"                     //提供系统启动提示音接口
#include "Serial.h"                     //提供USART2专用调试输出接口
#include "Lidar.h"                      //提供USART1 LD-14接收与十二点解析接口
#include "Timer.h"                      //提供TIM6一毫秒系统运行时基和蜂鸣器调度
#include "Avoidance.h"                  //提供与硬件和电机驱动无关的实时避障决策核心
#include "AvoidanceConfig.h"            //提供避障决策与调试显示更新周期参数
#include "AvoidanceMonitor.h"           //提供OLED、USART2和LED避障状态观测适配接口
#include "Bluetooth.h"                  //提供USART2蓝牙遥测数据包与手机控制命令接口
#include "Esp32Link.h"                  //提供UART4连接ESP32-C3的遥测复制和接收缓存接口
#include "WS2812.h"                     //提供PA11车灯动画刷新接口
#include "RosLink.h"                     //提供UART5鲁班猫ROS串口收发接口

int main(void)                          //定义系统主入口函数
{                                       
    LidarFrameTypeDef LidarFrame;        //保存最近一个LD-14数据帧的解析结果
    uint8_t LidarRawFrame[LIDAR_FRAME_SIZE]; //保存最近一个LD-14数据帧的四十七字节原始数据
    AvoidanceStatusTypeDef AvoidanceStatus; //保存当前实时避障状态快照
    MotionCommandTypeDef MotionCommand;  //保存等待后续电机适配层执行的标准化运动指令
    uint8_t PointIndex = 0U;             //保存当前输入避障核心的雷达测量点下标
    uint16_t DistanceCm = 0U;            //保存从毫米四舍五入换算得到的厘米距离
    uint32_t CurrentTimeMs = 0UL;        //保存主循环当前读取的系统毫秒时间
    uint32_t LastDecisionMs = 0UL;       //保存上一次执行避障决策的系统毫秒时间
    uint32_t LastOledMs = 0UL;           //保存上一次刷新OLED避障信息的系统毫秒时间

    /* ==================== 板载外设与避障模块初始化 ==================== */
    OLED_Init();                         //初始化PA5和PA4软件I2C OLED显示屏
    LED_Init();                          //初始化PC3、PB12和PB13板载三色LED
    Buzzer_Init();                       //初始化PB14高电平驱动蜂鸣器
    Serial_Init();                       //初始化PA2和PA3对应USART2为115200蓝牙通信口
    Esp32Link_Init();                    //初始化PC10和PC11对应UART4为115200 ESP32-C3通信口
    RosLink_Init();                      //初始化PC12和PD2对应UART5为115200鲁班猫ROS通信口
    Bluetooth_Init();                    //初始化蓝牙遥测协议与手机控制命令状态
    Timer_Init();                        //初始化TIM6为一毫秒系统时基并启动无阻塞蜂鸣器调度
    WS2812_Init();                       //初始化PA11、TIM1通道4和DMA1通道4驱动八颗WS2812灯珠
    Avoidance_Init();                    //初始化与电机驱动完全解耦的实时避障决策核心
    Avoidance_GetSnapshot(&AvoidanceStatus, &MotionCommand); //取得上电等待状态和安全停车初始指令
    AvoidanceMonitor_Init();             //初始化OLED四行避障状态与厘米距离界面
    Lidar_Init();                        //初始化PA9和PA10对应USART1为230400雷达数据口

    /* ==================== 上电状态提示 ==================== */
    LED_R_ON();                          //点亮红色LED表示正在等待完整三百六十度雷达数据
    Buzzer_Start();                      //输出两声短鸣提示系统初始化完成

    /* ==================== 实时避障处理主循环 ==================== */
    while (1)                            //持续输入雷达测量点并按固定周期更新避障决策
    {                                   //进入实时避障处理主循环作用域
        CurrentTimeMs = Timer_GetMs();   //读取当前TIM6毫秒时间并供全部无阻塞任务使用
        Bluetooth_ProcessReceivedData(); //统一解析USART2蓝牙和UART4 ESP32-C3下发的模式、运动、调速与停车命令
        WS2812_Task(CurrentTimeMs);       //使用本轮统一毫秒快照低频刷新八灯流水或绿色闪烁效果
        if (Lidar_GetLatestFrame(&LidarFrame, LidarRawFrame) != 0U) //判断USART1是否发布新的CRC正确数据帧
        {                               //进入有效雷达帧处理分支
            for (PointIndex = 0U; PointIndex < LIDAR_POINT_COUNT; PointIndex++) //依次向避障核心输入当前帧十二个测量点
            {                           //进入单个雷达测量点处理作用域
                DistanceCm = (uint16_t)((LidarFrame.Point[PointIndex].DistanceMillimeter + 5U) / 10U); //把毫米距离四舍五入转换为厘米
                Avoidance_InputPoint(LidarFrame.Point[PointIndex].AngleHundredth, DistanceCm, LidarFrame.Point[PointIndex].Confidence, CurrentTimeMs); //输入与硬件无关的角度厘米距离和置信度
            }                           //结束当前完整雷达帧十二点输入过程
            Avoidance_NotifyFrame(CurrentTimeMs); //通知避障核心当前雷达链路收到一个完整有效帧
        }                               //结束有效雷达帧处理分支

        if ((uint32_t)(CurrentTimeMs - LastDecisionMs) >= AVOIDANCE_DECISION_PERIOD_MS) //判断是否达到五毫秒避障决策周期
        {                               //进入实时避障状态机更新分支
            LastDecisionMs = CurrentTimeMs; //记录本次避障决策执行时间
            Avoidance_Update(CurrentTimeMs); //根据最新三百六十度距离图更新避障状态和运动指令
            Avoidance_GetSnapshot(&AvoidanceStatus, &MotionCommand); //取得供后续电机、OLED和串口使用的一致状态快照
            AvoidanceMonitor_UpdateAlerts(&AvoidanceStatus, CurrentTimeMs); //根据四向厘米距离更新LED和无阻塞蜂鸣器提示
        }                               //结束实时避障状态机更新分支

        if ((uint32_t)(CurrentTimeMs - LastOledMs) >= AVOIDANCE_OLED_PERIOD_MS) //判断是否达到一百毫秒OLED刷新周期
        {                               //进入OLED避障信息刷新分支
            LastOledMs = CurrentTimeMs; //记录本次OLED刷新时间
            AvoidanceMonitor_ShowOLED(&AvoidanceStatus, &MotionCommand); //显示避障状态、四向厘米距离和标准化运动指令
        }                               //结束OLED避障信息刷新分支

        if (Timer_TakeBluetooth10msFlag() != 0U) //判断TIM6是否产生蓝牙十毫秒发送请求
        {                               //进入蓝牙实时遥测数据包发送分支
            Bluetooth_SendTelemetry(&AvoidanceStatus); //发送双包头四向距离雷达状态小车状态和包尾
            Bluetooth_SendChassisTelemetry(); //发送双包头四电机原始速度陀螺仪角度和包尾
        }                               //结束蓝牙实时遥测数据包发送分支
    }                                   //结束实时避障处理主循环作用域
}                                       //结束主函数作用域
