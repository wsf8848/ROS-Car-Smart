#include "stm32f10x.h"                       //提供STM32F10x固定宽度整数类型
#include "AvoidanceMonitor.h"                //提供避障OLED串口和声光观测接口声明
#include "AvoidanceConfig.h"                 //提供未知距离和障碍物提示阈值定义
#include "OLED.h"                            //提供四行OLED字符显示接口
#include "Serial.h"                          //提供USART2格式化调试输出接口
#include "LED.h"                             //提供板载红绿蓝LED状态指示接口
#include "Buzzer.h"                          //提供前后障碍物无阻塞声音提示接口
#include "Bluetooth.h"                       //提供工作模式、掌机命令和四轮蓝牙遥测字段

static AvoidanceStateTypeDef LastOledState = AVOIDANCE_STATE_WAIT;                          //保存OLED上一次已经显示的避障状态
static uint16_t LastOledFrontCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                            //保存OLED上一次已经显示的前方距离
static uint16_t LastOledLeftCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                             //保存OLED上一次已经显示的左侧距离
static uint16_t LastOledRightCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                            //保存OLED上一次已经显示的右侧距离
static uint16_t LastOledRearCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                             //保存OLED上一次已经显示的后方距离
static int16_t LastOledLinearX = 0;                                                         //保存OLED上一次已经显示的前进指令
static int16_t LastOledAngularZ = 0;                                                        //保存OLED上一次已经显示的旋转指令
static BluetoothModeTypeDef LastOledMode = BLUETOOTH_MODE_NONE;                            //保存OLED当前已经绘制的工作模式页面

static void AvoidanceMonitor_ShowDistance(uint8_t Line, uint8_t Column, uint16_t DistanceCm); //声明三位厘米距离显示辅助函数

/**
  * 函    数：根据工作模式绘制OLED固定页面
  * 参    数：Mode 当前掌机选择的工作模式
  * 返 回 值：无
  */
static void AvoidanceMonitor_PrepareModePage(BluetoothModeTypeDef Mode)
{
    OLED_Clear();                                                                           //清除上一工作模式残留的全部字符和像素
    if (Mode == BLUETOOTH_MODE_MANUAL)                                                      //判断是否需要绘制蓝牙手动控制页面
    {
        OLED_ShowString(1U, 1U, "TX51/52 P0000 00");                                      //显示蓝牙双包头、当前速度档和最近调速字节
        OLED_ShowString(2U, 1U, "MODE:00 CMD:00  ");                                      //预留模式字节和最近控制字节显示位置
        OLED_ShowString(3U, 1U, "LR:+000 LF:+000 ");                                      //预留左后与左前轮速度显示位置
        OLED_ShowString(4U, 1U, "RF:+000 RR:+000 ");                                      //预留右前与右后轮速度显示位置
    }
    else if (Mode == BLUETOOTH_MODE_AUTONOMOUS)                                            //判断是否需要绘制自主导航预留页面
    {
        OLED_ShowString(1U, 1U, "MODE:AUTO 5D    ");                                      //显示自主导航模式和对应掌机字节
        OLED_ShowString(2U, 1U, "STATUS:RESERVED ");                                      //提示自主导航电机逻辑尚未接入
        OLED_ShowString(3U, 1U, "F:--- B:--- CM  ");                                      //预留自主页面前后厘米距离
        OLED_ShowString(4U, 1U, "L:--- R:--- CM  ");                                      //预留自主页面左右厘米距离
    }
    else                                                                                    //处理避障模式和上电尚未选择模式的页面
    {
        OLED_ShowString(1U, 1U, "AVD:WAIT        ");                                      //显示避障状态等待初始值
        OLED_ShowString(2U, 1U, "F:--- L:--- CM  ");                                      //显示前方和左侧厘米距离固定界面
        OLED_ShowString(3U, 1U, "R:--- B:--- CM  ");                                      //显示右侧和后方厘米距离固定界面
        OLED_ShowString(4U, 1U, "V:+000 W:+000   ");                                      //显示前进和旋转标准化指令固定界面
        LastOledState = (AvoidanceStateTypeDef)0xFFU;                                      //强制下一次刷新重写避障状态文本
        LastOledFrontCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                   //强制下一次刷新重写前方距离
        LastOledLeftCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                    //强制下一次刷新重写左侧距离
        LastOledRightCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                   //强制下一次刷新重写右侧距离
        LastOledRearCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                    //强制下一次刷新重写后方距离
        LastOledLinearX = 32767;                                                            //强制下一次刷新重写前进指令
        LastOledAngularZ = 32767;                                                           //强制下一次刷新重写旋转指令
    }
    LastOledMode = Mode;                                                                    //记录当前已经完成绘制的工作模式页面
}

