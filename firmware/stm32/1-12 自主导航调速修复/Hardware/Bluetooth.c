#include "stm32f10x.h"                                // 提供STM32F1固定宽度整数类型
#include "Bluetooth.h"                                // 提供蓝牙数据包与掌机命令接口声明
#include "Serial.h"                                   // 提供USART2字节收发和接收缓冲区接口
#include "AvoidanceConfig.h"                           // 提供未知距离占位值定义
#include "Timer.h"                                    // 提供蓝牙控制超时与PWM斜坡使用的毫秒时间

static volatile BluetoothControlTypeDef BluetoothControlCommand = BLUETOOTH_CONTROL_NONE; // 保存最近一次有效掌机运动命令
static volatile BluetoothModeTypeDef BluetoothMode = BLUETOOTH_MODE_NONE; // 保存掌机当前选择的工作模式
static volatile uint8_t BluetoothEmergencyStop = 1U;   // 上电默认锁存停车，必须先按B/Y/X选择模式
static BluetoothChassisTelemetryTypeDef BluetoothChassisTelemetry; // 保存底盘速度和陀螺仪遥测字段
static uint32_t BluetoothLastControlMs = 0UL;           // 保存最近一次手动运动或停车命令到达时间
static uint8_t BluetoothSpeedCommand = 0U;              // 保存最近一次L2或R2速度调整命令字节
static volatile BluetoothAvoidanceActionTypeDef BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 保存当前遥控避障接管动作

/**
  * 函    数：判断距离是否达到安全绕行要求
  * 参    数：DistanceCm 当前方向厘米距离
  * 返 回 值：非零表示距离有效且达到侧向安全距离
  */
static uint8_t Bluetooth_AvoidanceSideIsClear(uint16_t DistanceCm)
{
    if (DistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM) // 判断当前方向是否没有新鲜有效测距
    {
        return 0U;                                    // 未知距离按不安全处理
    }
    return (uint8_t)(DistanceCm >= AVOIDANCE_REMOTE_SIDE_CLEAR_CM); // 比较侧向距离是否达到可绕行阈值
}

/**
  * 函    数：选择遥控避障绕行方向
  * 参    数：Status 当前雷达四向状态快照，PreferLeft 是否优先左侧
  * 返 回 值：左转、右转或停止动作
  */
static BluetoothAvoidanceActionTypeDef Bluetooth_AvoidanceSelectTurn(const AvoidanceStatusTypeDef *Status, uint8_t PreferLeft)
{
    uint8_t LeftClear = Bluetooth_AvoidanceSideIsClear(Status->LeftDistanceCm); // 判断左侧是否达到绕行安全距离
    uint8_t RightClear = Bluetooth_AvoidanceSideIsClear(Status->RightDistanceCm); // 判断右侧是否达到绕行安全距离

    if (LeftClear != 0U && RightClear == 0U)           // 判断只有左侧可以安全绕行
    {
        return BLUETOOTH_AVOIDANCE_ACTION_TURN_LEFT;  // 选择左侧绕行
    }
    if (RightClear != 0U && LeftClear == 0U)           // 判断只有右侧可以安全绕行
    {
        return BLUETOOTH_AVOIDANCE_ACTION_TURN_RIGHT; // 选择右侧绕行
    }
    if (LeftClear == 0U && RightClear == 0U)           // 判断两侧均无法满足安全绕行距离
    {
        return BLUETOOTH_AVOIDANCE_ACTION_STOP;       // 保持停车等待空间恢复
    }
    if (Status->LeftDistanceCm > (uint16_t)(Status->RightDistanceCm + AVOIDANCE_REMOTE_CLEAR_MARGIN_CM)) // 判断左侧空间明显更大
    {
        return BLUETOOTH_AVOIDANCE_ACTION_TURN_LEFT;  // 选择空间更大的左侧绕行
    }
    if (Status->RightDistanceCm > (uint16_t)(Status->LeftDistanceCm + AVOIDANCE_REMOTE_CLEAR_MARGIN_CM)) // 判断右侧空间明显更大
    {
        return BLUETOOTH_AVOIDANCE_ACTION_TURN_RIGHT; // 选择空间更大的右侧绕行
    }
    return PreferLeft != 0U ? BLUETOOTH_AVOIDANCE_ACTION_TURN_LEFT : BLUETOOTH_AVOIDANCE_ACTION_TURN_RIGHT; // 空间接近时保持偏好方向减少抖动
}

/**
  * 函    数：判断掌机命令是否要求车体向前运动
  * 参    数：Command 当前掌机运动命令
  * 返 回 值：非零表示前进、前进左转或前进右转
  */
