#include "RosProtocol.h"                                // 提供正式协议消息类型和公开接口声明
#include "RosProtocolConfig.h"                          // 提供看门狗、标定和动力锁定集中配置
#include "RosLink.h"                                    // 提供USART1正式协议字节收发接口
#include "Timer.h"                                      // 提供STM32上电单调毫秒时间
#include "Bluetooth.h"                                  // 提供原有人工控制模式、命令和停车状态只读接口
#include "Chassis.h"                                    // 提供四轮PWM输出与累计编码器状态接口
#include "Battery.h"                                    // 提供电池电压和百分比接口
#include "ICM42670.h"                                   // 提供板载ICM42670六轴与温度采样接口
#include <string.h>                                      // 提供二进制载荷与浮点字段安全复制函数
#include <math.h>                                        // 提供静止检测所需的浮点绝对值和平方根函数

#define ROS_FRAME_MAGIC_FIRST 0xAAU                     // 定义正式协议第一个帧头字节
#define ROS_FRAME_MAGIC_SECOND 0x55U                    // 定义正式协议第二个帧头字节
#define ROS_FRAME_FIXED_BODY_SIZE 10U                   // 定义版本至时间戳的固定正文长度
#define ROS_FRAME_CRC_SIZE 2U                           // 定义帧尾CRC字段长度
#define ROS_FRAME_MAX_SIZE (2U + ROS_FRAME_FIXED_BODY_SIZE + ROS_PROTOCOL_MAX_PAYLOAD + ROS_FRAME_CRC_SIZE) // 定义完整正式协议最大帧长度
#define ROS_CMD_VEL_PAYLOAD_SIZE 9U                     // 定义两个float和控制源组成的速度载荷长度
#define ROS_ACK_PAYLOAD_SIZE 2U                         // 定义确认消息类型和结果组成的载荷长度
#define ROS_TELEMETRY_PAYLOAD_SIZE 73U                  // 定义网站驱动固定解析的完整遥测载荷长度
#define ROS_ACK_RESULT_OK 0U                            // 定义命令接收并执行成功结果
#define ROS_ACK_RESULT_BAD_LENGTH 1U                    // 定义命令载荷长度错误结果
#define ROS_ACK_RESULT_DENIED 2U                        // 定义人工抢占或安全条件拒绝结果
#define ROS_ACK_RESULT_UNSUPPORTED 3U                   // 定义未知消息类型结果
#define ROS_IMU_BIAS_CALIBRATION_MS 5000UL              // 定义上电静止零偏采集持续五秒
#define ROS_IMU_BIAS_MIN_SAMPLES 450U                   // 定义五秒窗口内允许的最少有效样本数
#define ROS_IMU_GYRO_STATIC_LIMIT_RADPS 0.08f          // 定义静止判断的单轴角速度上限
#define ROS_IMU_ACCEL_NORM_TOLERANCE 0.80f              // 定义静止判断的重力模长允许误差

typedef enum                                             // 定义逐字节协议解析状态
{
    ROS_PARSE_MAGIC_FIRST = 0U,                          // 等待第一个帧头字节AA
    ROS_PARSE_MAGIC_SECOND,                              // 等待第二个帧头字节55
    ROS_PARSE_BODY                                       // 接收版本到CRC组成的剩余帧数据
} RosParseStateTypeDef;                                  // 结束协议解析状态枚举定义

static RosParseStateTypeDef RosParseState = ROS_PARSE_MAGIC_FIRST; // 保存当前逐字节协议解析状态
static uint8_t RosFrameBuffer[ROS_FRAME_MAX_SIZE];       // 保存正在接收或发送的完整正式协议帧
static uint16_t RosFrameIndex = 0U;                      // 保存当前接收帧下一个写入位置
static uint16_t RosExpectedFrameSize = 0U;               // 保存解析长度字段后得到的完整帧长度
static uint16_t RosTransmitSequence = 0U;                // 保存STM32发往RDK的独立递增序号
static uint32_t RosLastCommandMs = 0UL;                  // 保存最近一个采纳的ROS速度命令时间
static uint32_t RosLastImuMs = 0UL;                      // 保存最近一次ICM42670采样时间
static uint32_t RosLastTelemetryMs = 0UL;                // 保存最近一次完整遥测发送时间
static uint32_t RosLastRxOverflowCount = 0UL;            // 保存最近一次观察到的USART1溢出统计
#if ROS_PROTOCOL_ROS_PWM_ENABLED != 0U                   // 仅在实车标定解锁构建中保存连续速度命令
static float RosLinearCommand = 0.0f;                    // 保存最近一个合法ROS线速度命令
static float RosAngularCommand = 0.0f;                   // 保存最近一个合法ROS角速度命令
#endif                                                   // 结束ROS速度命令缓存编译期开关
static uint8_t RosMode = ROS_MODE_MANUAL;                // 保存ROS链当前是否申请自主控制
static uint8_t RosEstop = 0U;                            // 保存ROS软件急停锁存状态
static uint8_t RosControlSource = ROS_SOURCE_NONE;       // 保存当前控制源供网页状态显示
static uint16_t RosStateFlags = ROS_STATE_COMMAND_TIMEOUT; // 保存命令超时、急停和人工抢占状态位
static uint16_t RosFaultCode = ROS_FAULT_CALIBRATION_PENDING; // 保存IMU、标定和串口故障位
static ICM42670SampleTypeDef RosImuSample;               // 保存最近一次有效ICM42670标准单位采样
static float RosGyroBiasX = 0.0f;                        // 保存上电采集的X轴陀螺仪零偏
static float RosGyroBiasY = 0.0f;                        // 保存上电采集的Y轴陀螺仪零偏
static float RosGyroBiasZ = 0.0f;                        // 保存上电采集的Z轴陀螺仪零偏
static float RosGyroBiasSumX = 0.0f;                     // 累计校零窗口内的X轴角速度
static float RosGyroBiasSumY = 0.0f;                     // 累计校零窗口内的Y轴角速度
static float RosGyroBiasSumZ = 0.0f;                     // 累计校零窗口内的Z轴角速度
static uint16_t RosGyroBiasSamples = 0U;                 // 保存校零窗口内的有效静止样本数量
static uint32_t RosGyroBiasStartMs = 0UL;                // 保存当前静止校零窗口起始时间
static uint8_t RosControlReady = 0U;                     // 保存上电校零是否完成并允许控制