/**
  * 函    数：刷新OLED蓝牙手动控制页面
  * 参    数：无
  * 返 回 值：无
  */
static void AvoidanceMonitor_ShowBluetoothPage(void)
{
    BluetoothChassisTelemetryTypeDef Telemetry;                                            //保存最近一次已经发送的四轮速度字段

    Bluetooth_GetChassisTelemetry(&Telemetry);                                             //取得十毫秒底盘蓝牙包使用的最近速度数据
    OLED_ShowNum(1U, 10U, Chassis_GetManualSpeedPwm(), 4U);                                 //显示当前手动控制外侧轮PWM速度档位
    OLED_ShowHexNum(1U, 15U, Bluetooth_GetSpeedCommand(), 2U);                              //显示最近一次L2减速或R2加速命令字节
    OLED_ShowHexNum(2U, 6U, (uint8_t)Bluetooth_GetMode(), 2U);                              //显示掌机下发的0x5B蓝牙模式字节
    OLED_ShowHexNum(2U, 13U, (uint8_t)Bluetooth_GetControlCommand(), 2U);                    //显示最近一次0x41至0x49控制命令
    OLED_ShowSignedNum(3U, 4U, (int8_t)Telemetry.LeftRearSpeed, 3U);                         //显示左后轮十毫秒有符号编码器增量
    OLED_ShowSignedNum(3U, 12U, (int8_t)Telemetry.LeftFrontSpeed, 3U);                       //显示左前轮十毫秒有符号编码器增量
    OLED_ShowSignedNum(4U, 4U, (int8_t)Telemetry.RightFrontSpeed, 3U);                       //显示右前轮十毫秒有符号编码器增量
    OLED_ShowSignedNum(4U, 12U, (int8_t)Telemetry.RightRearSpeed, 3U);                       //显示右后轮十毫秒有符号编码器增量
}

/**
  * 函    数：刷新OLED自主导航预留页面
  * 参    数：Status 避障状态快照
  * 返 回 值：无
  */
static void AvoidanceMonitor_ShowAutonomousPage(const AvoidanceStatusTypeDef *Status)
{
    AvoidanceMonitor_ShowDistance(3U, 3U, Status->FrontDistanceCm);                         //显示补偿安装方向后的前方厘米距离
    AvoidanceMonitor_ShowDistance(3U, 9U, Status->RearDistanceCm);                          //显示补偿安装方向后的后方厘米距离
    AvoidanceMonitor_ShowDistance(4U, 3U, Status->LeftDistanceCm);                          //显示补偿安装方向后的左侧厘米距离
    AvoidanceMonitor_ShowDistance(4U, 9U, Status->RightDistanceCm);                         //显示补偿安装方向后的右侧厘米距离
}

/**
  * 函    数：在OLED显示三位厘米距离
  * 参    数：Line 显示行，Column 显示列，DistanceCm 厘米距离
  * 返 回 值：无
  */
static void AvoidanceMonitor_ShowDistance(uint8_t Line, uint8_t Column, uint16_t DistanceCm)
{
    if (DistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM)                                       //判断当前方向是否没有新鲜有效距离
    {
        OLED_ShowString(Line, Column, "---");                                               //仅在距离未知时用三条短线显示占位符
    }
    else                                                                                    //处理当前方向包含有效厘米距离的情况
    {
        if (DistanceCm > 999U)                                                              //判断厘米距离是否超过OLED三位显示范围
        {
            DistanceCm = 999U;                                                              //将超量程显示值限制为九百九十九厘米
        }
        OLED_ShowNum(Line, Column, DistanceCm, 3U);                                         //以固定三位十进制数显示厘米距离
    }
}