static uint8_t Bluetooth_AvoidanceIsForwardCommand(BluetoothControlTypeDef Command)
{
    return (uint8_t)(Command == BLUETOOTH_CONTROL_FORWARD || Command == BLUETOOTH_CONTROL_FORWARD_LEFT || Command == BLUETOOTH_CONTROL_FORWARD_RIGHT); // 返回前进类命令判断结果
}

/**
  * 函    数：判断掌机命令是否要求车体向后运动
  * 参    数：Command 当前掌机运动命令
  * 返 回 值：非零表示后退、后退左转或后退右转
  */
static uint8_t Bluetooth_AvoidanceIsBackwardCommand(BluetoothControlTypeDef Command)
{
    return (uint8_t)(Command == BLUETOOTH_CONTROL_BACKWARD || Command == BLUETOOTH_CONTROL_BACKWARD_LEFT || Command == BLUETOOTH_CONTROL_BACKWARD_RIGHT); // 返回后退类命令判断结果
}

/**
  * 函    数：判断掌机命令是否为原地旋转
  * 参    数：Command 当前掌机运动命令
  * 返 回 值：非零表示原地左转或原地右转
  */
static uint8_t Bluetooth_AvoidanceIsRotateCommand(BluetoothControlTypeDef Command)
{
    return (uint8_t)(Command == BLUETOOTH_CONTROL_TURN_LEFT || Command == BLUETOOTH_CONTROL_TURN_RIGHT); // 返回原地旋转命令判断结果
}

/**
  * 函    数：把厘米距离压缩为单字节协议字段
  * 参    数：DistanceCm 当前方向厘米距离
  * 返 回 值：零至二百五十四表示实际厘米值，二百五十五表示未知或超范围
  */
static uint8_t Bluetooth_EncodeDistance(uint16_t DistanceCm)
{
    if (DistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM || DistanceCm >= 255U) // 判断距离是否未知或超过单字节有效范围
    {
        return 0xFFU;                                  // 使用0xFF表示未知或不小于二百五十五厘米
    }
    return (uint8_t)DistanceCm;                         // 直接返回有效厘米距离值
}

/**
  * 函    数：取得蓝牙协议激光雷达状态
  * 参    数：Status 当前避障状态快照
  * 返 回 值：0x00表示异常，0x01表示工作中
  */
static BluetoothLidarStatusTypeDef Bluetooth_GetLidarStatus(const AvoidanceStatusTypeDef *Status)
{
    if (Status == 0)                                   // 判断调用者是否提供有效状态地址
    {
        return BLUETOOTH_LIDAR_ABNORMAL;               // 状态地址无效时按雷达异常处理
    }
    if (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT) // 判断雷达是否尚未就绪或已经超时
    {
        return BLUETOOTH_LIDAR_ABNORMAL;               // 返回0x00表示激光雷达异常
    }
    return BLUETOOTH_LIDAR_WORKING;                    // 返回0x01表示激光雷达工作中
}

/**
  * 函    数：取得蓝牙协议小车状态
  * 参    数：Status 当前避障状态快照
  * 返 回 值：0x00待机，0x01避障，0x02自主导航，0x03蓝牙手动控制
  */
static BluetoothCarStatusTypeDef Bluetooth_GetCarStatus(const AvoidanceStatusTypeDef *Status)
{
    if (BluetoothEmergencyStop != 0U || BluetoothMode == BLUETOOTH_MODE_NONE) // 判断是否处于上电待机或A键紧急停车锁存
    {
        return BLUETOOTH_CAR_STANDBY;                  // 返回0x00表示小车待机或紧急停车
    }
    if (BluetoothMode == BLUETOOTH_MODE_MANUAL)        // 判断掌机是否选择B键蓝牙手动模式
    {
        if (BluetoothControlCommand == BLUETOOTH_CONTROL_STOP) // 判断手动模式下是否尚未收到运动命令或已经停车
        {
            return BLUETOOTH_CAR_STANDBY;              // 返回0x00表示手动模式待机
        }
        return BLUETOOTH_CAR_MANUAL;                   // 返回0x03表示蓝牙手动控制中
    }
    if (BluetoothMode == BLUETOOTH_MODE_AUTONOMOUS)     // 判断掌机是否选择X键自主导航模式
    {
        return BLUETOOTH_CAR_AUTONOMOUS;               // 返回0x02表示自主导航模式
    }
    if (Status != 0 && (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT)) // 判断避障模式下雷达是否异常
    {
        return BLUETOOTH_CAR_STANDBY;                  // 雷达异常时保持安全待机状态
    }
    return BLUETOOTH_CAR_AVOIDING;                     // 返回0x01表示实时避障模式
}

/**
  * 函    数：判断是否为掌机运动命令
  * 参    数：ReceivedByte 待判断的串口字节
  * 返 回 值：非零表示属于0x41至0x49运动控制范围
  */
