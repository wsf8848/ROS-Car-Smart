#ifndef __LED_H                         //防止LED头文件被重复包含
#define __LED_H                         //定义LED头文件包含保护宏

#include "stm32f10x.h"                 //提供uint16_t等基础类型定义

void LED_Init(void);                    //初始化PC3红灯、PB12绿灯和PB13蓝灯为低电平点亮的推挽输出
void LED_R_ON(void);                    //点亮PC3红色LED
void LED_R_OFF(void);                   //熄灭PC3红色LED
void LED_R_Turn(void);                  //翻转PC3红色LED状态
void LED_G_ON(void);                    //点亮PB12绿色LED
void LED_G_OFF(void);                   //熄灭PB12绿色LED
void LED_G_Turn(void);                  //翻转PB12绿色LED状态
void LED_B_ON(void);                    //点亮PB13蓝色LED
void LED_B_OFF(void);                   //熄灭PB13蓝色LED
void LED_B_Turn(void);                  //翻转PB13蓝色LED状态
void LED_ON(void);                      //兼容旧接口并点亮PC3红色LED
void LED_OFF(void);                     //兼容旧接口并熄灭PC3红色LED
void LED_Turn(void);                    //兼容旧接口并翻转PC3红色LED
void Led_Flash(uint16_t PeriodTicks);   //按调用次数控制PC3红色LED周期闪烁
void LED_Second_Flash(void);            //按10ms调用周期产生红色LED一秒双翻转效果

#endif                                  //结束LED头文件包含保护
