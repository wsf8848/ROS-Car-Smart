#include "stm32f10x.h"                                // 提供STM32F1寄存器与标准外设库定义
#include "Motor.h"                                    // 提供四路电机驱动接口声明
#include "PWM.h"                                      // 提供TIM8四路PWM输出接口

#define MOTOR_PWM_MAX 3599                             // 限制电机比较值不超过PWM周期

/**
  * 函    数：四路电机方向和PWM初始化
  * 参    数：无
  * 返 回 值：无
  */
void MotorABCD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;               // 定义电机方向GPIO初始化结构体

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE); // 开启电机方向引脚所在端口时钟
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;    // 配置方向引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11; // 选择PB0、PB1、PB5、PB8至PB11方向引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 设置PB方向引脚输出速度
    GPIO_Init(GPIOB, &GPIO_InitStructure);              // 写入GPIOB电机方向配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;           // 选择PC5对应右后轮方向引脚
    GPIO_Init(GPIOC, &GPIO_InitStructure);              // 写入GPIOC电机方向配置
    GPIO_ResetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11); // 关闭PB方向输出避免上电误动作
    GPIO_ResetBits(GPIOC, GPIO_Pin_5);                  // 关闭PC5方向输出避免上电误动作
    PWM_ABCD_Init();                                    // 初始化TIM8四路电机PWM输出
}

/**
  * 函    数：限制PWM比较值
  * 参    数：Speed 有符号PWM速度值
  * 返 回 值：零至最大PWM比较值
  */
static uint16_t Motor_ClampPwm(int16_t Speed)
{
    int32_t Magnitude = Speed;                          // 使用更宽类型保存取绝对值过程

    if (Magnitude < 0)                                  // 判断当前速度是否为反向值
    {
        Magnitude = -Magnitude;                         // 将反向速度转换为比较值幅值
    }
    if (Magnitude > MOTOR_PWM_MAX)                      // 判断幅值是否超过PWM周期
    {
        Magnitude = MOTOR_PWM_MAX;                      // 将超范围值限制为最大比较值
    }
    return (uint16_t)Magnitude;                         // 返回安全的PWM比较值
}

/**
  * 函    数：设置左后轮速度
  * 参    数：Speed 正值按验证极性前进，负值反向，零值停止
  * 返 回 值：无
  */
void MotorA_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                     // 判断左后轮是否需要正向运行
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);                // 设置左后轮正向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);              // 清除左后轮反向控制引脚
        PWM_SetCompare1(Motor_ClampPwm(Speed));         // 写入左后轮正向PWM比较值
    }
    else                                                // 处理左后轮反向运行请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_1);                // 设置左后轮反向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);              // 清除左后轮正向控制引脚
        PWM_SetCompare1(Motor_ClampPwm(Speed));         // 写入左后轮反向PWM比较值
    }
}

/**
  * 函    数：设置左前轮速度
  * 参    数：Speed 正值按验证极性前进，负值反向，零值停止
  * 返 回 值：无
  */
void MotorB_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                     // 判断左前轮是否需要正向运行
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_9);                // 设置左前轮正向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_8);              // 清除左前轮反向控制引脚
        PWM_SetCompare2(Motor_ClampPwm(Speed));         // 写入左前轮正向PWM比较值
    }
    else                                                // 处理左前轮反向运行请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_8);                // 设置左前轮反向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_9);              // 清除左前轮正向控制引脚
        PWM_SetCompare2(Motor_ClampPwm(Speed));         // 写入左前轮反向PWM比较值
    }
}

/**
  * 函    数：设置右前轮速度
  * 参    数：Speed 正值按验证极性前进，负值反向，零值停止
  * 返 回 值：无
  */
void MotorC_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                     // 判断右前轮是否需要正向运行
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_11);               // 设置右前轮正向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_10);             // 清除右前轮反向控制引脚
        PWM_SetCompare3(Motor_ClampPwm(Speed));         // 写入右前轮正向PWM比较值
    }
    else                                                // 处理右前轮反向运行请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_10);               // 设置右前轮反向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_11);             // 清除右前轮正向控制引脚
        PWM_SetCompare3(Motor_ClampPwm(Speed));         // 写入右前轮反向PWM比较值
    }
}

/**
  * 函    数：设置右后轮速度
  * 参    数：Speed 正值按验证极性前进，负值反向，零值停止
  * 返 回 值：无
  */
void MotorD_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                     // 判断右后轮是否需要正向运行
    {
        GPIO_SetBits(GPIOC, GPIO_Pin_5);                // 设置右后轮正向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);              // 清除右后轮反向控制引脚
        PWM_SetCompare4(Motor_ClampPwm(Speed));         // 写入右后轮正向PWM比较值
    }
    else                                                // 处理右后轮反向运行请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_5);                // 设置右后轮反向控制引脚
        GPIO_ResetBits(GPIOC, GPIO_Pin_5);              // 清除右后轮正向控制引脚
        PWM_SetCompare4(Motor_ClampPwm(Speed));         // 写入右后轮反向PWM比较值
    }
}

/**
  * 函    数：同时设置四个轮子的速度
  * 参    数：PWM1至PWM4 分别对应左后、左前、右前、右后轮的有符号PWM值
  * 返 回 值：无
  */
void Set_PWM(int16_t PWM1, int16_t PWM2, int16_t PWM3, int16_t PWM4)
{
    MotorA_SetSpeed(PWM1);                              // 更新左后轮方向和PWM
    MotorB_SetSpeed(PWM2);                              // 更新左前轮方向和PWM
    MotorC_SetSpeed(PWM3);                              // 更新右前轮方向和PWM
    MotorD_SetSpeed(PWM4);                              // 更新右后轮方向和PWM
}
