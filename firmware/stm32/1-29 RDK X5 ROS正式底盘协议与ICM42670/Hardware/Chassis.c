#include "Chassis.h"                                   // 提供底盘控制与速度采样接口声明
#include "Motor.h"                                     // 提供四路电机方向和PWM控制接口
#include "Encoder.h"                                   // 提供四路编码器接口
#include "RosProtocol.h"                                // 提供上电IMU校零完成状态查询

static int16_t ChassisTargetPwm[4] = {0, 0, 0, 0};     // 保存左后左前右前右后四轮目标PWM
static int16_t ChassisCurrentPwm[4] = {0, 0, 0, 0};    // 保存左后左前右前右后四轮当前PWM
static uint32_t ChassisLastRampMs = 0UL;                // 保存上一次执行PWM斜坡更新的毫秒时间
static uint16_t ChassisManualOuterPwm = CHASSIS_MANUAL_OUTER_PWM; // 保存当前手动控制外侧轮PWM速度档位
static uint16_t ChassisManualInnerPwm = CHASSIS_MANUAL_INNER_PWM; // 保存当前手动控制内侧轮PWM速度档位
static volatile int32_t ChassisAccumulatedTick[4] = {0, 0, 0, 0}; // 保存四轮累计编码器计数供ROS里程计使用
static volatile int16_t ChassisLatestDelta[4] = {0, 0, 0, 0}; // 保存最近一次十毫秒四轮编码器增量

/**
  * 函    数：设置四轮目标PWM
  * 参    数：LeftRear至RightRear 左后左前右前右后轮目标PWM
  * 返 回 值：无
  */
static void Chassis_SetTargetPwm(int16_t LeftRear, int16_t LeftFront, int16_t RightFront, int16_t RightRear)
{
    ChassisTargetPwm[0] = LeftRear;                     // 更新左后轮目标PWM
    ChassisTargetPwm[1] = LeftFront;                    // 更新左前轮目标PWM
    ChassisTargetPwm[2] = RightFront;                   // 更新右前轮目标PWM
    ChassisTargetPwm[3] = RightRear;                    // 更新右后轮目标PWM
}

/**
  * 函    数：把单路当前PWM平滑推进到目标PWM
  * 参    数：CurrentPwm 当前PWM，TargetPwm目标PWM，MaximumStep本次最大变化量
  * 返 回 值：完成一次限速推进后的PWM
  */
static int16_t Chassis_RampPwm(int16_t CurrentPwm, int16_t TargetPwm, int32_t MaximumStep)
{
    int32_t Difference = (int32_t)TargetPwm - CurrentPwm; // 计算目标值与当前值之间的有符号差值

    if (Difference > MaximumStep)                       // 判断正向加速差值是否超过本次允许变化量
    {
        return (int16_t)((int32_t)CurrentPwm + MaximumStep); // 按最大变化量平滑增加PWM
    }
    if (Difference < -MaximumStep)                      // 判断减速或反向差值是否超过本次允许变化量
    {
        return (int16_t)((int32_t)CurrentPwm - MaximumStep); // 按最大变化量平滑降低PWM并经过零点换向
    }
    return TargetPwm;                                   // 差值较小时直接到达目标PWM
}

/**
  * 函    数：根据外侧轮速度同步内侧轮速度
  * 参    数：OuterPwm 当前手动控制外侧轮PWM速度
  * 返 回 值：无
  */
static void Chassis_UpdateManualInnerPwm(uint16_t OuterPwm)
{
    ChassisManualOuterPwm = OuterPwm;                    // 保存新的手动控制外侧轮PWM速度
    ChassisManualInnerPwm = (uint16_t)(OuterPwm / CHASSIS_MANUAL_INNER_RATIO_DIVISOR); //保持内侧轮为外侧轮一半速度
}

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
    Chassis_UpdateManualInnerPwm(CHASSIS_MANUAL_OUTER_PWM); // 恢复默认手动速度档位
    ChassisLastRampMs = 0UL;                            // 清除PWM斜坡更新时间
    ChassisAccumulatedTick[0] = 0;                      // 清除左后轮累计编码器计数
    ChassisAccumulatedTick[1] = 0;                      // 清除左前轮累计编码器计数
    ChassisAccumulatedTick[2] = 0;                      // 清除右前轮累计编码器计数
    ChassisAccumulatedTick[3] = 0;                      // 清除右后轮累计编码器计数
    ChassisLatestDelta[0] = 0;                          // 清除左后轮最近编码器增量
    ChassisLatestDelta[1] = 0;                          // 清除左前轮最近编码器增量
    ChassisLatestDelta[2] = 0;                          // 清除右前轮最近编码器增量
    ChassisLatestDelta[3] = 0;                          // 清除右后轮最近编码器增量
    Chassis_Stop();                                     // 初始化完成后保持四轮安全停车
}

