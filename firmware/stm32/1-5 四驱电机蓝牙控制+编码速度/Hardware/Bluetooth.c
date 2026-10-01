#include "stm32f10x.h"                                // 提供STM32F1固定宽度整数类型
#include "Bluetooth.h"                                // 提供蓝牙数据包与掌机命令接口声明
#include "Serial.h"                                   // 提供USART2字节收发和接收缓冲区接口
#include "AvoidanceConfig.h"                           // 提供未知距离占位值定义

static volatile BluetoothControlTypeDef BluetoothControlCommand = BLUETOOTH_CONTROL_NONE; // 保存最近一次有效掌机运动命令
static volatile BluetoothModeTypeDef BluetoothMode = BLUETOOTH_MODE_NONE; // 保存掌机当前选择的工作模式
static volatile uint8_t BluetoothEmergencyStop = 1U;   // 上电默认锁存停车，必须先按B/Y/X选择模式
static BluetoothChassisTelemetryTypeDef BluetoothChassisTelemetry; // 保存底盘速度和陀螺仪遥测字段

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
void Bluetooth_Init(void)
{
    BluetoothControlCommand = BLUETOOTH_CONTROL_STOP;  // 上电默认运动命令设为停车
    BluetoothMode = BLUETOOTH_MODE_NONE;               // 上电默认不允许任何控制模式驱动电机
    BluetoothEmergencyStop = 1U;                       // 上电锁存安全停车，等待掌机模式键释放
    BluetoothChassisTelemetry.LeftRearSpeed = 0U;      // 初始化左后轮速度字段
    BluetoothChassisTelemetry.LeftFrontSpeed = 0U;     // 初始化左前轮速度字段
    BluetoothChassisTelemetry.RightFrontSpeed = 0U;    // 初始化右前轮速度字段
    BluetoothChassisTelemetry.RightRearSpeed = 0U;     // 初始化右后轮速度字段
    BluetoothChassisTelemetry.GyroscopeAngle = 0U;     // 初始化陀螺仪角度预留字段
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

    while (Serial_ReadByte(&ReceivedByte) != 0U)       // 依次处理USART2已经接收到的全部掌机字节
    {
        if (ReceivedByte == (uint8_t)BLUETOOTH_MODE_MANUAL || ReceivedByte == (uint8_t)BLUETOOTH_MODE_AVOIDANCE || ReceivedByte == (uint8_t)BLUETOOTH_MODE_AUTONOMOUS) // 判断是否收到B/Y/X模式键
        {
            BluetoothMode = (BluetoothModeTypeDef)ReceivedByte; // 保存掌机选择的工作模式
            BluetoothEmergencyStop = 0U;               // 模式键作为重新授权控制的安全释放动作
            BluetoothControlCommand = BLUETOOTH_CONTROL_STOP; // 切换模式时先清除上一模式运动命令
            Chassis_Stop();                             // 切换模式瞬间确保四轮停车
        }
        else if (ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_STOP) // 判断是否收到A键紧急停车或普通停车命令
        {
            BluetoothControlCommand = BLUETOOTH_CONTROL_STOP; // 保存0x49停车命令供遥测状态使用
            BluetoothEmergencyStop = 1U;               // 锁存紧急停车并关闭后续手动运动控制
            Chassis_ApplyBluetoothCommand(CHASSIS_COMMAND_STOP); // 立即向四路电机发送停车命令
        }
        else if (Bluetooth_IsMotionCommand(ReceivedByte) != 0U) // 判断是否收到0x41至0x48手动运动命令
        {
            BluetoothControlCommand = (BluetoothControlTypeDef)ReceivedByte; // 保存最近一次有效运动命令
            if (BluetoothMode == BLUETOOTH_MODE_MANUAL && BluetoothEmergencyStop == 0U) // 仅允许B键手动模式且未紧急停车时驱动电机
            {
                Chassis_ApplyBluetoothCommand(ReceivedByte); // 将掌机字节转换为四轮差速PWM输出
            }
            else                                            // 处理避障、自主或紧急锁存状态收到手动运动字节的情况
            {
                Chassis_Stop();                             // 非手动模式始终安全停车，等待后续模式适配层接管
            }
        }
    }
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
