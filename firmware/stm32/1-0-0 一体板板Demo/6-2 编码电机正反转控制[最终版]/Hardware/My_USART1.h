#ifndef __MY_USART1_H
#define __MY_USART1_H

#include <stdio.h>
#include "stm32f10x.h"                  // Device header

void My_USART1_Init(void);
void My_USART1_SendByte(uint8_t Byte);
void My_USART1_SendArray(uint8_t *Array, uint16_t Length);
void My_USART1_SendString(char *String);
void My_USART1_SendNumber(uint32_t Number, uint8_t Length);
void My_USART1_Printf(char *format, ...);

#endif
