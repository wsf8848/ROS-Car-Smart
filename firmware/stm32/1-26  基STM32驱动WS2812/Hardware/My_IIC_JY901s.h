#ifndef __MY_IIC_JY901S_H
#define __MY_IIC_JY901S_H

#include "stm32f10x.h"

/* JY901S 设备信息 */
#define JY901S_SLAVE_ADDR  0x50    // 7位从机地址（实际使用时需左移1位）
#define JY901S_WHO_AM_I    0x01    // 设备ID寄存器地址（假设为0x01，需根据手册确认）
#define JY901S_VALID_ID    0x39    // 预期设备ID值（需根据手册确认）

/* 函数声明 */
uint8_t JY901S_IsOnline(void);
uint8_t JY901S_ReadID(void);

#endif