/**
  * 函    数：判断指定方向是否存在提示距离内障碍物
  * 参    数：DistanceCm 当前方向厘米距离
  * 返 回 值：一表示存在障碍物，零表示距离未知或没有近距离障碍物
  */
static uint8_t AvoidanceMonitor_IsBlocked(uint16_t DistanceCm)
{
    if (DistanceCm != AVOIDANCE_UNKNOWN_DISTANCE_CM && DistanceCm <= AVOIDANCE_ALERT_DISTANCE_CM) //判断当前方向是否具有九十厘米内有效测距
    {
        return 1U;                                                                          //返回当前方向存在需要提示的障碍物
    }
    return 0U;                                                                              //返回当前方向没有需要提示的障碍物
}

/**
  * 函    数：初始化OLED避障观测界面
  * 参    数：无
  * 返 回 值：无
  */
void AvoidanceMonitor_Init(void)
{
    OLED_ShowString(1U, 1U, "AVD:WAIT        ");                                            //显示避障状态等待初始值
    OLED_ShowString(2U, 1U, "F:--- L:--- CM  ");                                            //显示前方和左侧厘米距离固定界面
    OLED_ShowString(3U, 1U, "R:--- B:--- CM  ");                                            //显示右侧和后方厘米距离固定界面
    OLED_ShowString(4U, 1U, "V:+000 W:+000   ");                                            //显示前进和旋转标准化指令固定界面
    LastOledState = AVOIDANCE_STATE_WAIT;                                                   //同步状态缓存以避免重复刷新相同字符
    LastOledFrontCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                       //同步前方未知距离显示缓存
    LastOledLeftCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                        //同步左侧未知距离显示缓存
    LastOledRightCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                       //同步右侧未知距离显示缓存
    LastOledRearCm = AVOIDANCE_UNKNOWN_DISTANCE_CM;                                        //同步后方未知距离显示缓存
    LastOledLinearX = 0;                                                                    //同步前进指令零值显示缓存
    LastOledAngularZ = 0;                                                                   //同步旋转指令零值显示缓存
    LastOledMode = BLUETOOTH_MODE_NONE;                                                     //记录上电初始页面尚未选择工作模式
}

/**
  * 函    数：通过OLED显示实时避障信息
  * 参    数：Status 避障状态快照，Command 标准化运动指令
  * 返 回 值：无
  */
