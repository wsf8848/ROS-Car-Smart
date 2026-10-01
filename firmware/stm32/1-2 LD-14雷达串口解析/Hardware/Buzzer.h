#ifndef __BUZZER_H                      //防止蜂鸣器头文件被重复包含
#define __BUZZER_H                      //定义蜂鸣器头文件包含保护宏

#include "stm32f10x.h"                 //提供uint16_t基础类型定义

void Buzzer_Init(void);                 //初始化PB14为高电平开启的蜂鸣器推挽输出
void Buzzer_ON(void);                   //控制PB14高电平并开启蜂鸣器
void Buzzer_OFF(void);                  //控制PB14低电平并关闭蜂鸣器
void Buzzer_Turn(void);                 //翻转PB14蜂鸣器当前状态
void Buzzer_Flash(uint16_t PeriodTicks);//按调用次数控制PB14蜂鸣器周期闪鸣
void Buzzer_Start(void);                //输出两声100ms的上电启动提示音
void Buzzer_start(void);                //兼容旧工程命名并输出启动提示音

#endif                                  //结束蜂鸣器头文件包含保护
