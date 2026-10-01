#ifndef __AUTONOMOUS_H__                           // 防止自主导航接口头文件被重复包含
#define __AUTONOMOUS_H__                           // 定义自主导航接口头文件保护宏

#include "Avoidance.h"                            // 提供雷达四向距离状态结构体

typedef enum                                      // 定义自主导航当前实际动作
{
    AUTONOMOUS_ACTION_STOP = 0U,                   // 表示自主导航已停车或保护停车
    AUTONOMOUS_ACTION_FORWARD,                     // 表示自主导航正常前进
    AUTONOMOUS_ACTION_LEFT,                        // 表示自主导航向左弧线绕行
    AUTONOMOUS_ACTION_RIGHT,                       // 表示自主导航向右弧线绕行
    AUTONOMOUS_ACTION_ROTATE_LEFT,                 // 表示自主导航原地左转
    AUTONOMOUS_ACTION_ROTATE_RIGHT                 // 表示自主导航原地右转
} AutonomousActionTypeDef;                         // 结束自主导航动作枚举

typedef enum                                      // 定义自主导航停车保护原因
{
    AUTONOMOUS_PROTECTION_NONE = 0U,               // 表示当前没有保护故障
    AUTONOMOUS_PROTECTION_SENSOR,                  // 表示雷达未就绪、故障或数据超时
    AUTONOMOUS_PROTECTION_MOTOR,                   // 表示四轮编码速度严重不一致
    AUTONOMOUS_PROTECTION_ATTITUDE,                // 表示俯仰角或横滚角超限
    AUTONOMOUS_PROTECTION_BATTERY                  // 表示电池电量低于保护阈值
} AutonomousProtectionTypeDef;                    // 结束自主导航保护原因枚举

void Autonomous_Init(void);                       // 初始化自主导航状态机和保护状态
void Autonomous_Update(const AvoidanceStatusTypeDef *Status, uint32_t NowMs); // 按雷达和保护状态更新自主底盘输出
void Autonomous_SetAttitude(int16_t PitchDeg10, int16_t RollDeg10, uint8_t Valid); // 预留陀螺仪俯仰横滚角输入接口
AutonomousActionTypeDef Autonomous_GetAction(void); // 获取自主导航当前实际动作
AutonomousProtectionTypeDef Autonomous_GetProtection(void); // 获取自主导航当前停车保护原因

#endif                                           // 结束自主导航接口头文件保护
