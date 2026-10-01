#ifndef __CHASSIS_H__                                  // 防止底盘适配头文件被重复包含
#define __CHASSIS_H__                                  // 定义底盘适配头文件包含保护宏

#include <stdint.h>                                    // 提供底盘接口使用的固定宽度整数类型

#define CHASSIS_COMMAND_FORWARD 0x41U                  // 定义掌机前进命令字节
#define CHASSIS_COMMAND_FORWARD_RIGHT 0x42U            // 定义掌机前进右转命令字节
#define CHASSIS_COMMAND_ROTATE_RIGHT 0x43U             // 定义掌机原地右转命令字节
#define CHASSIS_COMMAND_BACKWARD_RIGHT 0x44U           // 定义掌机后退右转命令字节
#define CHASSIS_COMMAND_BACKWARD 0x45U                 // 定义掌机后退命令字节
#define CHASSIS_COMMAND_BACKWARD_LEFT 0x46U            // 定义掌机后退左转命令字节
#define CHASSIS_COMMAND_ROTATE_LEFT 0x47U              // 定义掌机原地左转命令字节
#define CHASSIS_COMMAND_FORWARD_LEFT 0x48U             // 定义掌机前进左转命令字节
#define CHASSIS_COMMAND_STOP 0x49U                     // 定义掌机停车命令字节
#define CHASSIS_COMMAND_SPEED_DOWN 0x6AU               // 定义掌机L2减速命令字节
#define CHASSIS_COMMAND_SPEED_UP 0x6BU                 // 定义掌机R2加速命令字节

#define CHASSIS_MANUAL_OUTER_PWM 1800                 // 定义手动控制默认外侧轮PWM速度
#define CHASSIS_MANUAL_INNER_PWM 900                  // 定义手动控制默认内侧轮PWM速度
#define CHASSIS_MANUAL_MIN_PWM 600                    // 定义手动控制外侧轮最低PWM速度
#define CHASSIS_MANUAL_MAX_PWM 3300                   // 定义手动控制外侧轮最高PWM速度
#define CHASSIS_MANUAL_SPEED_STEP_PWM 300             // 定义每次L2或R2调整的外侧轮PWM步进
#define CHASSIS_MANUAL_INNER_RATIO_DIVISOR 2          // 定义转弯内侧轮速度为外侧轮速度的一半
#define CHASSIS_RAMP_PERIOD_MS 5UL                    // 定义手动控制PWM斜坡每五毫秒更新一次
#define CHASSIS_RAMP_STEP_PWM 120                     // 定义每次斜坡更新最多改变一百二十个PWM计数

typedef struct                                           // 定义四路编码器速度采样结果
{
    int16_t LeftRear;                                    // 保存左后轮十毫秒编码器有符号增量
    int16_t LeftFront;                                   // 保存左前轮十毫秒编码器有符号增量
    int16_t RightFront;                                  // 保存右前轮十毫秒编码器有符号增量
    int16_t RightRear;                                   // 保存右后轮十毫秒编码器有符号增量
} ChassisSpeedSnapshotTypeDef;                          // 结束四路编码器速度结构体定义

void Chassis_Init(void);                                 // 初始化四个电机PWM、方向和编码器接口
void Chassis_Stop(void);                                 // 立即关闭四个轮子的PWM输出
void Chassis_ApplyBluetoothCommand(uint8_t Command);    // 将掌机运动命令转换为四轮目标PWM
void Chassis_AdjustManualSpeed(int8_t Direction);       // 按正负方向调整手动控制速度档位
uint16_t Chassis_GetManualSpeedPwm(void);               // 读取当前手动控制外侧轮PWM速度
void Chassis_Update(uint32_t NowMs);                    // 按五毫秒斜坡把当前PWM平滑推进到目标值
void Chassis_GetSpeedSnapshot(ChassisSpeedSnapshotTypeDef *Snapshot); // 读取四路十毫秒编码器增量
uint8_t Chassis_EncodeSpeed(int16_t Speed);              // 将有符号编码器增量压缩为协议单字节

#endif                                                // 结束底盘适配头文件包含保护
