#ifndef __BATTERY_H__                              // 防止电池电量接口头文件被重复包含
#define __BATTERY_H__                              // 定义电池电量接口头文件保护宏

#include "stm32f10x.h"                            // 提供STM32F1固定宽度整数类型和ADC外设定义

#define BATTERY_ADC_CHANNEL ADC_Channel_14         // 定义电池分压采样使用ADC1通道14，对应PC4
#define BATTERY_ADC_GPIO_PORT GPIOC                // 定义电池分压采样GPIO端口为GPIOC
#define BATTERY_ADC_GPIO_PIN GPIO_Pin_4            // 定义电池分压采样GPIO引脚为PC4
#define BATTERY_ADC_REF_MV 3300U                   // 定义ADC参考电压为3300毫伏
#define BATTERY_ADC_RESOLUTION 4096U               // 定义12位ADC满量程计数为4096
#define BATTERY_DIVIDER_RATIO_NUM 111U             // 定义分压换算倍率分子11.1的整数表示
#define BATTERY_DIVIDER_RATIO_DEN 10U              // 定义分压换算倍率分母10
#define BATTERY_EMPTY_MV 10500U                    // 定义3S电池0%对应电压为10500毫伏
#define BATTERY_FULL_MV 12600U                     // 定义3S电池100%对应电压为12600毫伏
#define BATTERY_FILTER_SHIFT 2U                    // 定义电压一阶滤波右移量，约四分之一新采样权重

void Battery_Init(void);                           // 初始化PC4模拟输入和ADC1电池采样
void Battery_Update(void);                          // 采样一次电池电压并更新滤波结果
uint16_t Battery_GetVoltageMv(void);                // 获取滤波后的电池电压，单位毫伏
uint8_t Battery_GetVoltageDeciV(void);              // 获取协议使用的电池电压，单位0.1伏
uint8_t Battery_GetPercent(void);                  // 获取按10500~12600毫伏换算的电量百分比

#endif                                             // 结束电池电量接口头文件保护
