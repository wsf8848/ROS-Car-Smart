#ifndef __SERIAL_H                      //防止串口头文件被重复包含
#define __SERIAL_H                      //定义串口头文件包含保护宏

#include "stm32f10x.h"                 //提供串口接口所需的固定宽度整数类型

extern uint8_t RxData;                  //声明USART2最近一次接收的数据并兼容原主函数变量名
extern volatile uint8_t Serial_RxFlag;  //声明USART2新数据到达标志变量

void Serial_Init(void);                 //初始化PA2发送和PA3接收的USART2为115200-8-N-1
void Serial_SendByte(uint8_t Byte);     //通过USART2发送一个字节
void Serial_SendArray(const uint8_t *Array, uint16_t Length);//通过USART2发送指定长度字节数组
void Serial_SendString(const char *String);//通过USART2发送以结束符结尾的字符串
void Serial_SendNumber(uint32_t Number, uint8_t Length);//通过USART2发送固定长度十进制数字
void Serial_Printf(const char *Format, ...);//通过USART2发送格式化字符串
uint8_t Serial_ReadByte(uint8_t *Byte);  //从USART2接收环形缓冲区读取一个字节

#endif                                  //结束串口头文件包含保护
