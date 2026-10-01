#include "stm32f10x.h"                  //提供STM32F10x固定宽度整数类型
#include "Bluetooth.h"                  //提供蓝牙数据包与控制命令接口声明
#include "Serial.h"                     //提供USART2字节收发和接收缓冲区接口
#include "AvoidanceConfig.h"            //提供未知距离占位值定义

static volatile BluetoothControlTypeDef BluetoothControlCommand = BLUETOOTH_CONTROL_NONE;  //保存手机APP最近一次有效控制命令
static volatile uint8_t BluetoothManualMode = 0U;                                           //标记手机是否已经请求进入手动控制模式
static BluetoothChassisTelemetryTypeDef BluetoothChassisTelemetry;                          //保存等待十毫秒发送的四电机速度和陀螺仪角度原始值

/**
  * 函    数：把厘米距离压缩为单字节协议字段
  * 参    数：DistanceCm 当前方向厘米距离
  * 返 回 值：零至二百五十四表示实际厘米值，二百五十五表示未知或不小于二百五十五厘米
  */
static uint8_t Bluetooth_EncodeDistance(uint16_t DistanceCm)
{
    if (DistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM || DistanceCm >= 255U)                  //判断距离是否未知或超过单字节有效范围
    {
        return 0xFFU;                                                                        //使用0xFF表示未知或不小于二百五十五厘米
    }
    return (uint8_t)DistanceCm;                                                              //直接返回零至二百五十四厘米距离值
}

/**
  * 函    数：取得蓝牙协议激光雷达状态
  * 参    数：Status 当前避障状态快照
  * 返 回 值：0x00表示异常，0x01表示工作中
  */
static BluetoothLidarStatusTypeDef Bluetooth_GetLidarStatus(const AvoidanceStatusTypeDef *Status)
{
    if (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT) //判断雷达数据是否尚未就绪或已经超时
    {
        return BLUETOOTH_LIDAR_ABNORMAL;                                                     //返回0x00表示激光雷达异常
    }
    return BLUETOOTH_LIDAR_WORKING;                                                          //返回0x01表示激光雷达工作中
}

/**
  * 函    数：取得蓝牙协议小车状态
  * 参    数：Status 当前避障状态快照
  * 返 回 值：0x00待机，0x01避障，0x02自主导航，0x03蓝牙手动控制
  */
static BluetoothCarStatusTypeDef Bluetooth_GetCarStatus(const AvoidanceStatusTypeDef *Status)
{
    if (BluetoothManualMode != 0U)                                                          //判断手机是否已经发送手动运动或停车命令
    {
        if (BluetoothControlCommand == BLUETOOTH_CONTROL_STOP)                              //判断手机当前是否请求停车
        {
            return BLUETOOTH_CAR_STANDBY;                                                   //返回0x00表示蓝牙手动停车待机
        }
        return BLUETOOTH_CAR_MANUAL;                                                        //返回0x03表示蓝牙手动控制中
    }
    if (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT) //判断车辆是否因雷达无效保持安全停车
    {
        return BLUETOOTH_CAR_STANDBY;                                                       //返回0x00表示小车待机中
    }
    if (Status->State == AVOIDANCE_STATE_CRUISE)                                            //判断避障核心是否允许正常自主巡航
    {
        return BLUETOOTH_CAR_AUTONOMOUS;                                                    //返回0x02表示自主导航中
    }
    return BLUETOOTH_CAR_AVOIDING;                                                          //返回0x01表示正在执行避障或紧急停车
}

/**
  * 函    数：蓝牙协议状态初始化
  * 参    数：无
  * 返 回 值：无
  */
void Bluetooth_Init(void)
{
    BluetoothControlCommand = BLUETOOTH_CONTROL_NONE;                                      //清除上电前不存在的手机控制命令
    BluetoothManualMode = 0U;                                                               //默认由自主避障状态机决定小车状态
    BluetoothChassisTelemetry.LeftRearSpeed = 0U;                                           //电机尚未接入时左后速度字段发送0x00
    BluetoothChassisTelemetry.LeftFrontSpeed = 0U;                                          //电机尚未接入时左前速度字段发送0x00
    BluetoothChassisTelemetry.RightFrontSpeed = 0U;                                         //电机尚未接入时右前速度字段发送0x00
    BluetoothChassisTelemetry.RightRearSpeed = 0U;                                          //电机尚未接入时右后速度字段发送0x00
    BluetoothChassisTelemetry.GyroscopeAngle = 0U;                                          //陀螺仪尚未接入时角度字段发送0x00
}

/**
  * 函    数：解析手机APP控制数据
  * 参    数：无
  * 返 回 值：无
  */
void Bluetooth_ProcessReceivedData(void)
{
    uint8_t ReceivedByte;                                                                    //保存从USART2环形缓冲区读取的当前控制字节

    while (Serial_ReadByte(&ReceivedByte) != 0U)                                            //依次处理USART2已经收到的全部手机数据
    {
        if (ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_FORWARD ||                           //判断是否收到0x41前进命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_FORWARD_RIGHT ||                     //判断是否收到0x42前进右转命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_TURN_RIGHT ||                        //判断是否收到0x43原地右转命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_BACKWARD_RIGHT ||                    //判断是否收到0x44后退右转命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_BACKWARD ||                          //判断是否收到0x45后退命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_BACKWARD_LEFT ||                     //判断是否收到0x46后退左转命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_TURN_LEFT ||                         //判断是否收到0x47原地左转命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_FORWARD_LEFT ||                      //判断是否收到0x48前进左转命令
            ReceivedByte == (uint8_t)BLUETOOTH_CONTROL_STOP)                                //判断是否收到0x49停车命令
        {
            BluetoothControlCommand = (BluetoothControlTypeDef)ReceivedByte;                //保存等待后续电机适配层执行的有效控制命令
            BluetoothManualMode = 1U;                                                       //标记手机已经进入蓝牙手动控制模式
        }
    }
}