/**
  * 函    数：判断ICM42670采样是否满足静止条件
  * 参    数：Sample 当前六轴标准单位采样
  * 返 回 值：非零表示车辆当前满足静止校零条件
  */
static uint8_t RosProtocol_IsImuStatic(const ICM42670SampleTypeDef *Sample) //判断ICM42670采样是否满足静止条件
{
    float AccelNormSquared;                              // 保存三轴加速度模长平方
    float Gravity = 9.80665f;                            // 保存标准重力加速度
    float LowerGravity = Gravity - ROS_IMU_ACCEL_NORM_TOLERANCE; // 保存静止模长下限
    float UpperGravity = Gravity + ROS_IMU_ACCEL_NORM_TOLERANCE; // 保存静止模长上限
    if (Sample == 0)                                     // 判断调用者是否传入有效采样地址
    {
        return 0U;                                       // 无效地址不能判定为静止
    }
    AccelNormSquared = Sample->AccelXMeterPerSecond2 * Sample->AccelXMeterPerSecond2; // 累加X轴加速度平方
    AccelNormSquared += Sample->AccelYMeterPerSecond2 * Sample->AccelYMeterPerSecond2; // 累加Y轴加速度平方
    AccelNormSquared += Sample->AccelZMeterPerSecond2 * Sample->AccelZMeterPerSecond2; // 累加Z轴加速度平方
    if (fabsf(Sample->GyroXRadianPerSecond) > ROS_IMU_GYRO_STATIC_LIMIT_RADPS) // 判断X轴角速度是否超过静止阈值
    {
        return 0U;                                       // 车辆旋转时拒绝当前校零样本
    }
    if (fabsf(Sample->GyroYRadianPerSecond) > ROS_IMU_GYRO_STATIC_LIMIT_RADPS) // 判断Y轴角速度是否超过静止阈值
    {
        return 0U;                                       // 车辆俯仰旋转时拒绝当前校零样本
    }
    if (fabsf(Sample->GyroZRadianPerSecond) > ROS_IMU_GYRO_STATIC_LIMIT_RADPS) // 判断Z轴角速度是否超过静止阈值
    {
        return 0U;                                       // 车辆偏航旋转时拒绝当前校零样本
    }
    if (AccelNormSquared < LowerGravity * LowerGravity)  // 判断加速度模长是否低于静止重力范围
    {
        return 0U;                                       // 加速度异常时拒绝当前校零样本
    }
    if (AccelNormSquared > UpperGravity * UpperGravity)  // 判断加速度模长是否高于静止重力范围
    {
        return 0U;                                       // 加速度振动或搬动时拒绝当前校零样本
    }
    return 1U;                                           // 当前六轴数据满足静止条件
}

/**
  * 函    数：重置ICM42670静止零偏采集窗口
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_ResetBiasCalibration(uint32_t NowMs) //重置ICM42670静止零偏采集窗口
{
    RosGyroBiasSumX = 0.0f;                              // 清除X轴校零累计值
    RosGyroBiasSumY = 0.0f;                              // 清除Y轴校零累计值
    RosGyroBiasSumZ = 0.0f;                              // 清除Z轴校零累计值
    RosGyroBiasSamples = 0U;                             // 清除当前校零窗口样本数
    RosGyroBiasStartMs = NowMs;                          // 从当前静止采样重新开始五秒计时
    RosControlReady = 0U;                                // 重新校零期间禁止所有底盘控制
    RosStateFlags |= (uint16_t)(ROS_STATE_IMU_CALIBRATING | ROS_STATE_IMU_NOT_READY); // 发布校零进行中和未就绪状态
    RosFaultCode |= (uint16_t)(ROS_FAULT_IMU_BIAS_PENDING | ROS_FAULT_IMU_BIAS_FAILED); // 保持校零待完成和异常提示
    Chassis_Stop();                                      // 校零窗口重新开始时立即清除四轮动力输出
}

/**
  * 函    数：处理一个有效ICM42670采样并完成上电零偏采集
  * 参    数：Sample 当前未扣除零偏的六轴采样，NowMs当前毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_UpdateBiasCalibration(const ICM42670SampleTypeDef *Sample, uint32_t NowMs) //处理一个有效ICM42670采样并完成上电零偏采集
{
    if (RosControlReady != 0U)                         // 判断上电零偏是否已经完成
    {
        return;                                          // 已完成时不重复改变零偏基线
    }
    if (RosProtocol_IsImuStatic(Sample) == 0U)           // 判断本次采样是否满足车辆静止条件
    {
        RosProtocol_ResetBiasCalibration(NowMs);         // 车辆被移动时丢弃窗口并重新开始采集
        return;                                          // 本次非静止数据不进入零偏平均值
    }
    RosGyroBiasSumX += Sample->GyroXRadianPerSecond;     // 累加静止窗口内的X轴角速度
    RosGyroBiasSumY += Sample->GyroYRadianPerSecond;     // 累加静止窗口内的Y轴角速度
    RosGyroBiasSumZ += Sample->GyroZRadianPerSecond;     // 累加静止窗口内的Z轴角速度
    RosGyroBiasSamples++;                                // 累加一个有效静止样本
    if ((uint32_t)(NowMs - RosGyroBiasStartMs) < ROS_IMU_BIAS_CALIBRATION_MS) // 判断五秒静止窗口是否尚未结束
    {
        return;                                          // 窗口未结束时继续保持停车和校零状态
    }
    if (RosGyroBiasSamples < ROS_IMU_BIAS_MIN_SAMPLES)   // 判断有效样本是否达到最低数量
    {
        RosProtocol_ResetBiasCalibration(NowMs);         // 样本不足时重新开始完整窗口
        return;                                          // 不使用不完整窗口计算零偏
    }
    RosGyroBiasX = RosGyroBiasSumX / (float)RosGyroBiasSamples; // 计算X轴角速度平均零偏
    RosGyroBiasY = RosGyroBiasSumY / (float)RosGyroBiasSamples; // 计算Y轴角速度平均零偏
    RosGyroBiasZ = RosGyroBiasSumZ / (float)RosGyroBiasSamples; // 计算Z轴角速度平均零偏
    RosControlReady = 1U;                                // 标记校零完成并允许后续控制入口放行
    RosStateFlags &= (uint16_t)(~(ROS_STATE_IMU_CALIBRATING | ROS_STATE_IMU_NOT_READY)); // 清除校零进行中和未就绪状态
    RosFaultCode &= (uint16_t)(~(ROS_FAULT_IMU_BIAS_PENDING | ROS_FAULT_IMU_BIAS_FAILED)); // 清除零偏待完成和异常故障位
}

/**
  * 函    数：读取小端十六位无符号整数
  * 参    数：Data 字段首地址
  * 返 回 值：转换后的十六位数值
  */
