#include "Autonomous.h"                           // 提供自主导航状态机接口
#include "AutonomousConfig.h"                     // 提供自主导航距离和保护参数
#include "AvoidanceConfig.h"                      // 提供雷达传感器超时参数
#include "Bluetooth.h"                             // 提供当前工作模式和底盘速度接口
#include "Chassis.h"                               // 提供四轮底盘命令和编码速度接口
#include "Battery.h"                               // 提供电池百分比保护接口

static AutonomousActionTypeDef AutonomousAction = AUTONOMOUS_ACTION_STOP; // 保存自主导航当前实际动作
static AutonomousProtectionTypeDef AutonomousProtection = AUTONOMOUS_PROTECTION_NONE; // 保存自主导航当前保护原因
static int16_t AutonomousPitchDeg10 = 0;          // 保存预留陀螺仪俯仰角，单位0.1度
static int16_t AutonomousRollDeg10 = 0;           // 保存预留陀螺仪横滚角，单位0.1度
static uint8_t AutonomousAttitudeValid = 0U;      // 保存陀螺仪姿态数据是否有效
static uint8_t AutonomousMismatchCounter = 0U;    // 保存连续电机速度不一致计数
static AutonomousActionTypeDef AutonomousEscapeAction = AUTONOMOUS_ACTION_STOP; // 保存锁存的绕行方向
static uint32_t AutonomousLastSpeedCheckMs = 0UL; // 保存上一次编码器速度一致性检查时间

static uint8_t Autonomous_IsClear(uint16_t DistanceCm) // 判断侧向距离是否达到自主绕行安全距离
{
    return (uint8_t)(DistanceCm != AVOIDANCE_UNKNOWN_DISTANCE_CM && DistanceCm >= AUTONOMOUS_SIDE_CLEAR_CM); // 未知距离按不安全处理
}

static AutonomousActionTypeDef Autonomous_SelectEscape(const AvoidanceStatusTypeDef *Status) // 比较左右空间并选择绕行方向
{
    uint8_t LeftClear = Autonomous_IsClear(Status->LeftDistanceCm); // 保存左侧是否可通行
    uint8_t RightClear = Autonomous_IsClear(Status->RightDistanceCm); // 保存右侧是否可通行

    if (LeftClear != 0U && RightClear == 0U) return AUTONOMOUS_ACTION_LEFT; // 仅左侧安全时向左绕行
    if (RightClear != 0U && LeftClear == 0U) return AUTONOMOUS_ACTION_RIGHT; // 仅右侧安全时向右绕行
    if (LeftClear == 0U && RightClear == 0U) return AUTONOMOUS_ACTION_STOP; // 两侧均不安全时停车
    if (Status->LeftDistanceCm > (uint16_t)(Status->RightDistanceCm + AUTONOMOUS_CLEAR_MARGIN_CM)) return AUTONOMOUS_ACTION_LEFT; // 左侧明显更空旷
    if (Status->RightDistanceCm > (uint16_t)(Status->LeftDistanceCm + AUTONOMOUS_CLEAR_MARGIN_CM)) return AUTONOMOUS_ACTION_RIGHT; // 右侧明显更空旷
    return (AutonomousEscapeAction == AUTONOMOUS_ACTION_RIGHT) ? AUTONOMOUS_ACTION_RIGHT : AUTONOMOUS_ACTION_LEFT; // 空间接近时保持上次方向
}

static uint8_t Autonomous_MotorSpeedIsAbnormal(void) // 检查四个编码器速度是否严重不一致
{
    ChassisSpeedSnapshotTypeDef Snapshot;           // 保存本次四轮编码器增量快照
    int16_t MaximumSpeed;                            // 保存四轮速度绝对值最大值
    int16_t MinimumSpeed;                            // 保存四轮速度绝对值最小值
    int16_t SpeedValue;                             // 保存遍历中的单轮速度绝对值

    Chassis_GetSpeedSnapshot(&Snapshot);            // 读取并清零四轮十毫秒编码器计数
    MaximumSpeed = 0;                               // 初始化最大速度
    MinimumSpeed = 32767;                           // 初始化最小速度
    SpeedValue = Snapshot.LeftRear < 0 ? -Snapshot.LeftRear : Snapshot.LeftRear; if (SpeedValue > MaximumSpeed) MaximumSpeed = SpeedValue; if (SpeedValue < MinimumSpeed) MinimumSpeed = SpeedValue; // 处理左后轮
    SpeedValue = Snapshot.LeftFront < 0 ? -Snapshot.LeftFront : Snapshot.LeftFront; if (SpeedValue > MaximumSpeed) MaximumSpeed = SpeedValue; if (SpeedValue < MinimumSpeed) MinimumSpeed = SpeedValue; // 处理左前轮
    SpeedValue = Snapshot.RightFront < 0 ? -Snapshot.RightFront : Snapshot.RightFront; if (SpeedValue > MaximumSpeed) MaximumSpeed = SpeedValue; if (SpeedValue < MinimumSpeed) MinimumSpeed = SpeedValue; // 处理右前轮
    SpeedValue = Snapshot.RightRear < 0 ? -Snapshot.RightRear : Snapshot.RightRear; if (SpeedValue > MaximumSpeed) MaximumSpeed = SpeedValue; if (SpeedValue < MinimumSpeed) MinimumSpeed = SpeedValue; // 处理右后轮
    return (uint8_t)((MaximumSpeed > AUTONOMOUS_SPEED_MISMATCH_CM) && ((MaximumSpeed - MinimumSpeed) > AUTONOMOUS_SPEED_MISMATCH_CM)); // 仅在车辆确实运动时判断速度差异
}

