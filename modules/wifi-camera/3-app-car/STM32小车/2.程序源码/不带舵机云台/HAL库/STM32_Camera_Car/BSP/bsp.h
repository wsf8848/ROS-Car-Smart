#ifndef __BSP_H_
#define __BSP_H_


#include "stdio.h"
#include "string.h"
#include "main.h"
#include "tim.h"
#include "gpio.h"
#include "usart.h"
#include "stdint.h"

#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "bsp_tim.h"
#include "bsp_PID_motor.h"
#include "app_motor.h"
#include "bsp_wifi.h"
#include "bsp_usart1.h"
#include "bsp_usart2.h"




void BSP_Init(void);
void BSP_Loop(void);

void FUN_delay_s(float s);

#endif