static uint16_t RosProtocol_ReadU16(const uint8_t *Data) //读取小端十六位无符号整数
{
    return (uint16_t)((uint16_t)Data[0] | ((uint16_t)Data[1] << 8)); // 按低字节优先拼接十六位字段
}

/**
  * 函    数：写入小端十六位无符号整数
  * 参    数：Data 字段首地址，Value 待写入数值
  * 返 回 值：无
  */
static void RosProtocol_WriteU16(uint8_t *Data, uint16_t Value) //写入小端十六位无符号整数
{
    Data[0] = (uint8_t)(Value & 0xFFU);                  // 写入十六位数值低字节
    Data[1] = (uint8_t)(Value >> 8);                     // 写入十六位数值高字节
}

/**
  * 函    数：写入小端三十二位无符号整数
  * 参    数：Data 字段首地址，Value 待写入数值
  * 返 回 值：无
  */
static void RosProtocol_WriteU32(uint8_t *Data, uint32_t Value) //写入小端三十二位无符号整数
{
    Data[0] = (uint8_t)(Value & 0xFFUL);                 // 写入三十二位数值最低字节
    Data[1] = (uint8_t)((Value >> 8) & 0xFFUL);          // 写入三十二位数值次低字节
    Data[2] = (uint8_t)((Value >> 16) & 0xFFUL);         // 写入三十二位数值次高字节
    Data[3] = (uint8_t)((Value >> 24) & 0xFFUL);         // 写入三十二位数值最高字节
}

/**
  * 函    数：读取小端IEEE754单精度浮点数
  * 参    数：Data 四字节字段首地址
  * 返 回 值：转换后的浮点数
  */
static float RosProtocol_ReadFloat(const uint8_t *Data) //读取小端IEEE754单精度浮点数
{
    float Value;                                         // 保存从四个协议字节恢复的浮点值
    memcpy(&Value, Data, sizeof(float));                 // 按STM32小端IEEE754内存格式复制浮点字段
    return Value;                                        // 返回恢复后的协议浮点值
}

/**
  * 函    数：写入小端IEEE754单精度浮点数
  * 参    数：Data 四字节字段首地址，Value 待写入浮点数
  * 返 回 值：无
  */
static void RosProtocol_WriteFloat(uint8_t *Data, float Value) //写入小端IEEE754单精度浮点数
{
    memcpy(Data, &Value, sizeof(float));                 // 按STM32小端IEEE754内存格式写入浮点字段
}

/**
  * 函    数：将浮点值限制在指定绝对范围
  * 参    数：Value 输入值，Limit 正数绝对值上限
  * 返 回 值：限制后的浮点值
  */
#if ROS_PROTOCOL_ROS_PWM_ENABLED != 0U                   // 仅在动力解锁构建中需要执行速度和PWM限幅
static float RosProtocol_ClampFloat(float Value, float Limit) //将浮点值限制在指定绝对范围
{
    if (Value > Limit)                                   // 判断输入值是否超过正向上限
    {
        return Limit;                                    // 返回正向上限避免速度命令过大
    }
    if (Value < -Limit)                                  // 判断输入值是否超过负向上限
    {
        return -Limit;                                   // 返回负向上限避免速度命令过大
    }
    return Value;                                        // 输入值位于安全范围时保持原值
}
#endif                                                   // 结束浮点限幅函数编译期开关

/**
  * 函    数：计算CRC-16/CCITT-FALSE
  * 参    数：Data 数据首地址，Length 数据长度
  * 返 回 值：多项式0x1021初值0xFFFF的十六位校验值
  */
uint16_t RosProtocol_Crc16(const uint8_t *Data, uint16_t Length) //计算CRC-16/CCITT-FALSE
{
    uint16_t Crc = 0xFFFFU;                              // 使用CCITT-FALSE规定的初始值
    uint16_t Index;                                      // 保存当前处理的字节下标
    uint8_t BitIndex;                                    // 保存当前处理的数据位下标
    if (Data == 0)                                       // 判断调用者是否提供有效数据地址
    {
        return Crc;                                      // 地址无效时返回初始值并避免非法访问
    }
    for (Index = 0U; Index < Length; Index++)            // 依次处理输入数据全部字节
    {
        Crc ^= (uint16_t)Data[Index] << 8;               // 将当前字节异或到CRC高八位
        for (BitIndex = 0U; BitIndex < 8U; BitIndex++)   // 依次推进当前字节的八个数据位
        {
            if ((Crc & 0x8000U) != 0U)                   // 判断CRC最高位是否为一
            {
                Crc = (uint16_t)((Crc << 1) ^ 0x1021U);  // 左移并异或CCITT多项式
            }
            else                                        // 处理CRC最高位为零的情况
            {
                Crc = (uint16_t)(Crc << 1);              // 仅左移一位推进CRC计算
            }
        }
    }
    return Crc;                                          // 返回完整CRC-16/CCITT-FALSE结果
}

