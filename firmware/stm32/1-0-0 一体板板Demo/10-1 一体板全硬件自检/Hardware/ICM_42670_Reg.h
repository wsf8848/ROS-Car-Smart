#ifndef __ICM_42670_REG_H
#define __ICM_42670_REG_H

/************************** 用户Bank 0 寄存器映射 (常用) **************************/
#define DEVICE_CONFIG 0X11		//设备配置寄存器: 软件复位, 时钟设置

/************************** 寄存器地址定义（ICM-42670-P）**************************/
#define WHO_AM_I       0X75    // 设备ID寄存器（默认0x67）
#define DRIVE_CONFIG  0X01    // 设备配置寄存器
#define DRIVE_CONFIG1  0X03    // 驱动配置寄存器1
#define INT_CONFIG     0X06    // 中断配置寄存器
#define PWR_MGMT0      0X1F    // 电源管理寄存器0
#define INT_CONFIG1    0X05    // 中断配置寄存器1
#define INT_SOURCE0    0X2B    // 中断源配置寄存器0
#define GYRO_CONFIG0   0X20    // 陀螺仪配置寄存器0
#define GYRO_CONFIG1   0X23    // 陀螺仪配置寄存器1
#define GYRO_ACCEL_CONFIG0   0X24    // 陀螺仪&加速度计联合配置寄存器0
#define ACCEL_CONFIG0  0X21    // 加速度计配置寄存器0
#define ACCEL_CONFIG1  0X24    // 加速度计配置寄存器1
#define SELF_TEST_CONFIG 0X70  // 自测试配置寄存器

#define TEMP_DATA1     0X09    // 温度数据高位寄存器
#define TEMP_DATA0     0X0A    // 温度数据低位寄存器

#define ACCEL_DATA_X1  0X0B    // 加速度计X轴高位寄存器
#define ACCEL_DATA_X0  0X0C    // 加速度计X轴低位寄存器
#define ACCEL_DATA_Y1  0X0D    // 加速度计Y轴高位寄存器
#define ACCEL_DATA_Y0  0X0E    // 加速度计Y轴低位寄存器
#define ACCEL_DATA_Z1  0X0F    // 加速度计Z轴高位寄存器
#define ACCEL_DATA_Z0  0X10    // 加速度计Z轴低位寄存器

#define GYRO_DATA_X1   0X11    // 陀螺仪X轴高位寄存器
#define GYRO_DATA_X0   0X12    // 陀螺仪X轴低位寄存器
#define GYRO_DATA_Y1   0X13    // 陀螺仪Y轴高位寄存器
#define GYRO_DATA_Y0   0X14    // 陀螺仪Y轴低位寄存器
#define GYRO_DATA_Z1   0X15    // 陀螺仪Z轴高位寄存器
#define GYRO_DATA_Z0   0X16    // 陀螺仪Z轴低位寄存器

/************************** 控制常量定义 **************************/
#define DEVICE_RESET 0X01       //设备复位命令 (写入DEVICE_CONFIG)

#endif