static uint8_t Bluetooth_IsMotionCommand(uint8_t ReceivedByte)
{
    if (ReceivedByte >= CHASSIS_COMMAND_FORWARD && ReceivedByte <= CHASSIS_COMMAND_STOP) // 判断字节是否处于连续的掌机运动命令范围
    {
        return 1U;                                      // 返回非零表示属于有效运动命令
    }
    return 0U;                                          // 返回零表示不是运动命令
}

/**
  * 函    数：蓝牙协议状态和底盘初始化
  * 参    数：无
  * 返 回 值：无
  */
#include "Battery.h"                                  // 提供电池电压和百分比接口
#define BLUETOOTH_BATTERY_HEADER_SECOND 0x53U
#define BLUETOOTH_BATTERY_PACKET_LENGTH 5U
void Bluetooth_SendBatteryTelemetry(void);             // 前置声明电池电量数据包发送函数

void Bluetooth_Init(void)
{
    Battery_Init();                                    // 初始化PC4电池分压ADC采样
    BluetoothControlCommand = BLUETOOTH_CONTROL_STOP;  // 上电默认运动命令设为停车
    BluetoothMode = BLUETOOTH_MODE_NONE;               // 上电默认不允许任何控制模式驱动电机
    BluetoothEmergencyStop = 1U;                       // 上电锁存安全停车，等待掌机模式键释放
    BluetoothChassisTelemetry.LeftRearSpeed = 0U;      // 初始化左后轮速度字段
    BluetoothChassisTelemetry.LeftFrontSpeed = 0U;     // 初始化左前轮速度字段
    BluetoothChassisTelemetry.RightFrontSpeed = 0U;    // 初始化右前轮速度字段
    BluetoothChassisTelemetry.RightRearSpeed = 0U;     // 初始化右后轮速度字段
    BluetoothChassisTelemetry.GyroscopeAngle = 0U;     // 初始化陀螺仪角度预留字段
    BluetoothLastControlMs = 0UL;                      // 清除手动控制命令接收时间
    BluetoothSpeedCommand = 0U;                         // 清除上电时的速度调整命令显示
    BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 清除上电时的遥控避障动作
    Chassis_Init();                                    // 初始化TIM8四路PWM、TIM2至TIM5编码器和方向引脚
}

/**
  * 函    数：解析掌机模式和运动控制数据
  * 参    数：无
  * 返 回 值：无
  */
