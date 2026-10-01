#ifndef __MY_UART4_H
#define __MY_UART4_H

#include <stdio.h>
#include "stm32f10x.h"                  // Device header

void My_UART4_Init(void);
void My_UART4_SendByte(uint8_t Byte);
void My_UART4_SendArray(uint8_t *Array, uint16_t Length);
void My_UART4_SendString(char *String);
void My_UART4_SendNumber(uint32_t Number, uint8_t Length);
void My_UART4_Printf(char *format, ...);

#endif