void AvoidanceMonitor_ShowOLED(const AvoidanceStatusTypeDef *Status, const MotionCommandTypeDef *Command)
{
    BluetoothModeTypeDef CurrentMode = Bluetooth_GetMode();                                //读取掌机最近下发的0x5B、0x5C或0x5D工作模式

    if (CurrentMode != LastOledMode)                                                        //判断掌机是否切换了OLED工作页面
    {
        AvoidanceMonitor_PrepareModePage(CurrentMode);                                     //清屏并绘制新模式对应的固定页面
    }
    if (CurrentMode == BLUETOOTH_MODE_MANUAL)                                              //判断当前是否处于蓝牙手动控制模式
    {
        AvoidanceMonitor_ShowBluetoothPage();                                              //显示蓝牙发送周期、控制字节和四轮速度
        return;                                                                             //结束本次显示并隐藏雷达避障页面
    }
    if (CurrentMode == BLUETOOTH_MODE_AUTONOMOUS)                                          //判断当前是否处于自主导航预留模式
    {
        AvoidanceMonitor_ShowAutonomousPage(Status);                                       //显示自主页面和经过方向补偿的四向距离
        return;                                                                             //结束本次显示并隐藏避障指令页面
    }
    if (Status->State != LastOledState)                                                     //判断避障状态文本是否发生变化
    {
        OLED_ShowString(1U, 5U, "            ");                                            //仅在状态变化时清除旧状态文本区域
        OLED_ShowString(1U, 5U, Avoidance_GetStateText(Status->State));                      //显示当前避障状态英文短名称
        LastOledState = Status->State;                                                      //记录本次已经显示的避障状态
    }
    if (Status->FrontDistanceCm != LastOledFrontCm)                                         //判断前方厘米距离是否发生变化
    {
        AvoidanceMonitor_ShowDistance(2U, 3U, Status->FrontDistanceCm);                      //只重写前方三位距离字符以避免整行闪烁
        LastOledFrontCm = Status->FrontDistanceCm;                                          //记录本次已经显示的前方距离
    }
    if (Status->LeftDistanceCm != LastOledLeftCm)                                           //判断左侧厘米距离是否发生变化
    {
        AvoidanceMonitor_ShowDistance(2U, 9U, Status->LeftDistanceCm);                       //只重写左侧三位距离字符以避免整行闪烁
        LastOledLeftCm = Status->LeftDistanceCm;                                            //记录本次已经显示的左侧距离
    }
    if (Status->RightDistanceCm != LastOledRightCm)                                         //判断右侧厘米距离是否发生变化
    {
        AvoidanceMonitor_ShowDistance(3U, 3U, Status->RightDistanceCm);                      //只重写右侧三位距离字符以避免整行闪烁
        LastOledRightCm = Status->RightDistanceCm;                                          //记录本次已经显示的右侧距离
    }
    if (Status->RearDistanceCm != LastOledRearCm)                                           //判断后方厘米距离是否发生变化
    {
        AvoidanceMonitor_ShowDistance(3U, 9U, Status->RearDistanceCm);                       //只重写后方三位距离字符以避免整行闪烁
        LastOledRearCm = Status->RearDistanceCm;                                            //记录本次已经显示的后方距离
    }
    if (Command->LinearXPermille != LastOledLinearX)                                        //判断前进标准化指令是否发生变化
    {
        OLED_ShowSignedNum(4U, 3U, Command->LinearXPermille, 3U);                            //只重写前进指令对应四个字符
        LastOledLinearX = Command->LinearXPermille;                                         //记录本次已经显示的前进指令
    }
    if (Command->AngularZPermille != LastOledAngularZ)                                      //判断旋转标准化指令是否发生变化
    {
        OLED_ShowSignedNum(4U, 10U, Command->AngularZPermille, 3U);                          //只重写旋转指令对应四个字符
        LastOledAngularZ = Command->AngularZPermille;                                       //记录本次已经显示的旋转指令
    }
}

/**
  * 函    数：通过USART2打印实时避障信息
  * 参    数：Status 避障状态，Command 运动指令，ValidFrameCount 有效帧数，CrcErrorCount CRC错误数
  * 返 回 值：无
  */
void AvoidanceMonitor_PrintSerial(const AvoidanceStatusTypeDef *Status, const MotionCommandTypeDef *Command, uint32_t ValidFrameCount, uint32_t CrcErrorCount)
{
    Serial_Printf("[AVOID] state=%s front=%ucm left=%ucm right=%ucm rear=%ucm vx=%d/1000 vy=%d/1000 wz=%d/1000 stop=%u age=%lums bins=%u lidar_valid=%lu crc_error=%lu\r\n", //输出统一厘米距离和标准化运动指令
                  Avoidance_GetStateText(Status->State),                                    //提供当前避障状态英文短名称
                  Status->FrontDistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM ? 0U : Status->FrontDistanceCm, //提供前方最近障碍物厘米距离
                  Status->LeftDistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM ? 0U : Status->LeftDistanceCm, //提供左侧最近障碍物厘米距离
                  Status->RightDistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM ? 0U : Status->RightDistanceCm, //提供右侧最近障碍物厘米距离
                  Status->RearDistanceCm == AVOIDANCE_UNKNOWN_DISTANCE_CM ? 0U : Status->RearDistanceCm, //提供后方最近障碍物厘米距离
                  Command->LinearXPermille,                                                 //提供前后方向标准化速度指令
                  Command->LinearYPermille,                                                 //提供左右方向标准化速度指令
                  Command->AngularZPermille,                                                //提供车体旋转速度指令
                  Command->EmergencyStop,                                                   //提供电机适配层紧急停车标志
                  Status->DataAgeMs,                                                        //提供最近有效雷达帧数据年龄
                  Status->FreshBinCount,                                                    //提供当前完整距离图的新鲜栅格数量
                  ValidFrameCount,                                                          //提供USART1累计CRC正确帧数量
                  CrcErrorCount);                                                           //提供USART1累计CRC错误帧数量
}