void Bluetooth_ProcessReceivedData(void)
{
    uint8_t ReceivedByte;                               // 保存从USART2环形缓冲区读取的当前掌机字节
    uint32_t CurrentTimeMs = Timer_GetMs();             // 读取本轮控制解析和失联保护共用的毫秒时间

    while (Serial_ReadByte(&ReceivedByte) != 0U)       // 依次处理USART2已经接收到的全部掌机字节
    {
        if (ReceivedByte == (uint8_t)BLUETOOTH_MODE_MANUAL || ReceivedByte == (uint8_t)BLUETOOTH_MODE_AVOIDANCE || ReceivedByte == (uint8_t)BLUETOOTH_MODE_AUTONOMOUS) // 判断是否收到B/Y/X模式键
        {
            if (BluetoothMode != (BluetoothModeTypeDef)ReceivedByte) // 判断掌机是否确实切换到不同工作模式
            {
                BluetoothMode = (BluetoothModeTypeDef)ReceivedByte; // 保存掌机选择的新工作模式
                BluetoothControlCommand = BLUETOOTH_CONTROL_STOP; // 清除上一模式遗留的运动命令
                BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 清除上一模式遗留的避障接管动作
                Chassis_Stop();                         // 模式切换瞬间立即停车并等待新模式接管
            }
            BluetoothEmergencyStop = 0U;               // 模式键作为重新授权控制的安全释放动作
            BluetoothLastControlMs = CurrentTimeMs;    // 记录模式授权字节到达时间
        }
        else if (ReceivedByte == CHASSIS_COMMAND_SPEED_DOWN || ReceivedByte == CHASSIS_COMMAND_SPEED_UP) // 判断是否收到L2减速或R2加速命令
        {
            if (BluetoothMode == BLUETOOTH_MODE_MANUAL || BluetoothMode == BLUETOOTH_MODE_AVOIDANCE || BluetoothMode == BLUETOOTH_MODE_AUTONOMOUS) // 允许蓝牙手动、遥控避障和自主模式调整底盘速度
            {
                BluetoothSpeedCommand = ReceivedByte;   // 保存本次速度调整字节供OLED显示
                BluetoothLastControlMs = CurrentTimeMs; // 记录速度调整命令到达时间保持链路活跃
                Chassis_AdjustManualSpeed(ReceivedByte == CHASSIS_COMMAND_SPEED_UP ? 1 : -1); // 按0x6B加速或0x6A减速一个档位
                if (BluetoothControlCommand >= BLUETOOTH_CONTROL_FORWARD && BluetoothControlCommand <= BLUETOOTH_CONTROL_FORWARD_LEFT) // 判断当前是否存在正在执行的方向命令
                {
                    Chassis_ApplyBluetoothCommand((uint8_t)BluetoothControlCommand); // 立即按新速度刷新当前方向目标PWM
                }
                else if (BluetoothMode == BLUETOOTH_MODE_AUTONOMOUS) // 自主模式切换后命令缓存为停车，调速时直接刷新自主前进目标
                {
                    Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_FORWARD); // 立即把新的速度档位应用到自主前进目标
                }
            }
        }
        else if (ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_STOP) // 判断是否收到A键紧急停车或普通停车命令
        {
            BluetoothControlCommand = BLUETOOTH_CONTROL_STOP; // 保存0x49停车命令供遥测状态使用
            BluetoothEmergencyStop = 1U;               // 标记当前处于停车状态并供遥测显示
            BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 清除当前避障转向动作
            BluetoothLastControlMs = CurrentTimeMs;    // 记录停车字节到达时间
            Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_STOP); // 立即向四路电机发送停车命令
        }
        else if (Bluetooth_IsMotionCommand(ReceivedByte) != 0U) // 判断是否收到0x41至0x48手动运动命令
        {
            if (BluetoothControlCommand != (BluetoothControlTypeDef)ReceivedByte) // 判断掌机是否真正改变了期望运动方向
            {
                BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 新方向到达时清除旧方向的避障接管状态
            }
            BluetoothControlCommand = (BluetoothControlTypeDef)ReceivedByte; // 保存最近一次有效运动命令
            if (BluetoothMode == BLUETOOTH_MODE_MANUAL) // 仅允许B键手动模式接收掌机方向控制
            {
                BluetoothEmergencyStop = 0U;           // 合法方向命令自动解除0x49停车状态以兼容掌机41 41 49握手
                BluetoothLastControlMs = CurrentTimeMs; // 记录最近一次有效手动方向命令时间
                Chassis_ApplyBluetoothCommand(ReceivedByte); // 将掌机字节转换为四轮差速PWM输出
            }
            else if (BluetoothMode == BLUETOOTH_MODE_AVOIDANCE) // 允许0x5C模式保存掌机期望方向并交给雷达仲裁层接管
            {
                BluetoothEmergencyStop = 0U;           // 合法方向命令解除停车状态并允许避障层输出
                BluetoothLastControlMs = CurrentTimeMs; // 记录最近一次有效避障期望方向
            }
            else                                            // 处理自主或紧急锁存状态收到手动运动字节的情况
            {
                Chassis_Stop();                             // 非手动模式始终安全停车，等待后续模式适配层接管
            }
        }
    }

    if ((BluetoothMode == BLUETOOTH_MODE_MANUAL || BluetoothMode == BLUETOOTH_MODE_AVOIDANCE) && BluetoothControlCommand != BLUETOOTH_CONTROL_STOP && (uint32_t)(CurrentTimeMs - BluetoothLastControlMs) > BLUETOOTH_CONTROL_TIMEOUT_MS) // 判断蓝牙控制链路是否超过安全接收时限
    {
        BluetoothControlCommand = BLUETOOTH_CONTROL_STOP; // 把遥测命令状态更新为停车
        BluetoothEmergencyStop = 1U;                    // 标记蓝牙失联保护已经停车
        BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 清除失联前的避障接管动作
        Chassis_Stop();                                 // 立即关闭四轮输出防止断联后持续行驶
    }
    Chassis_Update(CurrentTimeMs);                      // 按五毫秒斜坡平滑推进四路手动PWM输出
}

