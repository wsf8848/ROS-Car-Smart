#ifndef __ICM_42670_H
#define __ICM_42670_H

#include "stm32f10x.h"                  // Device header
//C语言库
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>      // 用于atan2()函数
#include <stdbool.h>   // 包含标准布尔类型定义

unsigned char ICM_42670_WriteReg(unsigned char RegAddress, unsigned char Data);
uint8_t ICM_42670_ReadReg(uint8_t RegAddress);

void ICM_42670_Init(void);
uint8_t ICM_42670_GetID(void);
unsigned char ICM_INIT(void);
unsigned char ICM_Gyroscope_INIT(void);
unsigned char GYRO_ACC_TEMP_GET(void);

//获取角速度
typedef struct{
    double X;
    double Y;
    double Z;
}GYRO;

//获取加速度
typedef struct{
    double X;
    double Y;
    double Z;
}ACC;
//获取温度
typedef struct{
    double T;
}TEMP;

#endif