/**
  * 函    数：发送一个正式协议帧
  * 参    数：MessageType 消息类型，Payload 载荷地址，PayloadLength载荷长度，NowMs当前毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_SendFrame(uint8_t MessageType, const uint8_t *Payload, uint16_t PayloadLength, uint32_t NowMs) //发送一个正式协议帧
{
    uint16_t FrameLength;                                // 保存编码后的完整协议帧长度
    uint16_t Crc;                                        // 保存版本至载荷的CRC计算结果
    if (PayloadLength > ROS_PROTOCOL_MAX_PAYLOAD)        // 判断载荷是否超过协议固定上限
    {
        return;                                          // 超长载荷不发送防止缓冲区越界
    }
    RosFrameBuffer[0] = ROS_FRAME_MAGIC_FIRST;           // 写入第一个固定帧头字节AA
    RosFrameBuffer[1] = ROS_FRAME_MAGIC_SECOND;          // 写入第二个固定帧头字节55
    RosFrameBuffer[2] = ROS_PROTOCOL_VERSION;            // 写入正式协议版本号
    RosFrameBuffer[3] = MessageType;                     // 写入本帧消息类型
    RosProtocol_WriteU16(&RosFrameBuffer[4], PayloadLength); // 写入小端载荷长度
    RosProtocol_WriteU16(&RosFrameBuffer[6], RosTransmitSequence); // 写入STM32独立递增发送序号
    RosProtocol_WriteU32(&RosFrameBuffer[8], NowMs);     // 写入STM32上电单调毫秒时间戳
    if (PayloadLength > 0U && Payload != 0)              // 判断本帧是否带有有效载荷
    {
        memcpy(&RosFrameBuffer[12], Payload, PayloadLength); // 复制载荷到固定头部之后
    }
    Crc = RosProtocol_Crc16(&RosFrameBuffer[2], (uint16_t)(ROS_FRAME_FIXED_BODY_SIZE + PayloadLength)); // 计算版本至载荷CRC
    RosProtocol_WriteU16(&RosFrameBuffer[12U + PayloadLength], Crc); // 写入小端CRC校验字段
    FrameLength = (uint16_t)(14U + PayloadLength);       // 计算双字节帧头加正文载荷和CRC总长度
    RosLink_SendProtocolArray(RosFrameBuffer, FrameLength); // 通过USART1发送完整正式协议帧
    RosTransmitSequence++;                               // 递增STM32发送序号并允许十六位自然回绕
}

/**
  * 函    数：发送命令确认帧
  * 参    数：AcknowledgedType 被确认消息类型，Result处理结果，NowMs当前毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_SendAck(uint8_t AcknowledgedType, uint8_t Result, uint32_t NowMs) //发送命令确认帧
{
    uint8_t Payload[ROS_ACK_PAYLOAD_SIZE];               // 保存被确认类型和处理结果载荷
    Payload[0] = AcknowledgedType;                       // 写入被确认的原始消息类型
    Payload[1] = Result;                                 // 写入成功、长度错误、拒绝或不支持结果
    RosProtocol_SendFrame(ROS_MESSAGE_ACK, Payload, ROS_ACK_PAYLOAD_SIZE, NowMs); // 发送完整ACK正式协议帧
}

/**
  * 函    数：判断原有蓝牙或MQTT是否正在人工接管
  * 参    数：无
  * 返 回 值：非零表示ROS必须让权
  */
static uint8_t RosProtocol_ManualOverrideActive(void) //判断原有蓝牙或MQTT是否正在人工接管
{
    BluetoothControlTypeDef Command = Bluetooth_GetControlCommand(); // 读取原有人工控制最近运动命令
    if (Bluetooth_IsEmergencyStopped() != 0U)            // 判断原有蓝牙或MQTT是否已锁存停车
    {
        return 1U;                                       // 人工停车具有高于ROS的控制优先级
    }
    if (Command >= BLUETOOTH_CONTROL_FORWARD && Command <= BLUETOOTH_CONTROL_FORWARD_LEFT) // 判断原有人工链是否存在运动意图
    {
        return 1U;                                       // 人工运动命令存在时ROS必须立即让权
    }
    return 0U;                                           // 没有人工运动或停车锁存时允许ROS申请控制
}

/**
  * 函    数：安全停止ROS动力输出
  * 参    数：无
  * 返 回 值：无
  */
static void RosProtocol_StopRosMotion(void) //安全停止ROS动力输出
{
#if ROS_PROTOCOL_ROS_PWM_ENABLED != 0U                   // 仅在动力解锁构建中清除速度命令缓存
    RosLinearCommand = 0.0f;                             // 清除最近ROS线速度命令
    RosAngularCommand = 0.0f;                            // 清除最近ROS角速度命令
#endif                                                   // 结束速度命令缓存清除编译期开关
    Chassis_Stop();                                      // 立即关闭四轮动力输出并清除斜坡目标
}

/**
  * 函    数：将ROS速度命令转换为左右PWM
  * 参    数：无
  * 返 回 值：无
  */
static void RosProtocol_ApplyMotion(void) //将ROS速度命令转换为左右PWM
{
#if ROS_PROTOCOL_ROS_PWM_ENABLED != 0U                   // 仅在完成实车标定并显式解锁后编译ROS动力换算
    float LeftPwmFloat;                                  // 保存左侧两轮待换算PWM浮点值
    float RightPwmFloat;                                 // 保存右侧两轮待换算PWM浮点值
    int16_t LeftPwm;                                     // 保存左侧两轮最终PWM整数值
    int16_t RightPwm;                                    // 保存右侧两轮最终PWM整数值
    LeftPwmFloat = RosLinearCommand * ROS_PROTOCOL_LINEAR_TO_PWM - RosAngularCommand * ROS_PROTOCOL_ANGULAR_TO_PWM; // 计算左侧差速PWM
    RightPwmFloat = RosLinearCommand * ROS_PROTOCOL_LINEAR_TO_PWM + RosAngularCommand * ROS_PROTOCOL_ANGULAR_TO_PWM; // 计算右侧差速PWM
    LeftPwmFloat = RosProtocol_ClampFloat(LeftPwmFloat, ROS_PROTOCOL_MAX_PWM); // 限制左侧PWM不超过底盘安全范围
    RightPwmFloat = RosProtocol_ClampFloat(RightPwmFloat, ROS_PROTOCOL_MAX_PWM); // 限制右侧PWM不超过底盘安全范围
    LeftPwm = (int16_t)LeftPwmFloat;                     // 将左侧PWM转换为电机驱动整数计数
    RightPwm = (int16_t)RightPwmFloat;                   // 将右侧PWM转换为电机驱动整数计数
    Chassis_ApplyRosPwm(LeftPwm, RightPwm);              // 把左右差速目标写入四轮平滑控制层
#else                                                    // 处理尚未完成实车尺度和极性标定的安全构建
    RosProtocol_StopRosMotion();                         // 未标定阶段保持物理底盘安全停车
#endif                                                   // 结束ROS动力输出编译期开关
}

