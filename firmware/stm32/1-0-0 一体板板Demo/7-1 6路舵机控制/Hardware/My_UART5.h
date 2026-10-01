#ifndef __MY_UART5_H
#define __MY_UART5_H

#include <stdio.h>
#include "stm32f10x.h"                  // Device header

void My_UART5_Init(void);
void My_UART5_SendByte(uint8_t Byte);
void My_UART5_SendArray(uint8_t *Array, uint16_t Length);
void My_UART5_SendString(char *String);
void My_UART5_SendNumber(uint32_t Number, uint8_t Length);
void My_UART5_Printf(char *format, ...);

#endif
