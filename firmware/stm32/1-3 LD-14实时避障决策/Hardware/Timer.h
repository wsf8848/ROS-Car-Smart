#ifndef __TIMER_H                       //防止定时器头文件被重复包含
#define __TIMER_H                       //定义定时器头文件包含保护宏

#include "stm32f10x.h"                 //提供毫秒计时接口使用的固定宽度整数类型

void Timer_Init(void);                  //初始化并启动TIM2一毫秒系统运行时基
uint32_t Timer_GetMs(void);             //读取系统上电后累计运行的毫秒数
uint8_t Timer_TakeBluetooth10msFlag(void);//取得并清除TIM2产生的蓝牙十毫秒发送请求

#endif                                  //结束定时器头文件包含保护
