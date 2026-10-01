#ifndef __AllHeader_H
#define __AllHeader_H

#define bool _Bool
#define true 1
#define false 0

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <Time.h>

//基本头文件
#include "stm32f10x.h"
#include "delay.h"
#include "bsp_timer.h"
#include "bsp.h"

//串口部分
#include "bsp_usart.h"

//电机部分
#include "bsp_motor.h"
#include "bsp_encoder.h"
#include "PID_Motor.h"
#include "app_motor.h"

//esp32-wifi摄像头部分
#include "esp32_wifi.h"

#endif

