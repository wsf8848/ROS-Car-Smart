#ifndef __BSP_USART_H
#define __BSP_USART_H


#include <stdio.h>
#include "stm32f10x.h"
#include "esp32_wifi.h"



void USART1_init(u32 baudrate);
void USART1_Send_ArrayU8(uint8_t *BufferPtr, uint16_t Length);
void USART1_Send_U8(uint8_t ch);


void USART2_init(u32 baudrate);
void USART2_Send_ArrayU8(uint8_t *BufferPtr, uint16_t Length);
void USART2_Send_U8(uint8_t ch);

#endif
