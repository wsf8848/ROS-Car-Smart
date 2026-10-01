#include "stm32f10x.h"                  //提供STM32F10x固定宽度整数类型
#include "DriveControl.h"              //提供底盘控制适配接口声明
#include "Motor.h"                     //提供验证过的四电机方向和PWM驱动

static int16_t DriveTargetLeftRear = 0;                                                     //保存左后轮目标平滑PWM值
static int16_t DriveTargetLeftFront = 0;                                                    //保存左前轮目标平滑PWM值
static int16_t DriveTargetRightFront = 0;                                                   //保存右前轮目标平滑PWM值
static int16_t DriveTargetRightRear = 0;                                                    //保存右后轮目标平滑PWM值
static int16_t DriveCurrentLeftRear = 0;                                                    //保存左后轮当前平滑PWM值
static int16_t DriveCurrentLeftFront = 0;                                                   //保存左前轮当前平滑PWM值
static int16_t DriveCurrentRightFront = 0;                                                  //保存右前轮当前平滑PWM值
static int16_t DriveCurrentRightRear = 0;                                                   //保存右后轮当前平滑PWM值

/**
  * 函    数：限制底盘PWM范围
  * 参    数：Value 待限制的PWM值
  * 返 回 值：限制到正负最大PWM范围后的值
  */
static int16_t DriveControl_ClampPwm(int32_t Value)
{
    if (Value > DRIVE_MAX_PWM)                                                             //判断输入是否超过正向PWM上限
    {
        return DRIVE_MAX_PWM;                                                              //返回正向PWM上限
    }
    if (Value < -DRIVE_MAX_PWM)                                                            //判断输入是否超过反向PWM上限
    {
        return -DRIVE_MAX_PWM;                                                             //返回反向PWM上限
    }
    return (int16_t)Value;                                                                 //返回范围内的PWM值
}

/**
  * 函    数：按斜坡逼近目标PWM
  * 参    数：Current 当前PWM值，Target 目标PWM值
  * 返 回 值：本次平滑更新后的PWM值
  */
static int16_t DriveControl_RampToTarget(int16_t Current, int16_t Target)
{
    if (Current < Target)                                                                   //判断当前速度是否低于目标速度
    {
        Current = (int16_t)(Current + DRIVE_RAMP_STEP);                                     //按固定步长增加当前速度
        if (Current > Target)                                                               //判断增加后是否越过目标速度
        {
            Current = Target;                                                              //将越过目标的值修正为目标速度
        }
    }
    else if (Current > Target)                                                              //判断当前速度是否高于目标速度
    {
        Current = (int16_t)(Current - DRIVE_RAMP_STEP);                                     //按固定步长降低当前速度
        if (Current < Target)                                                               //判断降低后是否越过目标速度
        {
            Current = Target;                                                              //将越过目标的值修正为目标速度
        }
    }
    return Current;                                                                         //返回本次平滑后的当前速度
}

/**
  * 函    数：根据蓝牙命令生成四轮目标速度
  * 参    数：Command 手机蓝牙控制命令
  * 返 回 值：无
  */