/**
  * 函    数：发送蓝牙实时遥测数据包
  * 参    数：Status 当前避障状态快照
  * 返 回 值：无
  */
void Bluetooth_SendTelemetry(const AvoidanceStatusTypeDef *Status)
{
    uint8_t Packet[BLUETOOTH_PACKET_LENGTH];                                                 //保存准备通过USART2发送的固定九字节数据包

    Packet[0] = BLUETOOTH_PACKET_HEADER_FIRST;                                               //写入第一个固定包头0x55
    Packet[1] = BLUETOOTH_PACKET_HEADER_SECOND;                                              //写入第二个固定包头0x51
    Packet[2] = Bluetooth_EncodeDistance(Status->FrontDistanceCm);                           //写入0x01字段对应的前方厘米距离
    Packet[3] = Bluetooth_EncodeDistance(Status->RearDistanceCm);                            //写入0x02字段对应的后方厘米距离
    Packet[4] = Bluetooth_EncodeDistance(Status->LeftDistanceCm);                            //写入0x03字段对应的左侧厘米距离
    Packet[5] = Bluetooth_EncodeDistance(Status->RightDistanceCm);                           //写入0x04字段对应的右侧厘米距离
    Packet[6] = (uint8_t)Bluetooth_GetLidarStatus(Status);                                   //写入0x05字段对应的激光雷达工作状态
    Packet[7] = (uint8_t)Bluetooth_GetCarStatus(Status);                                     //写入0x06字段对应的小车运行状态
    Packet[8] = BLUETOOTH_PACKET_TAIL;                                                       //写入固定包尾0x5B
    Serial_SendArray(Packet, BLUETOOTH_PACKET_LENGTH);                                       //通过USART2向蓝牙模块发送完整九字节数据包
}

/**
  * 函    数：更新底盘电机与陀螺仪遥测字段
  * 参    数：LeftRearSpeed 左后速度，LeftFrontSpeed 左前速度，RightFrontSpeed 右前速度，RightRearSpeed 右后速度，GyroscopeAngle 陀螺仪角度
  * 返 回 值：无
  */
void Bluetooth_SetChassisTelemetry(uint8_t LeftRearSpeed, uint8_t LeftFrontSpeed, uint8_t RightFrontSpeed, uint8_t RightRearSpeed, uint8_t GyroscopeAngle)
{
    BluetoothChassisTelemetry.LeftRearSpeed = LeftRearSpeed;                                //更新0x01字段对应的左后电机原始速度值
    BluetoothChassisTelemetry.LeftFrontSpeed = LeftFrontSpeed;                              //更新0x02字段对应的左前电机原始速度值
    BluetoothChassisTelemetry.RightFrontSpeed = RightFrontSpeed;                            //更新0x03字段对应的右前电机原始速度值
    BluetoothChassisTelemetry.RightRearSpeed = RightRearSpeed;                              //更新0x04字段对应的右后电机原始速度值
    BluetoothChassisTelemetry.GyroscopeAngle = GyroscopeAngle;                              //更新0x05字段对应的陀螺仪原始角度值
}

/**
  * 函    数：发送底盘电机与陀螺仪遥测数据包
  * 参    数：无
  * 返 回 值：无
  */
void Bluetooth_SendChassisTelemetry(void)
{
    uint8_t Packet[BLUETOOTH_CHASSIS_PACKET_LENGTH];                                        //保存准备通过USART2发送的固定八字节数据包

    Packet[0] = BLUETOOTH_PACKET_HEADER_FIRST;                                               //写入与雷达数据包相同的第一个固定包头0x55
    Packet[1] = BLUETOOTH_CHASSIS_HEADER_SECOND;                                             //写入电机和陀螺仪数据包第二个包头0x52
    Packet[2] = BluetoothChassisTelemetry.LeftRearSpeed;                                     //写入0x01字段对应的左后电机原始速度值
    Packet[3] = BluetoothChassisTelemetry.LeftFrontSpeed;                                    //写入0x02字段对应的左前电机原始速度值
    Packet[4] = BluetoothChassisTelemetry.RightFrontSpeed;                                   //写入0x03字段对应的右前电机原始速度值
    Packet[5] = BluetoothChassisTelemetry.RightRearSpeed;                                    //写入0x04字段对应的右后电机原始速度值
    Packet[6] = BluetoothChassisTelemetry.GyroscopeAngle;                                    //写入0x05字段对应的陀螺仪原始角度值
    Packet[7] = BLUETOOTH_PACKET_TAIL;                                                       //写入与雷达数据包相同的固定包尾0x5B
    Serial_SendArray(Packet, BLUETOOTH_CHASSIS_PACKET_LENGTH);                               //通过USART2向蓝牙模块发送完整八字节数据包
}

/**
  * 函    数：取得手机APP控制命令
  * 参    数：无
  * 返 回 值：最近一次有效的前进后退左右转或停车命令
  */
BluetoothControlTypeDef Bluetooth_GetControlCommand(void)
{
    return BluetoothControlCommand;                                                         //返回等待后续四电机适配层执行的控制命令
}
