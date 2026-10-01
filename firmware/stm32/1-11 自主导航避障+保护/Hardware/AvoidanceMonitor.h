#ifndef __AVOIDANCE_MONITOR_H                   //防止避障观测适配头文件被重复包含
#define __AVOIDANCE_MONITOR_H                   //定义避障观测适配头文件包含保护宏

#include "stm32f10x.h"                         //提供板级显示与串口适配使用的固定宽度整数类型
#include "Avoidance.h"                         //提供避障状态快照类型和状态名称接口
#include "MotionCommand.h"                     //提供标准化运动指令类型

void AvoidanceMonitor_Init(void);               //初始化OLED避障状态显示占位内容
void AvoidanceMonitor_ShowOLED(const AvoidanceStatusTypeDef *Status, const MotionCommandTypeDef *Command);//通过OLED显示避障状态与四向厘米距离
void AvoidanceMonitor_PrintSerial(const AvoidanceStatusTypeDef *Status, const MotionCommandTypeDef *Command, uint32_t ValidFrameCount, uint32_t CrcErrorCount);//通过USART2打印避障状态与四向厘米距离
void AvoidanceMonitor_UpdateAlerts(const AvoidanceStatusTypeDef *Status, uint32_t NowMs);//根据四向厘米距离更新板载LED和无阻塞蜂鸣器

#endif                                         //结束避障观测适配头文件包含保护
