#ifndef __AVOIDANCE_H                            //防止避障决策头文件被重复包含
#define __AVOIDANCE_H                            //定义避障决策头文件包含保护宏

#include <stdint.h>                              //提供避障接口使用的固定宽度整数类型
#include "MotionCommand.h"                       //提供与具体电机无关的标准化运动指令类型

typedef enum                                     //定义实时避障状态机的全部运行状态
{                                                //开始避障状态枚举定义
    AVOIDANCE_STATE_WAIT = 0,                    //表示正在等待足够的三百六十度雷达数据
    AVOIDANCE_STATE_CRUISE,                      //表示前方安全并输出正常巡航指令
    AVOIDANCE_STATE_SLOW,                        //表示前方接近障碍物并降低前进速度
    AVOIDANCE_STATE_TURN_LEFT,                   //表示停止前进并向左侧空闲区域转向
    AVOIDANCE_STATE_TURN_RIGHT,                  //表示停止前进并向右侧空闲区域转向
    AVOIDANCE_STATE_ESCAPE,                      //表示左右均受阻并选择较大空间原地脱困
    AVOIDANCE_STATE_EMERGENCY_STOP,              //表示障碍物过近并要求电机立即停车
    AVOIDANCE_STATE_SENSOR_FAULT                 //表示雷达数据超时并要求故障停车
} AvoidanceStateTypeDef;                         //结束实时避障状态枚举类型定义

typedef struct                                   //定义供显示和电机适配层读取的避障状态快照
{                                                //开始避障状态快照结构体定义
    AvoidanceStateTypeDef State;                 //保存当前避障状态机状态
    uint16_t FrontDistanceCm;                    //保存前方扇区最近障碍物距离，单位厘米
    uint16_t LeftDistanceCm;                     //保存左侧扇区最近障碍物距离，单位厘米
    uint16_t RightDistanceCm;                    //保存右侧扇区最近障碍物距离，单位厘米
    uint16_t RearDistanceCm;                     //保存后方扇区最近障碍物距离，单位厘米
    uint16_t FreshBinCount;                      //保存当前仍然有效的角度栅格数量
    uint32_t DataAgeMs;                          //保存最近有效雷达帧距离当前时刻的毫秒数
} AvoidanceStatusTypeDef;                        //结束避障状态快照结构体类型定义

void Avoidance_Init(void);                       //初始化与硬件无关的避障决策核心
void Avoidance_InputPoint(uint16_t AngleHundredth, uint16_t DistanceCm, uint8_t Confidence, uint32_t NowMs);//输入一个角度距离和置信度测量点，距离单位厘米
void Avoidance_NotifyFrame(uint32_t NowMs);       //通知避障核心已经收到一个CRC正确的完整雷达帧
void Avoidance_Update(uint32_t NowMs);            //根据最新三百六十度距离图更新避障状态和运动指令
void Avoidance_GetSnapshot(AvoidanceStatusTypeDef *Status, MotionCommandTypeDef *Command);//复制当前避障状态和标准化运动指令
const char *Avoidance_GetStateText(AvoidanceStateTypeDef State);//取得适合OLED和USART2显示的避障状态英文短名称

#endif                                           //结束避障决策头文件包含保护
