#ifndef __ENCODER_H
#define __ENCODER_H
#include "stm32f10x.h"                  // Device header

void EncoderA_Init(void);
int16_t EncoderA_Get(void);

void EncoderB_Init(void);
int16_t EncoderB_Get(void);

void EncoderC_Init(void);
int16_t EncoderC_Get(void);

void EncoderD_Init(void);
int16_t EncoderD_Get(void);

#endif