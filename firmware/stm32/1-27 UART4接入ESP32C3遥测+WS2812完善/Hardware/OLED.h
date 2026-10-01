#ifndef __OLED_H                        //防止OLED头文件被重复包含
#define __OLED_H                        //定义OLED头文件包含保护宏

#include "stm32f10x.h"                 //提供OLED接口所需的固定宽度整数类型

void OLED_Init(void);                   //初始化PA5时钟和PA4数据的软件I2C OLED
void OLED_Clear(void);                  //清空OLED全部128乘64像素
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Character);//在指定位置显示一个8乘16 ASCII字符
void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String);//在指定位置显示ASCII字符串
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);//显示固定长度无符号十进制数
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);//显示带符号十进制数
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);//显示固定长度十六进制数
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);//显示固定长度二进制数
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number, uint8_t Length, uint8_t FractionLength);//显示带符号浮点数
void OLED_ShowCN(uint8_t Line, uint8_t Column, uint8_t Index);//显示指定下标的20乘16中文字模

#endif                                  //结束OLED头文件包含保护