/**
  * 函    数：处理一个CRC正确的正式协议帧
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_HandleFrame(uint32_t NowMs) //处理一个CRC正确的正式协议帧
{
    uint8_t Version = RosFrameBuffer[2];                 // 读取协议版本字段
    uint8_t MessageType = RosFrameBuffer[3];             // 读取消息类型字段
    uint16_t PayloadLength = RosProtocol_ReadU16(&RosFrameBuffer[4]); // 读取载荷长度字段
    const uint8_t *Payload = &RosFrameBuffer[12];        // 指向本帧载荷起始地址
    uint8_t AckResult = ROS_ACK_RESULT_OK;               // 默认命令处理结果为成功
    if (Version != ROS_PROTOCOL_VERSION)                 // 判断协议版本是否与固件支持版本一致
    {
        return;                                          // 版本不匹配时丢弃帧且不改变任何控制状态
    }
    if (MessageType == ROS_MESSAGE_CMD_VEL)              // 处理连续线速度和角速度命令
    {
        if (PayloadLength != ROS_CMD_VEL_PAYLOAD_SIZE)   // 判断速度载荷是否恰好为两个float和一个来源字节
        {
            AckResult = ROS_ACK_RESULT_BAD_LENGTH;       // 标记速度命令载荷长度错误
        }
        else if (Payload[8] != ROS_SOURCE_RDK || RosMode != ROS_MODE_AUTONOMOUS || RosEstop != 0U || RosProtocol_ManualOverrideActive() != 0U) // 判断来源、模式、急停和人工抢占条件
        {
            AckResult = ROS_ACK_RESULT_DENIED;           // 拒绝不满足安全条件的ROS速度命令
            RosProtocol_StopRosMotion();                 // 拒绝命令时确保不存在旧ROS动力输出
        }
        else                                            // 处理满足全部安全条件的ROS速度命令
        {
#if ROS_PROTOCOL_ROS_PWM_ENABLED != 0U                   // 仅在完成标定解锁后保存并执行非零速度
            RosLinearCommand = RosProtocol_ClampFloat(RosProtocol_ReadFloat(&Payload[0]), ROS_PROTOCOL_MAX_LINEAR_MPS); // 读取并限制线速度
            RosAngularCommand = RosProtocol_ClampFloat(RosProtocol_ReadFloat(&Payload[4]), ROS_PROTOCOL_MAX_ANGULAR_RADPS); // 读取并限制角速度
#else                                                    // 处理当前默认安全锁定构建
            (void)RosProtocol_ReadFloat(&Payload[0]);    // 验证线速度字段可按网站协议解码但不驱动车辆
            (void)RosProtocol_ReadFloat(&Payload[4]);    // 验证角速度字段可按网站协议解码但不驱动车辆
#endif                                                   // 结束ROS非零速度执行编译期开关
            RosLastCommandMs = NowMs;                    // 刷新STM32独立速度命令看门狗
            RosStateFlags &= (uint16_t)(~ROS_STATE_COMMAND_TIMEOUT); // 清除收到有效速度后的超时状态位
            RosProtocol_ApplyMotion();                   // 按标定锁状态执行或安全拦截底盘输出
        }
    }
    else if (MessageType == ROS_MESSAGE_HEARTBEAT)       // 处理RDK ROS链路心跳消息
    {
        if (PayloadLength != 1U)                         // 判断心跳载荷是否只包含一个控制源字节
        {
            AckResult = ROS_ACK_RESULT_BAD_LENGTH;       // 标记心跳载荷长度错误
        }
        else if (Payload[0] != ROS_SOURCE_RDK)           // 判断心跳声明来源是否为ROS
        {
            AckResult = ROS_ACK_RESULT_DENIED;           // 拒绝伪造或未知控制源心跳
        }
    }
    else if (MessageType == ROS_MESSAGE_SET_MODE)        // 处理ROS控制权模式切换命令
    {
        if (PayloadLength != 1U || Payload[0] > ROS_MODE_AUTONOMOUS) // 判断模式载荷长度和值域是否有效
        {
            AckResult = ROS_ACK_RESULT_BAD_LENGTH;       // 标记模式命令参数错误
        }
        else                                            // 处理有效的手动或ROS模式切换
        {
            RosProtocol_StopRosMotion();                 // 模式切换前先停车并清除旧ROS命令
            RosMode = Payload[0];                        // 保存RDK请求的手动或ROS自主模式
            RosLastCommandMs = NowMs;                    // 从模式切换时刻重新开始速度命令看门狗
            RosStateFlags |= ROS_STATE_COMMAND_TIMEOUT;  // 新模式必须等待首个有效速度命令才能运动
        }
    }
    else if (MessageType == ROS_MESSAGE_ESTOP)           // 处理ROS软件急停锁存或释放命令
    {
        if (PayloadLength != 1U || Payload[0] > 1U)      // 判断急停载荷是否为合法布尔值
        {
            AckResult = ROS_ACK_RESULT_BAD_LENGTH;       // 标记急停命令参数错误
        }
        else                                            // 处理合法急停状态变更
        {
            RosProtocol_StopRosMotion();                 // 急停状态变化前立即停车
            RosEstop = Payload[0];                       // 保存ROS软件急停锁存状态
            if (RosEstop != 0U)                          // 判断本次是否断言急停
            {
                RosStateFlags |= ROS_STATE_ESTOP;        // 设置急停状态位供网页显示
            }
            else                                        // 处理RDK释放ROS软件急停的情况
            {
                RosStateFlags &= (uint16_t)(~ROS_STATE_ESTOP); // 清除ROS急停状态位但仍等待新速度命令
                RosStateFlags |= ROS_STATE_COMMAND_TIMEOUT; // 要求释放后收到新速度命令才允许运动
            }
        }
    }
    else                                                // 处理固件不支持的消息类型
    {
        AckResult = ROS_ACK_RESULT_UNSUPPORTED;          // 标记未知消息类型不受支持
    }
    if (MessageType != ROS_MESSAGE_ACK && MessageType != ROS_MESSAGE_TELEMETRY) // 判断是否属于需要确认的RDK命令
    {
        RosProtocol_SendAck(MessageType, AckResult, NowMs); // 向RDK返回类型和处理结果
    }
}

/**
  * 函    数：输入一个USART1字节并推进正式协议解析
  * 参    数：Byte 当前接收字节，NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_ParseByte(uint8_t Byte, uint32_t NowMs) //输入一个USART1字节并推进正式协议解析
{
    uint16_t PayloadLength;                              // 保存解析固定头部后取得的载荷长度
    uint16_t ReceivedCrc;                                // 保存帧尾携带的CRC值
    uint16_t CalculatedCrc;                              // 保存本地重新计算的CRC值
    if (RosParseState == ROS_PARSE_MAGIC_FIRST)          // 判断解析器是否正在寻找第一个帧头字节
    {
        if (Byte == ROS_FRAME_MAGIC_FIRST)               // 判断当前字节是否为AA
        {
            RosFrameBuffer[0] = Byte;                    // 保存第一个帧头字节
            RosFrameIndex = 1U;                          // 移动到第二个帧头写入位置
            RosParseState = ROS_PARSE_MAGIC_SECOND;      // 切换到等待55状态
        }
        return;                                          // 完成本字节帧头同步处理
    }
    if (RosParseState == ROS_PARSE_MAGIC_SECOND)         // 判断解析器是否正在等待第二个帧头字节
    {
        if (Byte == ROS_FRAME_MAGIC_SECOND)              // 判断当前字节是否为55
        {
            RosFrameBuffer[1] = Byte;                    // 保存第二个帧头字节
            RosFrameIndex = 2U;                          // 移动到版本字段写入位置
            RosExpectedFrameSize = 0U;                   // 清除上一帧残留的预期长度
            RosParseState = ROS_PARSE_BODY;              // 切换到接收正文状态
        }
        else if (Byte != ROS_FRAME_MAGIC_FIRST)          // 判断当前字节是否也不是新的AA
        {
            RosParseState = ROS_PARSE_MAGIC_FIRST;       // 非AA非55时重新寻找第一个帧头字节
            RosFrameIndex = 0U;                          // 清除不完整帧写入位置
        }
        return;                                          // 完成本字节第二帧头处理
    }
    if (RosFrameIndex >= ROS_FRAME_MAX_SIZE)             // 判断异常输入是否已经占满最大帧缓冲区
    {
        RosParseState = ROS_PARSE_MAGIC_FIRST;           // 缓冲区越界前丢弃异常帧并重新同步
        RosFrameIndex = 0U;                              // 清除异常帧写入位置
        return;                                          // 不写入越界字节
    }
    RosFrameBuffer[RosFrameIndex] = Byte;                // 保存当前正文、载荷或CRC字节
    RosFrameIndex++;                                     // 移动到下一个接收字节写入位置
    if (RosFrameIndex == 6U)                             // 判断版本类型和载荷长度字段是否已经完整
    {
        PayloadLength = RosProtocol_ReadU16(&RosFrameBuffer[4]); // 读取小端载荷长度字段
        if (PayloadLength > ROS_PROTOCOL_MAX_PAYLOAD)    // 判断载荷长度是否超过协议允许上限
        {
            RosParseState = ROS_PARSE_MAGIC_FIRST;       // 超长帧立即丢弃并重新寻找帧头
            RosFrameIndex = 0U;                          // 清除异常帧写入位置
            return;                                      // 结束当前超长帧处理
        }
        RosExpectedFrameSize = (uint16_t)(14U + PayloadLength); // 计算双帧头、固定正文、载荷和CRC总长度
    }
    if (RosExpectedFrameSize != 0U && RosFrameIndex >= RosExpectedFrameSize) // 判断完整帧是否已经接收完毕
    {
        ReceivedCrc = RosProtocol_ReadU16(&RosFrameBuffer[RosExpectedFrameSize - 2U]); // 读取帧尾小端CRC字段
        CalculatedCrc = RosProtocol_Crc16(&RosFrameBuffer[2], (uint16_t)(RosExpectedFrameSize - 4U)); // 重新计算版本至载荷CRC
        if (ReceivedCrc == CalculatedCrc)                // 判断本地CRC是否与帧尾字段一致
        {
            RosProtocol_HandleFrame(NowMs);              // 仅处理CRC正确的正式协议帧
        }
        RosParseState = ROS_PARSE_MAGIC_FIRST;           // 完成当前帧后重新寻找下一个帧头
        RosFrameIndex = 0U;                              // 清除当前帧写入位置
        RosExpectedFrameSize = 0U;                       // 清除当前帧预期长度
    }
}

/**
  * 函    数：更新控制源、看门狗和人工抢占状态
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_UpdateControl(uint32_t NowMs) //更新控制源、看门狗和人工抢占状态
{
    uint8_t ManualOverride = RosProtocol_ManualOverrideActive(); // 读取原有人工运动或停车锁存状态
    if (ManualOverride != 0U)                            // 判断蓝牙或MQTT人工链是否正在抢占
    {
        RosControlSource = ROS_SOURCE_BLUETOOTH;         // 原有公开接口无法区分USART2与UART4，统一报告人工蓝牙兼容源
        RosStateFlags |= ROS_STATE_MANUAL_OVERRIDE;      // 设置人工抢占状态位供RDK停止非零命令
        RosProtocol_StopRosMotion();                     // 人工优先时立即清除ROS动力输出
        return;                                          // 保持原有蓝牙和MQTT逻辑继续控制底盘
    }
    RosStateFlags &= (uint16_t)(~ROS_STATE_MANUAL_OVERRIDE); // 没有人工命令时清除抢占状态位
    if (RosEstop != 0U)                                  // 判断ROS软件急停是否仍然锁存
    {
        RosControlSource = ROS_SOURCE_NONE;              // 急停期间不报告任何运动控制源
        RosStateFlags |= ROS_STATE_ESTOP;                // 保持急停状态位
        RosProtocol_StopRosMotion();                     // 急停期间持续确保底盘停车
        return;                                          // 不进入ROS速度执行路径
    }
    if (RosMode != ROS_MODE_AUTONOMOUS)                  // 判断RDK是否尚未明确切换到ROS自主模式
    {
        RosControlSource = ROS_SOURCE_NONE;              // 手动模式下释放ROS控制权
        RosProtocol_StopRosMotion();                     // 手动模式下不保留任何ROS输出
        return;                                          // 等待RDK发送正式模式切换命令
    }
    RosControlSource = ROS_SOURCE_RDK;                   // 安全条件满足时报告ROS控制源
    if ((uint32_t)(NowMs - RosLastCommandMs) > ROS_PROTOCOL_COMMAND_TIMEOUT_MS) // 判断连续速度命令是否超过三百五十毫秒未刷新
    {
        RosStateFlags |= ROS_STATE_COMMAND_TIMEOUT;      // 设置命令超时状态位供网页告警
        RosProtocol_StopRosMotion();                     // 由STM32本地独立执行失联停车
    }
}

/**
  * 函    数：定期采集ICM42670
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_UpdateImu(uint32_t NowMs) //定期采集ICM42670
{
    if ((uint32_t)(NowMs - RosLastImuMs) < ROS_PROTOCOL_IMU_PERIOD_MS) // 判断是否达到十毫秒IMU采样周期
    {
        return;                                          // 周期未到时保留最近一次有效采样
    }
    RosLastImuMs = NowMs;                                // 记录本次ICM42670采样时间
    if (ICM42670_ReadSample(&RosImuSample) != 0U)        // 判断本次软件I2C连续读取是否成功
    {
        RosFaultCode |= ROS_FAULT_IMU_READ;              // 设置运行期IMU读取故障位
    }
    else                                                // 处理本次六轴和温度读取成功的情况
    {
        RosFaultCode &= (uint16_t)(~ROS_FAULT_IMU_READ); // 清除可恢复的运行期IMU读取故障位
        RosProtocol_UpdateBiasCalibration(&RosImuSample, NowMs); // 使用有效采样推进上电静止五秒零偏校准
        if (RosControlReady != 0U)                       // 判断校零是否已完成并获得有效零偏
        {
            RosImuSample.GyroXRadianPerSecond -= RosGyroBiasX; // 从X轴角速度中扣除上电平均零偏
            RosImuSample.GyroYRadianPerSecond -= RosGyroBiasY; // 从Y轴角速度中扣除上电平均零偏
            RosImuSample.GyroZRadianPerSecond -= RosGyroBiasZ; // 从Z轴角速度中扣除上电平均零偏
        }
    }
}

/**
  * 函    数：发送网站驱动固定格式遥测
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
static void RosProtocol_SendTelemetry(uint32_t NowMs) //发送网站驱动固定格式遥测
{
    uint8_t Payload[ROS_TELEMETRY_PAYLOAD_SIZE];         // 保存网站驱动固定解析的七十三字节载荷
    ChassisRosSnapshotTypeDef ChassisSnapshot;           // 保存四轮累计计数和最近十毫秒增量
    float WheelSpeed[4];                                 // 保存四轮角速度，未标定前固定为零
    float BatteryVoltage;                                // 保存转换为伏特的电池电压
    float BatteryPercentage;                             // 保存零到一的电池百分比
    uint16_t Offset = 0U;                                // 保存逐字段编码的当前载荷偏移
    uint8_t Index;                                       // 保存四轮数组遍历下标
    if ((uint32_t)(NowMs - RosLastTelemetryMs) < ROS_PROTOCOL_TELEMETRY_PERIOD_MS) // 判断是否达到五十毫秒遥测周期
    {
        return;                                          // 周期未到时不占用USART1带宽
    }
    RosLastTelemetryMs = NowMs;                          // 记录本次正式遥测发送时间
    Chassis_GetRosSnapshot(&ChassisSnapshot);            // 读取不清零硬件编码器的四轮累计状态
    BatteryVoltage = (float)Battery_GetVoltageMv() / 1000.0f; // 将电池毫伏转换为协议伏特单位
    BatteryPercentage = (float)Battery_GetPercent() / 100.0f; // 将零到一百百分比转换为零到一
    for (Index = 0U; Index < 4U; Index++)                // 依次编码四轮累计ticks字段
    {
        RosProtocol_WriteU32(&Payload[Offset], (uint32_t)ChassisSnapshot.Tick[Index]); // 按二补码原样写入四轮int32计数
        Offset = (uint16_t)(Offset + 4U);                // 移动到下一个四轮ticks字段
        WheelSpeed[Index] = 0.0f;                        // 未取得编码器CPR前不发布虚构轮速尺度
    }
    for (Index = 0U; Index < 4U; Index++)                // 依次编码四轮角速度字段
    {
        RosProtocol_WriteFloat(&Payload[Offset], WheelSpeed[Index]); // 写入当前四轮标准单位速度
        Offset = (uint16_t)(Offset + 4U);                // 移动到下一个四轮速度字段
    }
    RosProtocol_WriteFloat(&Payload[Offset], RosImuSample.AccelXMeterPerSecond2 * ROS_PROTOCOL_IMU_ACCEL_X_SIGN); Offset = (uint16_t)(Offset + 4U); // 写入X轴加速度
    RosProtocol_WriteFloat(&Payload[Offset], RosImuSample.AccelYMeterPerSecond2 * ROS_PROTOCOL_IMU_ACCEL_Y_SIGN); Offset = (uint16_t)(Offset + 4U); // 写入Y轴加速度
    RosProtocol_WriteFloat(&Payload[Offset], RosImuSample.AccelZMeterPerSecond2 * ROS_PROTOCOL_IMU_ACCEL_Z_SIGN); Offset = (uint16_t)(Offset + 4U); // 写入Z轴加速度
    RosProtocol_WriteFloat(&Payload[Offset], RosImuSample.GyroXRadianPerSecond * ROS_PROTOCOL_IMU_GYRO_X_SIGN); Offset = (uint16_t)(Offset + 4U); // 写入X轴角速度
    RosProtocol_WriteFloat(&Payload[Offset], RosImuSample.GyroYRadianPerSecond * ROS_PROTOCOL_IMU_GYRO_Y_SIGN); Offset = (uint16_t)(Offset + 4U); // 写入Y轴角速度
    RosProtocol_WriteFloat(&Payload[Offset], RosImuSample.GyroZRadianPerSecond * ROS_PROTOCOL_IMU_GYRO_Z_SIGN); Offset = (uint16_t)(Offset + 4U); // 写入Z轴角速度
    RosProtocol_WriteFloat(&Payload[Offset], RosImuSample.TemperatureCelsius); Offset = (uint16_t)(Offset + 4U); // 写入ICM42670温度
    RosProtocol_WriteFloat(&Payload[Offset], BatteryVoltage); Offset = (uint16_t)(Offset + 4U); // 写入电池电压伏特值
    RosProtocol_WriteFloat(&Payload[Offset], BatteryPercentage); Offset = (uint16_t)(Offset + 4U); // 写入零到一电池电量比例
    Payload[Offset] = RosControlSource;                   // 写入无、人工兼容源或ROS控制源
    Offset = (uint16_t)(Offset + 1U);                    // 移动到状态位字段
    RosProtocol_WriteU16(&Payload[Offset], RosStateFlags); // 写入命令超时、急停和人工抢占状态位
    Offset = (uint16_t)(Offset + 2U);                    // 移动到故障码字段
    RosProtocol_WriteU16(&Payload[Offset], RosFaultCode); // 写入IMU、标定和串口故障位
    RosProtocol_SendFrame(ROS_MESSAGE_TELEMETRY, Payload, ROS_TELEMETRY_PAYLOAD_SIZE, NowMs); // 发送完整正式遥测帧
}

/**
  * 函    数：初始化ROS正式协议模块
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
void RosProtocol_Init(uint32_t NowMs) //初始化ROS正式协议模块
{
    memset(&RosImuSample, 0, sizeof(RosImuSample));      // 清零上电时的六轴和温度遥测缓存
    RosParseState = ROS_PARSE_MAGIC_FIRST;               // 初始化逐字节解析器等待AA帧头
    RosFrameIndex = 0U;                                  // 清除上电时的接收帧写入位置
    RosExpectedFrameSize = 0U;                           // 清除上电时的预期帧长度
    RosTransmitSequence = 0U;                            // 清除STM32正式协议发送序号
    RosLastCommandMs = NowMs;                            // 从初始化时刻开始ROS速度命令看门狗
    RosLastImuMs = NowMs;                                // 从初始化时刻开始ICM42670采样周期
    RosLastTelemetryMs = NowMs;                          // 从初始化时刻开始正式遥测周期
    RosLastRxOverflowCount = RosLink_GetRxOverflowCount(); // 记录USART1上电接收溢出基线
#if ROS_PROTOCOL_ROS_PWM_ENABLED != 0U                   // 仅在动力解锁构建中初始化速度命令缓存
    RosLinearCommand = 0.0f;                             // 清除上电时的ROS线速度命令
    RosAngularCommand = 0.0f;                            // 清除上电时的ROS角速度命令
#endif                                                   // 结束速度命令缓存初始化编译期开关
    RosMode = ROS_MODE_MANUAL;                           // 上电先释放ROS控制权并等待校零完成
    RosEstop = 0U;                                       // 清除ROS软件急停但底盘仍保持停车
    RosControlSource = ROS_SOURCE_NONE;                  // 上电默认没有控制源
    RosStateFlags = ROS_STATE_COMMAND_TIMEOUT;           // 上电等待首个有效速度命令并报告超时状态
    RosFaultCode = (uint16_t)(ROS_FAULT_CALIBRATION_PENDING | ROS_FAULT_IMU_BIAS_PENDING); // 标记底盘标定和IMU零偏均待完成
    RosGyroBiasX = 0.0f;                                 // 清除上电前残留的X轴零偏
    RosGyroBiasY = 0.0f;                                 // 清除上电前残留的Y轴零偏
    RosGyroBiasZ = 0.0f;                                 // 清除上电前残留的Z轴零偏
    RosProtocol_ResetBiasCalibration(NowMs);             // 建立上电静止校零窗口并锁定全部控制
    if (ICM42670_Init() != 0U)                           // 判断板载ICM42670身份和寄存器配置是否成功
    {
        RosFaultCode |= ROS_FAULT_IMU_NOT_READY;         // 设置IMU未就绪故障位供网页显示
    }
    RosProtocol_StopRosMotion();                         // 协议初始化完成后确保底盘保持安全停车
}

/**
  * 函    数：处理ROS正式协议、控制安全与遥测任务
  * 参    数：NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
void RosProtocol_Process(uint32_t NowMs) //处理ROS正式协议、控制安全与遥测任务
{
    uint8_t ReceivedByte;                                // 保存从USART1正式协议缓冲区读取的字节
    uint32_t OverflowCount;                              // 保存当前USART1接收溢出统计
    while (RosLink_ReadProtocolByte(&ReceivedByte) != 0U) // 持续处理本轮已经收到的全部USART1字节
    {
        RosProtocol_ParseByte(ReceivedByte, NowMs);      // 将当前字节输入AA55正式帧解析器
    }
    OverflowCount = RosLink_GetRxOverflowCount();        // 读取USART1累计接收缓冲区溢出次数
    if (OverflowCount != RosLastRxOverflowCount)         // 判断本轮是否出现新的USART1接收溢出
    {
        RosLastRxOverflowCount = OverflowCount;          // 更新最近观察到的接收溢出统计
        RosFaultCode |= ROS_FAULT_RX_OVERFLOW;           // 设置USART1接收溢出故障位
    }
    RosProtocol_UpdateControl(NowMs);                    // 执行人工优先、模式、急停和三百五十毫秒看门狗
    RosProtocol_UpdateImu(NowMs);                        // 按十毫秒周期采集ICM42670六轴和温度
    RosProtocol_SendTelemetry(NowMs);                    // 按五十毫秒周期发送网站固定格式完整遥测
}

/**
  * 函    数：取得当前控制源
  * 参    数：无
  * 返 回 值：零无、一人工兼容源、三ROS
  */
uint8_t RosProtocol_GetControlSource(void) //取得当前控制源
{
    return RosControlSource;                             // 返回当前控制仲裁结果
}

/**
  * 函    数：取得当前状态位
  * 参    数：无
  * 返 回 值：命令超时、急停和人工抢占位组合
  */
uint16_t RosProtocol_GetStateFlags(void) //取得当前状态位
{
    return RosStateFlags;                                // 返回当前控制安全状态位
}

/**
  * 函    数：取得当前故障码
  * 参    数：无
  * 返 回 值：IMU、标定和串口故障位组合
  */
uint16_t RosProtocol_GetFaultCode(void) //取得当前故障码
{
    return RosFaultCode;                                 // 返回当前可诊断故障位
}

/**
  * 函    数：查询上电IMU校零控制锁
  * 参    数：无
  * 返 回 值：一表示静止校零完成，零表示仍禁止底盘控制
  */
uint8_t RosProtocol_IsControlReady(void) //查询上电IMU校零控制锁
{
    return RosControlReady;                              // 返回当前是否允许蓝牙、MQTT和ROS底盘输出
}