static void DriveControl_SetManualTarget(BluetoothControlTypeDef Command)
{
    DriveTargetLeftRear = 0;                                                                //默认清除左后目标速度
    DriveTargetLeftFront = 0;                                                               //默认清除左前目标速度
    DriveTargetRightFront = 0;                                                              //默认清除右前目标速度
    DriveTargetRightRear = 0;                                                               //默认清除右后目标速度

    if (Command == BLUETOOTH_CONTROL_FORWARD)                                               //判断是否请求前进
    {
        DriveTargetLeftRear = DRIVE_MAX_PWM;                                                //左后轮输出正向最大平滑目标
        DriveTargetLeftFront = DRIVE_MAX_PWM;                                               //左前轮输出正向最大平滑目标
        DriveTargetRightFront = DRIVE_MAX_PWM;                                              //右前轮输出正向最大平滑目标
        DriveTargetRightRear = DRIVE_MAX_PWM;                                               //右后轮输出正向最大平滑目标
    }
    else if (Command == BLUETOOTH_CONTROL_FORWARD_RIGHT)                                     //判断是否请求前进同时右转
    {
        DriveTargetLeftRear = DRIVE_MAX_PWM;                                                //左后轮保持外侧高速
        DriveTargetLeftFront = DRIVE_MAX_PWM;                                               //左前轮保持外侧高速
        DriveTargetRightFront = DRIVE_CURVE_PWM;                                            //右前轮降低内侧速度
        DriveTargetRightRear = DRIVE_CURVE_PWM;                                             //右后轮降低内侧速度
    }
    else if (Command == BLUETOOTH_CONTROL_TURN_RIGHT)                                        //判断是否请求原地右转
    {
        DriveTargetLeftRear = DRIVE_TURN_PWM;                                               //左后轮正向驱动
        DriveTargetLeftFront = DRIVE_TURN_PWM;                                              //左前轮正向驱动
        DriveTargetRightFront = -DRIVE_TURN_PWM;                                            //右前轮反向驱动
        DriveTargetRightRear = -DRIVE_TURN_PWM;                                             //右后轮反向驱动
    }
    else if (Command == BLUETOOTH_CONTROL_BACKWARD_RIGHT)                                    //判断是否请求后退同时右转
    {
        DriveTargetLeftRear = -DRIVE_MAX_PWM;                                               //左后轮保持外侧反向高速
        DriveTargetLeftFront = -DRIVE_MAX_PWM;                                              //左前轮保持外侧反向高速
        DriveTargetRightFront = -DRIVE_CURVE_PWM;                                           //右前轮降低内侧反向速度
        DriveTargetRightRear = -DRIVE_CURVE_PWM;                                            //右后轮降低内侧反向速度
    }
    else if (Command == BLUETOOTH_CONTROL_BACKWARD)                                         //判断是否请求后退
    {
        DriveTargetLeftRear = -DRIVE_MAX_PWM;                                               //左后轮输出反向最大平滑目标
        DriveTargetLeftFront = -DRIVE_MAX_PWM;                                              //左前轮输出反向最大平滑目标
        DriveTargetRightFront = -DRIVE_MAX_PWM;                                             //右前轮输出反向最大平滑目标
        DriveTargetRightRear = -DRIVE_MAX_PWM;                                              //右后轮输出反向最大平滑目标
    }
    else if (Command == BLUETOOTH_CONTROL_BACKWARD_LEFT)                                     //判断是否请求后退同时左转
    {
        DriveTargetLeftRear = -DRIVE_CURVE_PWM;                                             //左后轮降低内侧反向速度
        DriveTargetLeftFront = -DRIVE_CURVE_PWM;                                            //左前轮降低内侧反向速度
        DriveTargetRightFront = -DRIVE_MAX_PWM;                                             //右前轮保持外侧反向高速
        DriveTargetRightRear = -DRIVE_MAX_PWM;                                              //右后轮保持外侧反向高速
    }
    else if (Command == BLUETOOTH_CONTROL_TURN_LEFT)                                        //判断是否请求原地左转
    {
        DriveTargetLeftRear = -DRIVE_TURN_PWM;                                              //左后轮反向驱动
        DriveTargetLeftFront = -DRIVE_TURN_PWM;                                             //左前轮反向驱动
        DriveTargetRightFront = DRIVE_TURN_PWM;                                             //右前轮正向驱动
        DriveTargetRightRear = DRIVE_TURN_PWM;                                              //右后轮正向驱动
    }
    else if (Command == BLUETOOTH_CONTROL_FORWARD_LEFT)                                      //判断是否请求前进同时左转
    {
        DriveTargetLeftRear = DRIVE_CURVE_PWM;                                              //左后轮降低内侧速度
        DriveTargetLeftFront = DRIVE_CURVE_PWM;                                             //左前轮降低内侧速度
        DriveTargetRightFront = DRIVE_MAX_PWM;                                              //右前轮保持外侧高速
        DriveTargetRightRear = DRIVE_MAX_PWM;                                               //右后轮保持外侧高速
    }
}

/**
  * 函    数：根据自主运动指令生成四轮目标速度
  * 参    数：Command 避障核心标准运动指令
  * 返 回 值：无
  */
static void DriveControl_SetAutonomousTarget(const MotionCommandTypeDef *Command)
{
    int32_t ForwardPwm = (int32_t)Command->LinearXPermille * DRIVE_MAX_PWM / 1000;          //将标准前后速度换算为PWM值
    int32_t TurnPwm = (int32_t)Command->AngularZPermille * DRIVE_MAX_PWM / 2000;             //将标准旋转速度换算为左右差速的一半
    int16_t LeftPwm = DriveControl_ClampPwm(ForwardPwm - TurnPwm);                           //计算左侧两轮自主目标速度
    int16_t RightPwm = DriveControl_ClampPwm(ForwardPwm + TurnPwm);                         //计算右侧两轮自主目标速度

    DriveTargetLeftRear = LeftPwm;                                                          //将自主左侧目标写入左后轮
    DriveTargetLeftFront = LeftPwm;                                                         //将自主左侧目标写入左前轮
    DriveTargetRightFront = RightPwm;                                                       //将自主右侧目标写入右前轮
    DriveTargetRightRear = RightPwm;                                                        //将自主右侧目标写入右后轮
}