/**
  * 函    数：按雷达状态仲裁蓝牙遥控实际底盘动作
  * 参    数：Status 当前雷达四向状态快照，NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
void Bluetooth_UpdateAvoidanceControl(const AvoidanceStatusTypeDef *Status, uint32_t NowMs)
{
    BluetoothControlTypeDef RequestedCommand = BluetoothControlCommand; // 保存掌机持续发送的期望运动命令
    uint8_t PreferLeft = 0U;                             // 保存两侧空间接近时的默认绕行偏好
    uint8_t IsForward;                                   // 保存当前命令是否属于前进类
    uint8_t IsBackward;                                  // 保存当前命令是否属于后退类
    uint8_t IsRotate;                                    // 保存当前命令是否属于原地旋转类

    (void)NowMs;                                         // 明确当前仲裁只使用状态年龄而不直接使用毫秒参数
    if (BluetoothMode != BLUETOOTH_MODE_AVOIDANCE || Status == 0) // 判断是否处于有效遥控避障模式
    {
        BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 非遥控避障模式清除动作状态
        if (BluetoothMode != BLUETOOTH_MODE_MANUAL)      // 判断是否需要为自主和待机模式保持停车
        {
            Chassis_Stop();                             // 自主逻辑接入前不允许本层驱动电机
        }
        return;                                          // 结束无效模式的避障仲裁
    }
    if (BluetoothEmergencyStop != 0U || RequestedCommand == BLUETOOTH_CONTROL_STOP) // 判断是否处于急停或没有掌机运动意图
    {
        BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 清除避障动作缓存
        Chassis_Stop();                                  // 急停或停车意图优先于所有雷达绕行动作
        return;                                          // 结束本次安全停车处理
    }
    if (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT || Status->DataAgeMs > AVOIDANCE_SENSOR_TIMEOUT_MS) // 判断雷达是否未就绪、故障或数据过期
    {
        BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_STOP; // 标记雷达不可用导致的安全停车动作
        Chassis_Stop();                                  // 雷达失效时禁止遥控车辆继续运动
        return;                                          // 结束本次无有效传感器数据处理
    }

    IsForward = Bluetooth_AvoidanceIsForwardCommand(RequestedCommand); // 识别前进类期望指令
    IsBackward = Bluetooth_AvoidanceIsBackwardCommand(RequestedCommand); // 识别后退类期望指令
    IsRotate = Bluetooth_AvoidanceIsRotateCommand(RequestedCommand); // 识别原地旋转类期望指令
    PreferLeft = (uint8_t)(RequestedCommand == BLUETOOTH_CONTROL_FORWARD_LEFT || RequestedCommand == BLUETOOTH_CONTROL_BACKWARD_LEFT || RequestedCommand == BLUETOOTH_CONTROL_TURN_LEFT); // 根据掌机当前方向建立空间接近时的偏好

    if (IsForward != 0U)                              // 处理掌机要求向前运动的情况
    {
        if (Status->FrontDistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM) // 判断前方扇区是否没有有效距离
        {
            BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_STOP; // 未知前方按不安全处理
            Chassis_Stop();                              // 未知前方距离时保持停车
            return;                                      // 结束本次前进仲裁
        }
        if (BluetoothAvoidanceAction != BLUETOOTH_AVOIDANCE_ACTION_NONE && Status->FrontDistanceCm >= AVOIDANCE_REMOTE_FRONT_RELEASE_CM) // 判断前方是否已恢复足够释放距离
        {
            BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 释放已经完成的绕行接管
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_NONE && Status->FrontDistanceCm <= AVOIDANCE_REMOTE_FRONT_TRIGGER_CM) // 判断前方是否首次进入转向区
        {
            BluetoothAvoidanceAction = Bluetooth_AvoidanceSelectTurn(Status, PreferLeft); // 按左右可用空间选择局部绕行方向
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_LEFT) // 判断当前是否执行左侧绕行
        {
            if (Bluetooth_AvoidanceSideIsClear(Status->LeftDistanceCm) == 0U) // 判断左侧绕行通道是否已经被新障碍物占用
            {
                BluetoothAvoidanceAction = Bluetooth_AvoidanceSelectTurn(Status, 0U); // 左侧受阻时重新选择右侧或停车
            }
            if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_LEFT) // 判断重新选择后是否仍可向左绕行
            {
                Chassis_ApplyBluetoothCommand(Status->FrontDistanceCm <= AVOIDANCE_REMOTE_EMERGENCY_CM ? CHASSIS_COMMAND_ROTATE_LEFT : CHASSIS_COMMAND_FORWARD_LEFT); // 近距离原地左转，普通距离左弧线绕行
                return;                                  // 完成左侧前方避障输出
            }
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_RIGHT) // 判断当前是否执行右侧绕行
        {
            if (Bluetooth_AvoidanceSideIsClear(Status->RightDistanceCm) == 0U) // 判断右侧绕行通道是否已经被新障碍物占用
            {
                BluetoothAvoidanceAction = Bluetooth_AvoidanceSelectTurn(Status, 1U); // 右侧受阻时重新选择左侧或停车
            }
            if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_RIGHT) // 判断重新选择后是否仍可向右绕行
            {
                Chassis_ApplyBluetoothCommand(Status->FrontDistanceCm <= AVOIDANCE_REMOTE_EMERGENCY_CM ? CHASSIS_COMMAND_ROTATE_RIGHT : CHASSIS_COMMAND_FORWARD_RIGHT); // 近距离原地右转，普通距离右弧线绕行
                return;                                  // 完成右侧前方避障输出
            }
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_STOP) // 判断前方和两侧均无法安全绕行
        {
            Chassis_Stop();                              // 保持停车直到下一次雷达更新发现可用空间
            return;                                      // 完成本次前方安全停车输出
        }
        if (RequestedCommand == BLUETOOTH_CONTROL_FORWARD_LEFT && Status->LeftDistanceCm <= AVOIDANCE_REMOTE_SIDE_BLOCK_CM) // 判断前进左转指令的左侧是否受阻
        {
            Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_FORWARD); // 左侧受阻时暂时改为直行保持车体距离
        }
        else if (RequestedCommand == BLUETOOTH_CONTROL_FORWARD_RIGHT && Status->RightDistanceCm <= AVOIDANCE_REMOTE_SIDE_BLOCK_CM) // 判断前进右转指令的右侧是否受阻
        {
            Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_FORWARD); // 右侧受阻时暂时改为直行保持车体距离
        }
        else
        {
            Chassis_ApplyBluetoothCommand((uint8_t)RequestedCommand); // 前方安全时执行掌机原始前进意图
        }
        return;                                          // 完成前进类遥控避障输出
    }

    if (IsBackward != 0U)                             // 处理掌机要求向后运动的情况
    {
        if (Status->RearDistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM) // 判断后方扇区是否没有有效距离
        {
            BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_STOP; // 未知后方按不安全处理
            Chassis_Stop();                              // 未知后方距离时保持停车
            return;                                      // 结束本次后退仲裁
        }
        if (BluetoothAvoidanceAction != BLUETOOTH_AVOIDANCE_ACTION_NONE && Status->RearDistanceCm >= AVOIDANCE_REMOTE_FRONT_RELEASE_CM) // 判断后方是否已恢复足够释放距离
        {
            BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 释放已经完成的后退绕行接管
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_NONE && Status->RearDistanceCm <= AVOIDANCE_REMOTE_FRONT_TRIGGER_CM) // 判断后方是否首次进入转向区
        {
            BluetoothAvoidanceAction = Bluetooth_AvoidanceSelectTurn(Status, PreferLeft); // 按左右可用空间选择后退绕行方向
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_LEFT) // 判断当前是否执行左侧后退绕行
        {
            if (Bluetooth_AvoidanceSideIsClear(Status->LeftDistanceCm) == 0U) // 判断左侧后退绕行通道是否受阻
            {
                BluetoothAvoidanceAction = Bluetooth_AvoidanceSelectTurn(Status, 0U); // 左侧受阻时重新选择右侧或停车
            }
            if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_LEFT) // 判断重新选择后是否仍可向左后退绕行
            {
                Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_BACKWARD_LEFT); // 使用左侧后退弧线避开后方障碍
                return;                                  // 完成左侧后退避障输出
            }
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_RIGHT) // 判断当前是否执行右侧后退绕行
        {
            if (Bluetooth_AvoidanceSideIsClear(Status->RightDistanceCm) == 0U) // 判断右侧后退绕行通道是否受阻
            {
                BluetoothAvoidanceAction = Bluetooth_AvoidanceSelectTurn(Status, 1U); // 右侧受阻时重新选择左侧或停车
            }
            if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_TURN_RIGHT) // 判断重新选择后是否仍可向右后退绕行
            {
                Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_BACKWARD_RIGHT); // 使用右侧后退弧线避开后方障碍
                return;                                  // 完成右侧后退避障输出
            }
        }
        if (BluetoothAvoidanceAction == BLUETOOTH_AVOIDANCE_ACTION_STOP) // 判断后方和两侧均无法安全绕行
        {
            Chassis_Stop();                              // 保持停车直到后方空间恢复
            return;                                      // 完成本次后退安全停车输出
        }
        Chassis_ApplyBluetoothCommand((uint8_t)RequestedCommand); // 后方安全时执行掌机原始后退意图
        return;                                          // 完成后退类遥控避障输出
    }

    if (IsRotate != 0U)                               // 处理掌机要求原地旋转的情况
    {
        if (RequestedCommand == BLUETOOTH_CONTROL_TURN_LEFT && Status->LeftDistanceCm <= AVOIDANCE_REMOTE_SIDE_BLOCK_CM) // 判断原地左转扫掠侧是否受阻
        {
            if (Bluetooth_AvoidanceSideIsClear(Status->RightDistanceCm) != 0U) // 判断反向旋转侧是否存在安全空间
            {
                Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_ROTATE_RIGHT); // 左侧受阻时改向右侧原地旋转
            }
            else
            {
                BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_STOP; // 两侧均受阻时标记停车动作
                Chassis_Stop();                          // 两侧均受阻时保持安全停车
            }
        }
        else if (RequestedCommand == BLUETOOTH_CONTROL_TURN_RIGHT && Status->RightDistanceCm <= AVOIDANCE_REMOTE_SIDE_BLOCK_CM) // 判断原地右转扫掠侧是否受阻
        {
            if (Bluetooth_AvoidanceSideIsClear(Status->LeftDistanceCm) != 0U) // 判断反向旋转侧是否存在安全空间
            {
                Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_ROTATE_LEFT); // 右侧受阻时改向左侧原地旋转
            }
            else
            {
                BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_STOP; // 两侧均受阻时标记停车动作
                Chassis_Stop();                          // 两侧均受阻时保持安全停车
            }
        }
        else
        {
            Chassis_ApplyBluetoothCommand((uint8_t)RequestedCommand); // 旋转扫掠侧安全时执行掌机原始旋转意图
        }
        return;                                          // 完成原地旋转遥控避障输出
    }

    BluetoothAvoidanceAction = BLUETOOTH_AVOIDANCE_ACTION_NONE; // 未识别运动意图时清除避障接管动作
    Chassis_Stop();                                      // 未识别命令按安全策略停车
}

/**
  * 函    数：发送雷达距离和小车状态数据包
  * 参    数：Status 当前避障状态快照
  * 返 回 值：无
  */