void Autonomous_Init(void) // 初始化自主导航状态机
{
    AutonomousAction = AUTONOMOUS_ACTION_STOP;       // 上电默认自主动作停车
    AutonomousProtection = AUTONOMOUS_PROTECTION_NONE; // 清除自主保护原因
    AutonomousMismatchCounter = 0U;                  // 清除电机速度异常计数
    AutonomousEscapeAction = AUTONOMOUS_ACTION_STOP; // 清除锁存绕行方向
    AutonomousLastSpeedCheckMs = 0UL;                // 清除编码器检查时间
    AutonomousAttitudeValid = 0U;                    // 默认陀螺仪尚未接入
}

void Autonomous_SetAttitude(int16_t PitchDeg10, int16_t RollDeg10, uint8_t Valid) // 更新预留姿态保护输入
{
    AutonomousPitchDeg10 = PitchDeg10;               // 保存俯仰角
    AutonomousRollDeg10 = RollDeg10;                 // 保存横滚角
    AutonomousAttitudeValid = Valid;                 // 保存姿态数据有效标志
}

void Autonomous_Update(const AvoidanceStatusTypeDef *Status, uint32_t NowMs) // 执行一次自主导航决策
{
    (void)NowMs;                                      // 当前决策使用5ms调用节拍，不直接依赖绝对时间
    if (Bluetooth_GetMode() != BLUETOOTH_MODE_AUTONOMOUS || Bluetooth_IsEmergencyStopped() != 0U || Status == 0) // 判断是否仍处于自主模式且未被停车锁存
    {
        AutonomousAction = AUTONOMOUS_ACTION_STOP;   // 非自主模式清除自主动作
        AutonomousProtection = AUTONOMOUS_PROTECTION_NONE; // 非自主模式清除保护原因
        AutonomousMismatchCounter = 0U;              // 清除速度异常计数
        Chassis_Stop();                              // 非自主模式不允许自主模块驱动底盘
        return;                                       // 结束非自主处理
    }
    AutonomousProtection = AUTONOMOUS_PROTECTION_NONE; // 每次先清除瞬时保护原因
    if (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT || Status->DataAgeMs > AVOIDANCE_SENSOR_TIMEOUT_MS) // 检查雷达链路安全
    {
        AutonomousProtection = AUTONOMOUS_PROTECTION_SENSOR; // 标记雷达保护
    }
    else if (Battery_GetPercent() < AUTONOMOUS_LOW_BATTERY_PERCENT) // 检查电池电量是否低于10%
    {
        AutonomousProtection = AUTONOMOUS_PROTECTION_BATTERY; // 标记低电量保护
    }
    else if (AutonomousAttitudeValid != 0U && (AutonomousPitchDeg10 > AUTONOMOUS_PITCH_LIMIT_DEG10 || AutonomousPitchDeg10 < -AUTONOMOUS_PITCH_LIMIT_DEG10 || AutonomousRollDeg10 > AUTONOMOUS_ROLL_LIMIT_DEG10 || AutonomousRollDeg10 < -AUTONOMOUS_ROLL_LIMIT_DEG10)) // 检查预留俯仰横滚角
    {
        AutonomousProtection = AUTONOMOUS_PROTECTION_ATTITUDE; // 标记姿态保护
    }
    else if (AutonomousEscapeAction == AUTONOMOUS_ACTION_STOP) // 仅在自主直行阶段执行编码器速度一致性检查
    {
        if ((uint32_t)(NowMs - AutonomousLastSpeedCheckMs) >= 20UL) // 每20毫秒检查一次，避免与10毫秒蓝牙遥测重复读取编码器
        {
            AutonomousLastSpeedCheckMs = NowMs;       // 记录本次编码器速度检查时间
            if (Autonomous_MotorSpeedIsAbnormal() != 0U) // 判断本次四轮速度是否严重不一致
            {
                if (AutonomousMismatchCounter < 255U) AutonomousMismatchCounter++; // 累加连续速度异常次数
                if (AutonomousMismatchCounter >= AUTONOMOUS_SPEED_MISMATCH_COUNT) AutonomousProtection = AUTONOMOUS_PROTECTION_MOTOR; // 连续异常后进入电机保护
            }
            else
            {
                AutonomousMismatchCounter = 0U;      // 本次速度恢复正常时清除连续异常计数
            }
        }
    }
    else
    {
        AutonomousMismatchCounter = 0U;              // 弧线绕行阶段不执行电机速度差异保护
    }
    if (AutonomousProtection != AUTONOMOUS_PROTECTION_NONE) // 判断是否需要保护停车
    {
        AutonomousAction = AUTONOMOUS_ACTION_STOP;   // 锁定自主停车动作
        AutonomousEscapeAction = AUTONOMOUS_ACTION_STOP; // 清除绕行方向
        Chassis_Stop();                              // 立即关闭四路电机目标
        return;                                       // 保护状态下不再执行导航
    }
    if (Status->FrontDistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM) // 未知前方距离按不安全处理
    {
        AutonomousProtection = AUTONOMOUS_PROTECTION_SENSOR; // 标记传感器保护
        AutonomousAction = AUTONOMOUS_ACTION_STOP;   // 保持停车
        Chassis_Stop();                              // 停车等待有效距离
        return;                                       // 结束未知距离处理
    }
    if (AutonomousEscapeAction != AUTONOMOUS_ACTION_STOP && Status->FrontDistanceCm >= AUTONOMOUS_FRONT_RELEASE_CM) // 判断绕行是否可以释放
    {
        AutonomousEscapeAction = AUTONOMOUS_ACTION_STOP; // 前方恢复后回到自主直行
    }
    if (AutonomousEscapeAction == AUTONOMOUS_ACTION_STOP && Status->FrontDistanceCm <= AUTONOMOUS_FRONT_TRIGGER_CM) // 判断是否首次进入绕行区
    {
        AutonomousEscapeAction = Autonomous_SelectEscape(Status); // 按左右空间选择路径
    }
    if (Status->FrontDistanceCm <= AUTONOMOUS_EMERGENCY_CM) // 判断是否进入紧急近距离
    {
        AutonomousEscapeAction = Autonomous_SelectEscape(Status); // 紧急时重新选择可用方向
        if (AutonomousEscapeAction == AUTONOMOUS_ACTION_LEFT) { AutonomousAction = AUTONOMOUS_ACTION_ROTATE_LEFT; Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_ROTATE_LEFT); return; } // 紧急左转
        if (AutonomousEscapeAction == AUTONOMOUS_ACTION_RIGHT) { AutonomousAction = AUTONOMOUS_ACTION_ROTATE_RIGHT; Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_ROTATE_RIGHT); return; } // 紧急右转
        AutonomousAction = AUTONOMOUS_ACTION_STOP; Chassis_Stop(); return; // 两侧均不安全时停车
    }
    if (AutonomousEscapeAction == AUTONOMOUS_ACTION_LEFT && Autonomous_IsClear(Status->LeftDistanceCm) != 0U) // 判断是否执行左弧线
    {
        AutonomousAction = AUTONOMOUS_ACTION_LEFT; Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_FORWARD_LEFT); return; // 自主向左绕行
    }
    if (AutonomousEscapeAction == AUTONOMOUS_ACTION_RIGHT && Autonomous_IsClear(Status->RightDistanceCm) != 0U) // 判断是否执行右弧线
    {
        AutonomousAction = AUTONOMOUS_ACTION_RIGHT; Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_FORWARD_RIGHT); return; // 自主向右绕行
    }
    if (AutonomousEscapeAction != AUTONOMOUS_ACTION_STOP) // 判断锁存方向是否被新障碍物阻断
    {
        AutonomousEscapeAction = Autonomous_SelectEscape(Status); // 重新选侧
        if (AutonomousEscapeAction == AUTONOMOUS_ACTION_LEFT || AutonomousEscapeAction == AUTONOMOUS_ACTION_RIGHT) return; // 下一周期按新方向执行
    }
    AutonomousAction = AUTONOMOUS_ACTION_FORWARD;     // 前方安全时保持自主前进
    Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_FORWARD); // 使用当前L2/R2速度档位执行前进
}

AutonomousActionTypeDef Autonomous_GetAction(void) // 获取自主导航当前动作
{
    return AutonomousAction;                          // 返回自主导航动作
}

AutonomousProtectionTypeDef Autonomous_GetProtection(void) // 获取自主导航保护原因
{
    return AutonomousProtection;                      // 返回自主导航保护原因
}