/**
  * 函    数：四电机控制初始化
  * 参    数：无
  * 返 回 值：无
  */
void DriveControl_Init(void)
{
    DriveTargetLeftRear = 0;                                                                //清除左后轮目标速度
    DriveTargetLeftFront = 0;                                                               //清除左前轮目标速度
    DriveTargetRightFront = 0;                                                              //清除右前轮目标速度
    DriveTargetRightRear = 0;                                                               //清除右后轮目标速度
    DriveCurrentLeftRear = 0;                                                               //清除左后轮当前速度
    DriveCurrentLeftFront = 0;                                                              //清除左前轮当前速度
    DriveCurrentRightFront = 0;                                                             //清除右前轮当前速度
    DriveCurrentRightRear = 0;                                                              //清除右后轮当前速度
    MotorABCD_Init();                                                                       //初始化验证过的四电机方向和TIM8 PWM驱动
}

/**
  * 函    数：更新平滑四轮输出
  * 参    数：Status 当前避障状态，AutonomousCommand 自主指令，ManualCommand 蓝牙命令
  * 返 回 值：无
  */
void DriveControl_Update(const AvoidanceStatusTypeDef *Status, const MotionCommandTypeDef *AutonomousCommand, BluetoothControlTypeDef ManualCommand)
{
    if (Status->State == AVOIDANCE_STATE_WAIT || Status->State == AVOIDANCE_STATE_SENSOR_FAULT || Status->State == AVOIDANCE_STATE_EMERGENCY_STOP || AutonomousCommand->EmergencyStop != 0U) //判断雷达未就绪、故障或紧急停车
    {
        DriveTargetLeftRear = 0;                                                            //安全优先时清除左后轮目标
        DriveTargetLeftFront = 0;                                                           //安全优先时清除左前轮目标
        DriveTargetRightFront = 0;                                                          //安全优先时清除右前轮目标
        DriveTargetRightRear = 0;                                                           //安全优先时清除右后轮目标
    }
    else if (ManualCommand != BLUETOOTH_CONTROL_NONE)                                       //判断是否已经收到蓝牙手动控制命令
    {
        DriveControl_SetManualTarget(ManualCommand);                                       //按九档蓝牙命令生成四轮手动目标
    }
    else                                                                                     //处理尚未进入手动模式的自主避障状态
    {
        DriveControl_SetAutonomousTarget(AutonomousCommand);                               //按避障核心标准指令生成四轮自主目标
    }

    DriveCurrentLeftRear = DriveControl_RampToTarget(DriveCurrentLeftRear, DriveTargetLeftRear); //平滑更新左后轮速度
    DriveCurrentLeftFront = DriveControl_RampToTarget(DriveCurrentLeftFront, DriveTargetLeftFront); //平滑更新左前轮速度
    DriveCurrentRightFront = DriveControl_RampToTarget(DriveCurrentRightFront, DriveTargetRightFront); //平滑更新右前轮速度
    DriveCurrentRightRear = DriveControl_RampToTarget(DriveCurrentRightRear, DriveTargetRightRear); //平滑更新右后轮速度
    Set_PWM(DriveCurrentLeftRear, DriveCurrentLeftFront, DriveCurrentRightFront, DriveCurrentRightRear); //把四轮平滑速度写入验证过的方向和PWM驱动
}

/**
  * 函    数：读取当前四轮PWM值
  * 参    数：LeftRear 左后值，LeftFront 左前值，RightFront 右前值，RightRear 右后值
  * 返 回 值：无
  */
void DriveControl_GetCurrentPwm(int16_t *LeftRear, int16_t *LeftFront, int16_t *RightFront, int16_t *RightRear)
{
    if (LeftRear != 0)                                                                      //判断调用者是否提供左后结果地址
    {
        *LeftRear = DriveCurrentLeftRear;                                                   //返回当前左后轮平滑PWM值
    }
    if (LeftFront != 0)                                                                     //判断调用者是否提供左前结果地址
    {
        *LeftFront = DriveCurrentLeftFront;                                                  //返回当前左前轮平滑PWM值
    }
    if (RightFront != 0)                                                                    //判断调用者是否提供右前结果地址
    {
        *RightFront = DriveCurrentRightFront;                                                //返回当前右前轮平滑PWM值
    }
    if (RightRear != 0)                                                                     //判断调用者是否提供右后结果地址
    {
        *RightRear = DriveCurrentRightRear;                                                  //返回当前右后轮平滑PWM值
    }
}
