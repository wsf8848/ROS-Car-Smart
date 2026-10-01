#include "Chassis.h"                                   // 提供底盘控制与速度采样接口声明
#include "Motor.h"                                     // 提供四路电机方向和PWM控制接口
#include "Encoder.h"                                   // 提供四路编码器接口

/**
  * 函    数：底盘驱动初始化
  * 参    数：无
  * 返 回 值：无
  */
void Chassis_Init(void)
{
    EncoderA_Init();                                    // 初始化左后轮TIM3正交编码器
    EncoderB_Init();                                    // 初始化左前轮TIM2正交编码器
    EncoderC_Init();                                    // 初始化右前轮TIM4正交编码器
    EncoderD_Init();                                    // 初始化右后轮TIM5正交编码器
    MotorABCD_Init();                                   // 初始化四轮方向引脚和TIM8 PWM
    Chassis_Stop();                                     // 初始化完成后保持四轮安全停车
}

/**
  * 函    数：底盘安全停车
  * 参    数：无
  * 返 回 值：无
  */
void Chassis_Stop(void)
{
    Set_PWM(0, 0, 0, 0);                                // 将四个轮子的PWM比较值清零并关闭动力输出
}

/**
  * 函    数：执行掌机运动命令
  * 参    数：Command 掌机发送的单字节运动命令
  * 返 回 值：无
  */
void Chassis_ApplyBluetoothCommand(uint8_t Command)
{
    switch (Command)                                    // 根据掌机协议选择四轮差速组合
    {
        case CHASSIS_COMMAND_FORWARD:                   // 处理0x41全车前进命令
            Set_PWM(CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM); // 四轮同速前进
            break;                                      // 结束前进命令处理
        case CHASSIS_COMMAND_FORWARD_RIGHT:             // 处理0x42前进并右转命令
            Set_PWM(CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_INNER_PWM, CHASSIS_MANUAL_INNER_PWM); // 左侧快、右侧慢实现平滑右弯
            break;                                      // 结束前进右转命令处理
        case CHASSIS_COMMAND_ROTATE_RIGHT:              // 处理0x43原地右转命令
            Set_PWM(CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM); // 左轮前进、右轮后退实现原地右转
            break;                                      // 结束原地右转命令处理
        case CHASSIS_COMMAND_BACKWARD_RIGHT:            // 处理0x44后退并右转命令
            Set_PWM(-CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_INNER_PWM, -CHASSIS_MANUAL_INNER_PWM); // 左侧快、右侧慢实现平滑后退右弯
            break;                                      // 结束后退右转命令处理
        case CHASSIS_COMMAND_BACKWARD:                  // 处理0x45全车后退命令
            Set_PWM(-CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM); // 四轮同速后退
            break;                                      // 结束后退命令处理
        case CHASSIS_COMMAND_BACKWARD_LEFT:             // 处理0x46后退并左转命令
            Set_PWM(-CHASSIS_MANUAL_INNER_PWM, -CHASSIS_MANUAL_INNER_PWM, -CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM); // 右侧快、左侧慢实现平滑后退左弯
            break;                                      // 结束后退左转命令处理
        case CHASSIS_COMMAND_ROTATE_LEFT:               // 处理0x47原地左转命令
            Set_PWM(-CHASSIS_MANUAL_OUTER_PWM, -CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM); // 左轮后退、右轮前进实现原地左转
            break;                                      // 结束原地左转命令处理
        case CHASSIS_COMMAND_FORWARD_LEFT:              // 处理0x48前进并左转命令
            Set_PWM(CHASSIS_MANUAL_INNER_PWM, CHASSIS_MANUAL_INNER_PWM, CHASSIS_MANUAL_OUTER_PWM, CHASSIS_MANUAL_OUTER_PWM); // 右侧快、左侧慢实现平滑左弯
            break;                                      // 结束前进左转命令处理
        case CHASSIS_COMMAND_STOP:                      // 处理0x49停车命令
            Chassis_Stop();                             // 立即关闭四轮动力输出
            break;                                      // 结束停车命令处理
        default:                                        // 处理未知掌机命令
            Chassis_Stop();                             // 未知命令按安全策略停车
            break;                                      // 结束未知命令处理
    }
}

/**
  * 函    数：读取四路编码器速度快照
  * 参    数：Snapshot 用于保存四路十毫秒编码器增量的结构体地址
  * 返 回 值：无
  */
void Chassis_GetSpeedSnapshot(ChassisSpeedSnapshotTypeDef *Snapshot)
{
    if (Snapshot == 0)                                 // 判断调用者是否提供有效的结果地址
    {
        return;                                         // 地址无效时不访问编码器并直接返回
    }
    Snapshot->LeftRear = (int16_t)(-((int32_t)EncoderA_Get())); // 按已验证极性修正左后轮编码器正负方向
    Snapshot->LeftFront = EncoderB_Get();               // 读取左前轮编码器十毫秒增量
    Snapshot->RightFront = EncoderC_Get();              // 读取右前轮编码器十毫秒增量
    Snapshot->RightRear = (int16_t)(-((int32_t)EncoderD_Get())); // 按已验证极性修正右后轮编码器正负方向
}

/**
  * 函    数：将编码器增量压缩为蓝牙单字节
  * 参    数：Speed 有符号编码器增量
  * 返 回 值：采用int8二补码表示的协议字节，超范围时饱和
  */
uint8_t Chassis_EncodeSpeed(int16_t Speed)
{
    if (Speed > 127)                                    // 判断正向编码器增量是否超过协议范围
    {
        Speed = 127;                                    // 将正向编码器增量饱和到127
    }
    if (Speed < -127)                                   // 判断反向编码器增量是否超过协议范围
    {
        Speed = -127;                                   // 将反向编码器增量饱和到-127
    }
    return (uint8_t)((int8_t)Speed);                    // 以int8二补码形式返回单字节速度字段
}
