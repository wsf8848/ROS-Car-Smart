#ifndef __LIDAR_H                       //防止激光雷达头文件被重复包含
#define __LIDAR_H                       //定义激光雷达头文件包含保护宏

#include "stm32f10x.h"                 //提供激光雷达接口使用的固定宽度整数类型

#define LIDAR_POINT_COUNT 12U           //定义LD-14每个数据帧包含十二个测量点
#define LIDAR_FRAME_SIZE 47U            //定义LD-14固定数据帧长度为四十七字节

typedef struct                          //定义单个激光雷达测量点数据结构
{                                       //开始单个测量点结构体定义
    uint16_t AngleHundredth;             //保存放大一百倍后的测量角度
    uint16_t DistanceMillimeter;         //保存以毫米为单位的测量距离
    uint8_t Confidence;                  //保存当前测量点的信号置信度
} LidarPointTypeDef;                    //结束单个测量点结构体类型定义

typedef struct                          //定义一个完整LD-14解析帧数据结构
{                                       //开始完整解析帧结构体定义
    uint16_t SpeedDegreePerSecond;       //保存以度每秒为单位的雷达转速
    uint16_t StartAngleHundredth;        //保存放大一百倍后的帧起始角度
    uint16_t EndAngleHundredth;          //保存放大一百倍后的帧结束角度
    LidarPointTypeDef Point[LIDAR_POINT_COUNT];//保存一帧十二个角度距离和置信度测量点
    uint16_t TimestampMillisecond;       //保存雷达提供的毫秒时间戳
    uint8_t Crc8;                        //保存数据帧携带的CRC8校验值
} LidarFrameTypeDef;                    //结束完整解析帧结构体类型定义

void Lidar_Init(void);                  //初始化PC12发送和PD2接收的UART5为230400-8-N-1
uint8_t Lidar_GetLatestFrame(LidarFrameTypeDef *Frame, uint8_t *RawFrame);//取得并解析最近一个CRC正确的LD-14原始帧
uint32_t Lidar_GetValidFrameCount(void);//读取UART5累计接收的CRC正确帧数量
uint32_t Lidar_GetCrcErrorCount(void);  //读取UART5累计检测到的CRC错误帧数量
void Lidar_DebugPrint(const LidarFrameTypeDef *Frame, const uint8_t *RawFrame, uint32_t FrameNumber);//通过USART2打印原始帧和十二点解析结果

#endif                                  //结束激光雷达头文件包含保护
