#ifndef __ENCODER_H__                                  // 防止编码器头文件被重复包含
#define __ENCODER_H__                                  // 定义编码器头文件包含保护宏

#include "stm32f10x.h"                                // 提供STM32F1标准外设类型定义

void EncoderA_Init(void);                              // 初始化左后轮TIM3编码器接口
int16_t EncoderA_Get(void);                             // 读取并清零左后轮编码器增量
void EncoderB_Init(void);                              // 初始化左前轮TIM2编码器接口
int16_t EncoderB_Get(void);                             // 读取并清零左前轮编码器增量
void EncoderC_Init(void);                              // 初始化右前轮TIM4编码器接口
int16_t EncoderC_Get(void);                             // 读取并清零右前轮编码器增量
void EncoderD_Init(void);                              // 初始化右后轮TIM5编码器接口
int16_t EncoderD_Get(void);                             // 读取并清零右后轮编码器增量

#endif                                                // 结束编码器头文件包含保护
