#ifndef __BLUETOOTH_H                    //防止蓝牙协议头文件被重复包含
#define __BLUETOOTH_H                    //定义蓝牙协议头文件包含保护宏

#include "stm32f10x.h"                  //提供蓝牙协议接口使用的固定宽度整数类型
#include "Avoidance.h"                  //提供蓝牙遥测数据包使用的避障状态类型

#define BLUETOOTH_PACKET_HEADER_FIRST 0x55U  //定义蓝牙遥测数据包第一个固定包头字节
#define BLUETOOTH_PACKET_HEADER_SECOND 0x51U //定义蓝牙遥测数据包第二个固定包头字节
#define BLUETOOTH_PACKET_TAIL 0x5BU          //定义蓝牙遥测数据包固定包尾字节
#define BLUETOOTH_PACKET_LENGTH 9U           //定义蓝牙遥测数据包总长度为九个字节
#define BLUETOOTH_CHASSIS_HEADER_SECOND 0x52U //定义电机和陀螺仪数据包第二个固定包头字节
#define BLUETOOTH_CHASSIS_PACKET_LENGTH 8U    //定义电机和陀螺仪数据包总长度为八个字节

typedef enum                              //定义手机APP发送的小车手动控制命令
{
    BLUETOOTH_CONTROL_NONE = 0x00,         //表示上电后尚未收到手机控制命令
    BLUETOOTH_CONTROL_FORWARD = 0x41,      //表示收到0x41并请求小车前进
    BLUETOOTH_CONTROL_BACKWARD = 0x45,     //表示收到0x45并请求小车后退
    BLUETOOTH_CONTROL_TURN_LEFT = 0x47,    //表示收到0x47并请求小车左转
    BLUETOOTH_CONTROL_TURN_RIGHT = 0x43,   //表示收到0x43并请求小车右转
    BLUETOOTH_CONTROL_STOP = 0x49          //表示收到0x49并请求小车立即停车
} BluetoothControlTypeDef;                //结束手机APP控制命令枚举定义

typedef enum                              //定义蓝牙遥测数据包中的激光雷达状态值
{
    BLUETOOTH_LIDAR_ABNORMAL = 0x00,       //表示雷达等待数据或数据超时异常
    BLUETOOTH_LIDAR_WORKING = 0x01         //表示雷达数据持续更新并可用于避障
} BluetoothLidarStatusTypeDef;             //结束激光雷达状态值枚举定义

typedef enum                              //定义蓝牙遥测数据包中的小车状态值
{
    BLUETOOTH_CAR_STANDBY = 0x00,          //表示小车待机或雷达异常安全停车
    BLUETOOTH_CAR_AVOIDING = 0x01,         //表示小车正在减速转向脱困或紧急避障
    BLUETOOTH_CAR_AUTONOMOUS = 0x02,       //表示小车正在自主导航巡航
    BLUETOOTH_CAR_MANUAL = 0x03            //表示小车已收到蓝牙前后左右手动控制命令
} BluetoothCarStatusTypeDef;               //结束小车状态值枚举定义

typedef struct                              //定义等待后续电机和陀螺仪模块更新的原始遥测字段
{
    uint8_t LeftRearSpeed;                  //保存0x01字段对应的左后电机原始速度值
    uint8_t LeftFrontSpeed;                 //保存0x02字段对应的左前电机原始速度值
    uint8_t RightFrontSpeed;                //保存0x03字段对应的右前电机原始速度值
    uint8_t RightRearSpeed;                 //保存0x04字段对应的右后电机原始速度值
    uint8_t GyroscopeAngle;                 //保存0x05字段对应的陀螺仪原始角度值
} BluetoothChassisTelemetryTypeDef;         //结束底盘电机和陀螺仪遥测结构体定义

void Bluetooth_Init(void);                 //初始化蓝牙控制命令与工作模式状态
void Bluetooth_ProcessReceivedData(void);  //解析USART2环形缓冲区中的手机控制字节
void Bluetooth_SendTelemetry(const AvoidanceStatusTypeDef *Status);//发送九字节四向距离与状态数据包
void Bluetooth_SetChassisTelemetry(uint8_t LeftRearSpeed, uint8_t LeftFrontSpeed, uint8_t RightFrontSpeed, uint8_t RightRearSpeed, uint8_t GyroscopeAngle);//更新四个电机速度和陀螺仪角度原始字段
void Bluetooth_SendChassisTelemetry(void); //发送八字节四电机速度与陀螺仪角度数据包
BluetoothControlTypeDef Bluetooth_GetControlCommand(void);//取得等待后续电机适配层执行的手机控制命令

#endif                                      //结束蓝牙协议头文件包含保护