/**
  * 函    数：底盘安全停车
  * 参    数：无
  * 返 回 值：无
  */
void Chassis_Stop(void)
{
    Chassis_SetTargetPwm(0, 0, 0, 0);                  // 清除四轮目标PWM并取消尚未完成的加速过程
    ChassisCurrentPwm[0] = 0;                           // 清除左后轮当前PWM缓存
    ChassisCurrentPwm[1] = 0;                           // 清除左前轮当前PWM缓存
    ChassisCurrentPwm[2] = 0;                           // 清除右前轮当前PWM缓存
    ChassisCurrentPwm[3] = 0;                           // 清除右后轮当前PWM缓存
    Set_PWM(0, 0, 0, 0);                                // 将四个轮子的PWM比较值清零并关闭动力输出
}

/**
  * 函    数：执行掌机运动命令
  * 参    数：Command 掌机发送的单字节运动命令
  * 返 回 值：无
  */
void Chassis_ApplyBluetoothCommand(uint8_t Command)
{
    if (RosProtocol_IsControlReady() == 0U)             // 判断上电五秒IMU校零是否尚未完成
    {
        Chassis_Stop();                                  // 校零期间强制停车并拒绝蓝牙、MQTT和旧自主输出
        return;                                          // 不进入任何运动命令分支
    }
    switch (Command)                                    // 根据掌机协议选择四轮差速组合
    {
        case CHASSIS_COMMAND_FORWARD:                   // 处理0x41全车前进命令
            Chassis_SetTargetPwm(ChassisManualOuterPwm, ChassisManualOuterPwm, ChassisManualOuterPwm, ChassisManualOuterPwm); // 设置四轮同速前进目标
            break;                                      // 结束前进命令处理
        case CHASSIS_COMMAND_FORWARD_RIGHT:             // 处理0x42前进并右转命令
            Chassis_SetTargetPwm(ChassisManualOuterPwm, ChassisManualOuterPwm, ChassisManualInnerPwm, ChassisManualInnerPwm); // 设置左侧快右侧慢的右弯目标
            break;                                      // 结束前进右转命令处理
        case CHASSIS_COMMAND_ROTATE_RIGHT:              // 处理0x43原地右转命令
            Chassis_SetTargetPwm(ChassisManualOuterPwm, ChassisManualOuterPwm, -ChassisManualOuterPwm, -ChassisManualOuterPwm); // 设置左轮前进右轮后退的原地右转目标
            break;                                      // 结束原地右转命令处理
        case CHASSIS_COMMAND_BACKWARD_RIGHT:            // 处理0x44后退并右转命令
            Chassis_SetTargetPwm(-ChassisManualOuterPwm, -ChassisManualOuterPwm, -ChassisManualInnerPwm, -ChassisManualInnerPwm); // 设置左侧快右侧慢的后退右弯目标
            break;                                      // 结束后退右转命令处理
        case CHASSIS_COMMAND_BACKWARD:                  // 处理0x45全车后退命令
            Chassis_SetTargetPwm(-ChassisManualOuterPwm, -ChassisManualOuterPwm, -ChassisManualOuterPwm, -ChassisManualOuterPwm); // 设置四轮同速后退目标
            break;                                      // 结束后退命令处理
        case CHASSIS_COMMAND_BACKWARD_LEFT:             // 处理0x46后退并左转命令
            Chassis_SetTargetPwm(-ChassisManualInnerPwm, -ChassisManualInnerPwm, -ChassisManualOuterPwm, -ChassisManualOuterPwm); // 设置右侧快左侧慢的后退左弯目标
            break;                                      // 结束后退左转命令处理
        case CHASSIS_COMMAND_ROTATE_LEFT:               // 处理0x47原地左转命令
            Chassis_SetTargetPwm(-ChassisManualOuterPwm, -ChassisManualOuterPwm, ChassisManualOuterPwm, ChassisManualOuterPwm); // 设置左轮后退右轮前进的原地左转目标
            break;                                      // 结束原地左转命令处理
        case CHASSIS_COMMAND_FORWARD_LEFT:              // 处理0x48前进并左转命令
            Chassis_SetTargetPwm(ChassisManualInnerPwm, ChassisManualInnerPwm, ChassisManualOuterPwm, ChassisManualOuterPwm); // 设置右侧快左侧慢的前进左弯目标
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
  * 函    数：调整手动控制速度档位
  * 参    数：Direction 正一表示加速，负一表示减速，零表示保持
  * 返 回 值：无
  */
void Chassis_AdjustManualSpeed(int8_t Direction)
{
    int32_t NextPwm = ChassisManualOuterPwm;             // 使用宽类型计算下一档速度避免有符号溢出

    if (Direction > 0)                                  // 判断是否收到R2加速请求
    {
        NextPwm += CHASSIS_MANUAL_SPEED_STEP_PWM;       // 增加一个三百计数速度档位
    }
    else if (Direction < 0)                             // 判断是否收到L2减速请求
    {
        NextPwm -= CHASSIS_MANUAL_SPEED_STEP_PWM;       // 减少一个三百计数速度档位
    }
    if (NextPwm < CHASSIS_MANUAL_MIN_PWM)               // 判断下一档速度是否低于安全最低PWM
    {
        NextPwm = CHASSIS_MANUAL_MIN_PWM;               // 将速度限制在六百计数最低档位
    }
    if (NextPwm > CHASSIS_MANUAL_MAX_PWM)               // 判断下一档速度是否超过PWM周期允许范围
    {
        NextPwm = CHASSIS_MANUAL_MAX_PWM;               // 将速度限制在三千三百计数最高档位
    }
    Chassis_UpdateManualInnerPwm((uint16_t)NextPwm);     // 保存外侧速度并同步更新内侧速度
}

/**
  * 函    数：读取当前手动控制外侧轮PWM速度
  * 参    数：无
  * 返 回 值：当前手动控制外侧轮PWM速度
  */
uint16_t Chassis_GetManualSpeedPwm(void)
{
    return ChassisManualOuterPwm;                        // 返回当前速度档位供OLED显示和调试使用
}

/**
  * 函    数：更新四轮平滑PWM输出
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
void Chassis_Update(uint32_t NowMs)
{
    uint32_t ElapsedMs = (uint32_t)(NowMs - ChassisLastRampMs); // 计算距离上一次斜坡更新经过的时间
    uint32_t RampCount;                                  // 保存本次需要补执行的五毫秒斜坡次数
    int32_t MaximumStep;                                 // 保存本次允许改变的最大PWM总量

    if (ElapsedMs < CHASSIS_RAMP_PERIOD_MS)              // 判断是否尚未达到下一次PWM斜坡周期
    {
        return;                                          // 保持当前输出并等待五毫秒周期到达
    }
    RampCount = ElapsedMs / CHASSIS_RAMP_PERIOD_MS;      // 把经过时间换算为需要执行的斜坡次数
    if (RampCount > 10UL)                                // 判断主循环长时间阻塞后是否产生过多补偿次数
    {
        RampCount = 10UL;                                // 限制单次补偿幅度以保留平滑启停效果
    }
    ChassisLastRampMs = NowMs;                           // 记录本次PWM斜坡更新时间
    MaximumStep = (int32_t)CHASSIS_RAMP_STEP_PWM * RampCount; // 计算本次允许改变的最大PWM总量
    ChassisCurrentPwm[0] = Chassis_RampPwm(ChassisCurrentPwm[0], ChassisTargetPwm[0], MaximumStep); // 平滑更新左后轮PWM
    ChassisCurrentPwm[1] = Chassis_RampPwm(ChassisCurrentPwm[1], ChassisTargetPwm[1], MaximumStep); // 平滑更新左前轮PWM
    ChassisCurrentPwm[2] = Chassis_RampPwm(ChassisCurrentPwm[2], ChassisTargetPwm[2], MaximumStep); // 平滑更新右前轮PWM
    ChassisCurrentPwm[3] = Chassis_RampPwm(ChassisCurrentPwm[3], ChassisTargetPwm[3], MaximumStep); // 平滑更新右后轮PWM
    Set_PWM(ChassisCurrentPwm[0], ChassisCurrentPwm[1], ChassisCurrentPwm[2], ChassisCurrentPwm[3]); // 同步写入四路电机方向和PWM比较值
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
    ChassisLatestDelta[0] = Snapshot->LeftRear;          // 发布左后轮最近十毫秒编码器增量
    ChassisLatestDelta[1] = Snapshot->LeftFront;         // 发布左前轮最近十毫秒编码器增量
    ChassisLatestDelta[2] = Snapshot->RightFront;        // 发布右前轮最近十毫秒编码器增量
    ChassisLatestDelta[3] = Snapshot->RightRear;         // 发布右后轮最近十毫秒编码器增量
    ChassisAccumulatedTick[0] += Snapshot->LeftRear;     // 累加左后轮编码器计数
    ChassisAccumulatedTick[1] += Snapshot->LeftFront;    // 累加左前轮编码器计数
    ChassisAccumulatedTick[2] += Snapshot->RightFront;   // 累加右前轮编码器计数
    ChassisAccumulatedTick[3] += Snapshot->RightRear;    // 累加右后轮编码器计数
}

/**
  * 函    数：读取ROS四轮累计状态
  * 参    数：Snapshot 用于保存累计计数和最近增量的结构体地址
  * 返 回 值：无
  */
void Chassis_GetRosSnapshot(ChassisRosSnapshotTypeDef *Snapshot)
{
    uint32_t InterruptState;                              // 保存进入临界区前的全局中断状态
    if (Snapshot == 0)                                   // 判断调用者是否提供有效结果地址
    {
        return;                                          // 地址无效时不访问累计状态
    }
    InterruptState = __get_PRIMASK();                    // 保存调用前的全局中断屏蔽状态
    __disable_irq();                                     // 短暂屏蔽中断保证多字段快照一致
    Snapshot->Tick[0] = ChassisAccumulatedTick[0];       // 复制左后轮累计编码器计数
    Snapshot->Tick[1] = ChassisAccumulatedTick[1];       // 复制左前轮累计编码器计数
    Snapshot->Tick[2] = ChassisAccumulatedTick[2];       // 复制右前轮累计编码器计数
    Snapshot->Tick[3] = ChassisAccumulatedTick[3];       // 复制右后轮累计编码器计数
    Snapshot->Delta[0] = ChassisLatestDelta[0];          // 复制左后轮最近编码器增量
    Snapshot->Delta[1] = ChassisLatestDelta[1];          // 复制左前轮最近编码器增量
    Snapshot->Delta[2] = ChassisLatestDelta[2];          // 复制右前轮最近编码器增量
    Snapshot->Delta[3] = ChassisLatestDelta[3];          // 复制右后轮最近编码器增量
    if (InterruptState == 0UL)                           // 判断调用前是否允许全局中断
    {
        __enable_irq();                                  // 恢复调用前允许中断的运行状态
    }
}

/**
  * 函    数：执行ROS左右差速PWM
  * 参    数：LeftPwm 左侧两轮目标PWM，RightPwm右侧两轮目标PWM
  * 返 回 值：无
  */
void Chassis_ApplyRosPwm(int16_t LeftPwm, int16_t RightPwm)
{
    if (RosProtocol_IsControlReady() == 0U)             // 判断上电五秒IMU校零是否尚未完成
    {
        Chassis_Stop();                                  // 校零期间拒绝ROS速度目标并保持四轮停车
        return;                                          // 不写入任何非零底盘目标
    }
    Chassis_SetTargetPwm(LeftPwm, LeftPwm, RightPwm, RightPwm); // 将左右差速目标复制到同侧前后轮
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
