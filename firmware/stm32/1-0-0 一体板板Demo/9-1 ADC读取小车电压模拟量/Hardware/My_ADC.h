#ifndef __MY_ADC_H
#define __MY_ADC_H
#include "stm32f10x.h"                  // Device header

void AD_Init(void);
uint16_t Get_Adc(uint16_t ch);
float convert_battery_voltage(uint16_t adc_value);
	
#endif