void Bluetooth_SendTelemetry(const AvoidanceStatusTypeDef *Status)
{
    uint8_t Packet[BLUETOOTH_PACKET_LENGTH];             // 保存准备发送的九字节雷达状态数据包

    if (Status == 0)                                   // 判断调用者是否提供有效状态快照
    {
        return;                                         // 状态无效时不发送不完整数据包
    }
    Packet[0] = BLUETOOTH_PACKET_HEADER_FIRST;          // 写入第一个固定包头0x55
    Packet[1] = BLUETOOTH_PACKET_HEADER_SECOND;         // 写入第二个固定包头0x51
    Packet[2] = Bluetooth_EncodeDistance(Status->FrontDistanceCm); // 写入0x01前方厘米距离
    Packet[3] = Bluetooth_EncodeDistance(Status->RearDistanceCm); // 写入0x02后方厘米距离
    Packet[4] = Bluetooth_EncodeDistance(Status->LeftDistanceCm); // 写入0x03左侧厘米距离
    Packet[5] = Bluetooth_EncodeDistance(Status->RightDistanceCm); // 写入0x04右侧厘米距离
    Packet[6] = (uint8_t)Bluetooth_GetLidarStatus(Status); // 写入0x05激光雷达工作状态
    Packet[7] = (uint8_t)Bluetooth_GetCarStatus(Status); // 写入0x06小车工作状态
    Packet[8] = BLUETOOTH_PACKET_TAIL;                  // 写入固定包尾0x5B
    Serial_SendArray(Packet, BLUETOOTH_PACKET_LENGTH);  // 通过USART2向蓝牙发送完整雷达状态数据包
    Battery_Update();                                   // 在同一发送节拍更新一次电池电压和百分比
    Bluetooth_SendBatteryTelemetry();                   // 通过独立0x53数据包发送电池遥测
}

