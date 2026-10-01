#ifndef __ICM42670_H__                                  // 防止ICM42670驱动头文件被重复包含
#define __ICM42670_H__                                  // 定义ICM42670驱动头文件保护宏

#include "stm32f10x.h"                                 // 提供GPIO和固定宽度整数类型定义

#define ICM42670_EXPECTED_ID 0x47U                      // 定义已验证ICM42670的WHO_AM_I期望值
#define ICM42670_I2C_WRITE_ADDRESS 0xD0U                // 定义ICM42670八位I2C写地址
#define ICM42670_I2C_READ_ADDRESS 0xD1U                 // 定义ICM42670八位I2C读地址

typedef struct                                           // 定义ICM42670一次六轴温度采样结果
{
    float AccelXMeterPerSecond2;                         // 保存X轴加速度，单位米每二次方秒
    float AccelYMeterPerSecond2;                         // 保存Y轴加速度，单位米每二次方秒
    float AccelZMeterPerSecond2;                         // 保存Z轴加速度，单位米每二次方秒
    float GyroXRadianPerSecond;                          // 保存X轴角速度，单位弧度每秒
    float GyroYRadianPerSecond;                          // 保存Y轴角速度，单位弧度每秒
    float GyroZRadianPerSecond;                          // 保存Z轴角速度，单位弧度每秒
    float TemperatureCelsius;                            // 保存芯片温度，单位摄氏度
} ICM42670SampleTypeDef;                                 // 结束ICM42670采样结构体定义

uint8_t ICM42670_Init(void);                             // 初始化PB4/PA12软件I2C并配置ICM42670
uint8_t ICM42670_ReadSample(ICM42670SampleTypeDef *Sample); // 读取并换算一次六轴与温度数据
uint8_t ICM42670_IsReady(void);                          // 查询WHO_AM_I和初始化是否通过
uint8_t ICM42670_GetDeviceId(void);                      // 读取最近一次检测到的WHO_AM_I值
uint32_t ICM42670_GetReadErrorCount(void);               // 读取累计I2C应答或参数错误次数

#endif                                                  // 结束ICM42670驱动头文件保护
