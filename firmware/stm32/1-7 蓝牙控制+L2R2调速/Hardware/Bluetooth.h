#ifndef __BLUETOOTH_H__                                // 防止蓝牙协议头文件被重复包含
#define __BLUETOOTH_H__                                // 定义蓝牙协议头文件包含保护宏

#include "stm32f10x.h"                                // 提供蓝牙协议接口使用的固定宽度整数类型
#include "Avoidance.h"                                // 提供雷达遥测数据包使用的避障状态类型
#include "Chassis.h"                                  // 提供掌机运动命令和底盘速度结构体定义

#define BLUETOOTH_PACKET_HEADER_FIRST 0x55U            // 定义雷达状态数据包第一个固定包头字节
#define BLUETOOTH_PACKET_HEADER_SECOND 0x51U           // 定义雷达状态数据包第二个固定包头字节
#define BLUETOOTH_PACKET_TAIL 0x5BU                    // 定义两种遥测数据包共用的固定包尾字节
#define BLUETOOTH_PACKET_LENGTH 9U                     // 定义雷达状态数据包总长度
#define BLUETOOTH_CHASSIS_HEADER_SECOND 0x52U          // 定义底盘速度数据包第二个固定包头字节
#define BLUETOOTH_CHASSIS_PACKET_LENGTH 8U             // 定义底盘速度数据包总长度
#define BLUETOOTH_CONTROL_TIMEOUT_MS 350UL              // 定义超过三百五十毫秒无手动命令时自动停车

typedef enum                                           // 定义掌机发送的小车运动控制命令
{
    BLUETOOTH_CONTROL_NONE = 0x00U,                    // 表示尚未收到有效运动命令
    BLUETOOTH_CONTROL_FORWARD = CHASSIS_COMMAND_FORWARD, // 0x41表示四轮前进
    BLUETOOTH_CONTROL_FORWARD_RIGHT = CHASSIS_COMMAND_FORWARD_RIGHT, // 0x42表示前进并右转
    BLUETOOTH_CONTROL_TURN_RIGHT = CHASSIS_COMMAND_ROTATE_RIGHT, // 0x43表示原地右转
    BLUETOOTH_CONTROL_BACKWARD_RIGHT = CHASSIS_COMMAND_BACKWARD_RIGHT, // 0x44表示后退并右转
    BLUETOOTH_CONTROL_BACKWARD = CHASSIS_COMMAND_BACKWARD, // 0x45表示四轮后退
    BLUETOOTH_CONTROL_BACKWARD_LEFT = CHASSIS_COMMAND_BACKWARD_LEFT, // 0x46表示后退并左转
    BLUETOOTH_CONTROL_TURN_LEFT = CHASSIS_COMMAND_ROTATE_LEFT, // 0x47表示原地左转
    BLUETOOTH_CONTROL_FORWARD_LEFT = CHASSIS_COMMAND_FORWARD_LEFT, // 0x48表示前进并左转
    BLUETOOTH_CONTROL_STOP = CHASSIS_COMMAND_STOP // 0x49表示立即停车，手动模式下一条方向命令可恢复
} BluetoothControlTypeDef;                            // 结束掌机运动控制命令枚举定义

typedef enum                                           // 定义掌机左侧模式切换按钮对应的工作模式
{
    BLUETOOTH_MODE_NONE = 0x00U,                       // 表示上电后尚未选择工作模式
    BLUETOOTH_MODE_MANUAL = 0x5BU,                     // 0x5B对应掌机B键，进入蓝牙手动控制
    BLUETOOTH_MODE_AVOIDANCE = 0x5CU,                  // 0x5C对应掌机Y键，预留给实时避障控制
    BLUETOOTH_MODE_AUTONOMOUS = 0x5DU                 // 0x5D对应掌机X键，预留给自主导航控制
} BluetoothModeTypeDef;                                // 结束工作模式枚举定义

typedef enum                                           // 定义雷达状态数据包中的激光雷达状态字段
{
    BLUETOOTH_LIDAR_ABNORMAL = 0x00U,                  // 0x00表示雷达等待数据或数据超时异常
    BLUETOOTH_LIDAR_WORKING = 0x01U                   // 0x01表示雷达数据持续更新并可用
} BluetoothLidarStatusTypeDef;                         // 结束激光雷达状态枚举定义

typedef enum                                           // 定义雷达状态数据包中的小车工作状态字段
{
    BLUETOOTH_CAR_STANDBY = 0x00U,                     // 0x00表示待机或紧急停车
    BLUETOOTH_CAR_AVOIDING = 0x01U,                    // 0x01表示实时避障模式
    BLUETOOTH_CAR_AUTONOMOUS = 0x02U,                  // 0x02表示自主导航模式
    BLUETOOTH_CAR_MANUAL = 0x03U                      // 0x03表示蓝牙手动控制模式
} BluetoothCarStatusTypeDef;                           // 结束小车工作状态枚举定义

typedef struct                                           // 定义底盘速度数据包的缓存字段
{
    uint8_t LeftRearSpeed;                              // 0x01字段，左后轮十毫秒编码器增量int8二补码
    uint8_t LeftFrontSpeed;                             // 0x02字段，左前轮十毫秒编码器增量int8二补码
    uint8_t RightFrontSpeed;                            // 0x03字段，右前轮十毫秒编码器增量int8二补码
    uint8_t RightRearSpeed;                             // 0x04字段，右后轮十毫秒编码器增量int8二补码
    uint8_t GyroscopeAngle;                             // 0x05字段，陀螺仪角度预留值，接入后按协议更新
} BluetoothChassisTelemetryTypeDef;                     // 结束底盘速度数据包缓存结构体定义

void Bluetooth_Init(void);                              // 初始化蓝牙状态并初始化四驱底盘
void Bluetooth_ProcessReceivedData(void);               // 解析USART2环形缓冲区中的掌机模式和运动字节
void Bluetooth_SendTelemetry(const AvoidanceStatusTypeDef *Status); // 发送0x55 0x51雷达状态数据包
void Bluetooth_SetChassisTelemetry(uint8_t LeftRearSpeed, uint8_t LeftFrontSpeed, uint8_t RightFrontSpeed, uint8_t RightRearSpeed, uint8_t GyroscopeAngle); // 更新底盘遥测兼容缓存
void Bluetooth_SendChassisTelemetry(void);               // 发送0x55 0x52四路编码器和陀螺仪数据包
void Bluetooth_GetChassisTelemetry(BluetoothChassisTelemetryTypeDef *Telemetry); // 取得OLED显示使用的最近一次底盘遥测字段
uint8_t Bluetooth_GetSpeedCommand(void);                 // 取得最近一次L2或R2速度调整字节
BluetoothControlTypeDef Bluetooth_GetControlCommand(void); // 取得最近一次有效掌机运动命令
BluetoothModeTypeDef Bluetooth_GetMode(void);            // 取得当前掌机选择的工作模式
uint8_t Bluetooth_IsEmergencyStopped(void);             // 取得当前停车或蓝牙失联保护状态

#endif                                                // 结束蓝牙协议头文件包含保护