/**
  * 函数：发送电池电压和百分比数据包
  * 参数：无
  * 返回值：无
  */
void Bluetooth_SendBatteryTelemetry(void)
{
    uint8_t Packet[BLUETOOTH_BATTERY_PACKET_LENGTH];    // 保存五字节电池电量数据包

    Packet[0] = BLUETOOTH_PACKET_HEADER_FIRST;          // 写入固定包头第一个字节0x55
    Packet[1] = BLUETOOTH_BATTERY_HEADER_SECOND;        // 写入电池包第二个包头0x53
    Packet[2] = Battery_GetVoltageDeciV();               // 写入转换后的电池电压，单位0.1伏
    Packet[3] = Battery_GetPercent();                   // 写入转换后的电量百分比，范围0至100
    Packet[4] = BLUETOOTH_PACKET_TAIL;                  // 写入固定包尾0x5B
    Serial_SendArray(Packet, BLUETOOTH_BATTERY_PACKET_LENGTH); // 通过USART2向蓝牙发送电池电量包
}

/**
  * 函    数：更新底盘遥测兼容缓存
  * 参    数：LeftRearSpeed至RightRearSpeed 四轮协议速度字段，GyroscopeAngle陀螺仪角度字段
  * 返 回 值：无
  */
void Bluetooth_SetChassisTelemetry(uint8_t LeftRearSpeed, uint8_t LeftFrontSpeed, uint8_t RightFrontSpeed, uint8_t RightRearSpeed, uint8_t GyroscopeAngle)
{
    BluetoothChassisTelemetry.LeftRearSpeed = LeftRearSpeed; // 更新左后轮协议速度缓存
    BluetoothChassisTelemetry.LeftFrontSpeed = LeftFrontSpeed; // 更新左前轮协议速度缓存
    BluetoothChassisTelemetry.RightFrontSpeed = RightFrontSpeed; // 更新右前轮协议速度缓存
    BluetoothChassisTelemetry.RightRearSpeed = RightRearSpeed; // 更新右后轮协议速度缓存
    BluetoothChassisTelemetry.GyroscopeAngle = GyroscopeAngle; // 更新陀螺仪角度协议缓存
}