/**
  * 函    数：更新板载LED和蜂鸣器障碍物提示
  * 参    数：Status 避障状态快照，NowMs 当前系统毫秒时间
  * 返 回 值：无
  */
void AvoidanceMonitor_UpdateAlerts(const AvoidanceStatusTypeDef *Status, uint32_t NowMs)
{
    uint8_t FrontBlocked = AvoidanceMonitor_IsBlocked(Status->FrontDistanceCm);              //判断前方是否存在提示距离内障碍物
    uint8_t RearBlocked = AvoidanceMonitor_IsBlocked(Status->RearDistanceCm);                 //判断后方是否存在提示距离内障碍物
    uint8_t LeftBlocked = AvoidanceMonitor_IsBlocked(Status->LeftDistanceCm);                 //判断左侧是否存在提示距离内障碍物
    uint8_t RightBlocked = AvoidanceMonitor_IsBlocked(Status->RightDistanceCm);               //判断右侧是否存在提示距离内障碍物
    uint8_t BlinkOn = (uint8_t)(((NowMs / AVOIDANCE_LED_FAST_HALF_PERIOD_MS) & 1UL) == 0UL);  //生成一百毫秒明暗切换的快速闪烁相位

    if (Bluetooth_GetMode() == BLUETOOTH_MODE_MANUAL)                                      //判断当前是否处于蓝牙手动控制模式
    {
        Buzzer_SetObstacleAlert(0U, 0U);                                                    //关闭激光雷达前后障碍物蜂鸣器提示
        LED_R_OFF();                                                                         //关闭激光雷达正常运行红灯提示
        LED_G_OFF();                                                                         //关闭激光雷达左侧障碍物绿灯提示
        LED_B_OFF();                                                                         //关闭激光雷达右侧障碍物蓝灯提示
        return;                                                                              //退出声光更新并避免雷达状态覆盖手动控制模式
    }

    if (Bluetooth_GetMode() == BLUETOOTH_MODE_AVOIDANCE)                                    //判断当前是否处于蓝牙遥控避障模式
    {
        Bluetooth_UpdateAvoidanceControl(Status, NowMs);                                     //按雷达四向距离仲裁掌机期望动作并输出避障动作
    }

    Buzzer_SetObstacleAlert(FrontBlocked, RearBlocked);                                      //提交前方单响和后方优先双响的非阻塞提示请求
    LED_R_OFF();                                                                             //默认先熄灭红色雷达运行指示灯
    LED_G_OFF();                                                                             //默认先熄灭绿色左侧障碍物指示灯
    LED_B_OFF();                                                                             //默认先熄灭蓝色右侧障碍物指示灯

    if (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT) //判断雷达是否尚未就绪或已经超时故障
    {
        LED_R_ON();                                                                          //红灯常亮表示当前雷达数据不能用于安全行驶
        Buzzer_SetObstacleAlert(0U, 0U);                                                     //雷达数据无效时关闭基于旧距离的声音提示
        return;                                                                              //结束本次无效雷达状态声光更新
    }

    if (LeftBlocked == 0U && RightBlocked == 0U)                                             //判断左右两侧是否均没有近距离障碍物
    {
        if (BlinkOn != 0U)                                                                   //判断当前是否处于红灯快速闪烁点亮相位
        {
            LED_R_ON();                                                                      //红灯快速闪烁表示激光雷达正常运行
        }
    }
    if (LeftBlocked != 0U && BlinkOn != 0U)                                                  //判断左侧受阻且处于绿色灯点亮相位
    {
        LED_G_ON();                                                                          //绿色灯快速闪烁提示左侧存在障碍物
    }
    if (RightBlocked != 0U && BlinkOn != 0U)                                                 //判断右侧受阻且处于蓝色灯点亮相位
    {
        LED_B_ON();                                                                          //蓝色灯快速闪烁提示右侧存在障碍物
    }
}
