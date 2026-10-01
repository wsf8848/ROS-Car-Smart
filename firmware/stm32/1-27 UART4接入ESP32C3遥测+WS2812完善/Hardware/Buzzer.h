#ifndef __BUZZER_H                      //防止蜂鸣器头文件被重复包含
#define __BUZZER_H                      //定义蜂鸣器头文件包含保护宏

#include "stm32f10x.h"                 //提供uint16_t基础类型定义

void Buzzer_Init(void);                 //初始化PB14为高电平开启的蜂鸣器推挽输出
void Buzzer_ON(void);                   //控制PB14高电平并开启蜂鸣器
void Buzzer_OFF(void);                  //控制PB14低电平并关闭蜂鸣器
void Buzzer_Turn(void);                 //翻转PB14蜂鸣器当前状态
void Buzzer_Flash(uint16_t PeriodTicks);//按调用次数控制PB14蜂鸣器周期闪鸣
void Buzzer_Start(void);                //启动两声一百毫秒的无阻塞上电提示音
void Buzzer_start(void);                //兼容旧工程命名并启动无阻塞上电提示音
void Buzzer_SetObstacleAlert(uint8_t FrontBlocked, uint8_t RearBlocked);//根据前后障碍物请求单响或循环双响提示
void Buzzer_SetRemoteHorn(uint8_t Enabled); //设置远程连续双响鸣笛状态并优先于障碍物提示
void Buzzer_Tick1ms(void);              //由TIM6每一毫秒调用并推进蜂鸣器提示状态机

#endif                                  //结束蜂鸣器头文件包含保护