/**
  * 函    数：读取编码器并发送底盘速度数据包
  * 参    数：无
  * 返 回 值：无
  */
void Bluetooth_SendChassisTelemetry(void)
{
    uint8_t Packet[BLUETOOTH_CHASSIS_PACKET_LENGTH];    // 保存准备发送的八字节底盘遥测数据包
    ChassisSpeedSnapshotTypeDef SpeedSnapshot;          // 保存本次十毫秒窗口读取的四路编码器增量

    Chassis_GetSpeedSnapshot(&SpeedSnapshot);           // 读取并清零四个编码器计数器
    BluetoothChassisTelemetry.LeftRearSpeed = Chassis_EncodeSpeed(SpeedSnapshot.LeftRear); // 将左后轮有符号增量压缩为int8二补码
    BluetoothChassisTelemetry.LeftFrontSpeed = Chassis_EncodeSpeed(SpeedSnapshot.LeftFront); // 将左前轮有符号增量压缩为int8二补码
    BluetoothChassisTelemetry.RightFrontSpeed = Chassis_EncodeSpeed(SpeedSnapshot.RightFront); // 将右前轮有符号增量压缩为int8二补码
    BluetoothChassisTelemetry.RightRearSpeed = Chassis_EncodeSpeed(SpeedSnapshot.RightRear); // 将右后轮有符号增量压缩为int8二补码
    Packet[0] = BLUETOOTH_PACKET_HEADER_FIRST;          // 写入底盘数据包第一个包头0x55
    Packet[1] = BLUETOOTH_CHASSIS_HEADER_SECOND;        // 写入底盘数据包第二个包头0x52
    Packet[2] = BluetoothChassisTelemetry.LeftRearSpeed; // 写入0x01左后轮速度字段
    Packet[3] = BluetoothChassisTelemetry.LeftFrontSpeed; // 写入0x02左前轮速度字段
    Packet[4] = BluetoothChassisTelemetry.RightFrontSpeed; // 写入0x03右前轮速度字段
    Packet[5] = BluetoothChassisTelemetry.RightRearSpeed; // 写入0x04右后轮速度字段
    Packet[6] = BluetoothChassisTelemetry.GyroscopeAngle; // 写入0x05陀螺仪角度预留字段
    Packet[7] = BLUETOOTH_PACKET_TAIL;                  // 写入底盘数据包固定包尾0x5B
    Serial_SendArray(Packet, BLUETOOTH_CHASSIS_PACKET_LENGTH); // 通过USART2向蓝牙发送完整底盘遥测数据包
}

/**
  * 函    数：取得最近一次底盘遥测字段
  * 参    数：Telemetry 用于保存四轮速度和陀螺仪字段的结构体地址
  * 返 回 值：无
  */
void Bluetooth_GetChassisTelemetry(BluetoothChassisTelemetryTypeDef *Telemetry)
{
    if (Telemetry == 0)                                // 判断调用者是否提供有效的结果保存地址
    {
        return;                                         // 地址无效时不访问遥测缓存
    }
    *Telemetry = BluetoothChassisTelemetry;             // 复制最近一次已经通过蓝牙发送的完整底盘字段
}

/**
  * 函    数：取得最近一次速度调整命令
  * 参    数：无
  * 返 回 值：0x6A减速、0x6B加速或零表示尚未调整
  */
uint8_t Bluetooth_GetSpeedCommand(void)
{
    return BluetoothSpeedCommand;                       // 返回最近一次L2或R2命令字节供OLED显示
}

/**
  * 函    数：取得当前遥控避障实际动作
  * 参    数：无
  * 返 回 值：当前跟随、左绕、右绕或停车动作
  */
BluetoothAvoidanceActionTypeDef Bluetooth_GetAvoidanceAction(void)
{
    return BluetoothAvoidanceAction;                    // 返回当前雷达仲裁层的实际动作状态
}

/**
  * 函    数：取得最近一次掌机运动命令
  * 参    数：无
  * 返 回 值：最近一次有效的0x41至0x49命令
  */
BluetoothControlTypeDef Bluetooth_GetControlCommand(void)
{
    return BluetoothControlCommand;                     // 返回当前掌机运动命令缓存
}

/**
  * 函    数：取得当前掌机工作模式
  * 参    数：无
  * 返 回 值：0x5B手动、0x5C避障、0x5D自主或0x00待选择
  */
BluetoothModeTypeDef Bluetooth_GetMode(void)
{
    return BluetoothMode;                               // 返回当前模式缓存
}

/**
  * 函    数：取得紧急停车锁存状态
  * 参    数：无
  * 返 回 值：非零表示A键已经锁存停车
  */
uint8_t Bluetooth_IsEmergencyStopped(void)
{
    return BluetoothEmergencyStop;                      // 返回当前紧急停车锁存标志
}
