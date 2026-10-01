#ifndef __DRIVE_CONTROL_H                 //防止底盘控制头文件被重复包含
#define __DRIVE_CONTROL_H                 //定义底盘控制头文件包含保护宏

#include "stm32f10x.h"                  //提供底盘控制接口使用的固定宽度整数类型
#include "Avoidance.h"                  //提供自主避障状态和标准运动指令类型
#include "Bluetooth.h"                  //提供手机蓝牙控制命令类型

#define DRIVE_MAX_PWM 1800                //定义四轮控制使用的最大PWM比较值
#define DRIVE_CURVE_PWM 900               //定义前进或后退转向时内侧轮PWM比较值
#define DRIVE_TURN_PWM 1500               //定义原地左右转向时单侧PWM比较值
#define DRIVE_RAMP_STEP 180               //定义每次五毫秒控制更新允许的最大速度变化量

void DriveControl_Init(void);             //初始化四电机驱动并将小车置于安全停止状态
void DriveControl_Update(const AvoidanceStatusTypeDef *Status, const MotionCommandTypeDef *AutonomousCommand, BluetoothControlTypeDef ManualCommand);//按避障优先级和蓝牙命令更新平滑四轮输出
void DriveControl_GetCurrentPwm(int16_t *LeftRear, int16_t *LeftFront, int16_t *RightFront, int16_t *RightRear);//读取当前四轮平滑PWM值

#endif                                    //结束底盘控制头文件包含保护
