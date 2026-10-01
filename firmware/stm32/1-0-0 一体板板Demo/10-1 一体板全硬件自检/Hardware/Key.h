#ifndef __KEY_H                         //防止按键头文件被重复包含
#define __KEY_H                         //定义按键头文件包含保护宏

#include "stm32f10x.h"                 //提供uint8_t基础类型定义

void Key_Init(void);                    //初始化PC0、PC1和PC2为内部上拉按键输入
uint8_t Key_GetNum(void);               //读取按键并返回0至3的单次消抖键值

#endif                                  //结束按键头文件包含保护
