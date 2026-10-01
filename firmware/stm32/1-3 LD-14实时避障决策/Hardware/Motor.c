#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Motor.h"                     //提供四电机方向和速度接口声明
#include "PWM.h"                       //提供TIM8四通道PWM输出接口

#define MOTOR_COMPARE_LIMIT 1800U       //定义当前车辆控制允许的最大PWM比较值

/**
  * 函    数：四电机方向与PWM初始化
  * 参    数：无
  * 返 回 值：无
  */
void MotorABCD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义电机方向GPIO初始化结构体

    /*开启方向引脚所在端口时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, ENABLE);             //开启GPIOB和GPIOC外设时钟

    /*配置左后和左前方向引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;                                        //配置电机方向引脚为推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_9;         //选择左后PB0/PB1和左前PB8/PB9方向引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //设置方向引脚输出速度为五十兆赫兹
    GPIO_Init(GPIOB, &GPIO_InitStructure);                                                   //将左后和左前方向引脚配置写入GPIOB

    /*配置右前和右后方向引脚*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_10 | GPIO_Pin_11;                    //选择右后PB5/PC5和右前PB10/PB11方向引脚中的GPIOB部分
    GPIO_Init(GPIOB, &GPIO_InitStructure);                                                   //将右前和右后GPIOB方向引脚配置写入GPIOB
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;                                                //选择右后电机PC5方向引脚
    GPIO_Init(GPIOC, &GPIO_InitStructure);                                                   //将右后PC5方向引脚配置写入GPIOC

    /*设置所有方向和PWM为安全停止状态*/
    GPIO_ResetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_5 | GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11); //清除GPIOB方向输出
    GPIO_ResetBits(GPIOC, GPIO_Pin_5);                                                       //清除GPIOC右后方向输出
    PWM_ABCD_Init();                                                                         //初始化TIM8四路电机PWM
    Set_PWM(0, 0, 0, 0);                                                                     //确保四个电机上电保持停止
}

/**
  * 函    数：限制电机PWM比较值
  * 参    数：Speed 待限制的有符号速度
  * 返 回 值：限制到当前PWM安全范围后的绝对比较值
  */
static uint16_t Motor_GetCompare(int16_t Speed)
{
    int32_t Magnitude = Speed;                                                               //复制待转换的有符号速度值

    if (Magnitude < 0)                                                                       //判断速度是否为负方向
    {
        Magnitude = -Magnitude;                                                             //取负速度绝对值用于PWM比较
    }
    if (Magnitude > MOTOR_COMPARE_LIMIT)                                                     //判断速度是否超过当前安全PWM上限
    {
        Magnitude = MOTOR_COMPARE_LIMIT;                                                     //将速度限制到一千八百比较值
    }
    return (uint16_t)Magnitude;                                                             //返回限制后的PWM比较值
}

/**
  * 函    数：设置左后电机速度
  * 参    数：Speed 正值前进方向，负值后退方向
  * 返 回 值：无
  */
void MotorA_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                                                          //判断左后电机是否请求正方向
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_0);                                                     //设置左后电机正方向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_1);                                                   //清除左后电机反方向控制引脚
    }
    else                                                                                     //处理左后电机负方向请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_1);                                                     //设置左后电机反方向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_0);                                                   //清除左后电机正方向控制引脚
    }
    PWM_SetCompare1(Motor_GetCompare(Speed));                                                //将左后电机速度绝对值写入TIM8通道一
}

/**
  * 函    数：设置左前电机速度
  * 参    数：Speed 正值前进方向，负值后退方向
  * 返 回 值：无
  */
void MotorB_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                                                          //判断左前电机是否请求正方向
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_9);                                                     //设置左前电机正方向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_8);                                                   //清除左前电机反方向控制引脚
    }
    else                                                                                     //处理左前电机负方向请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_8);                                                     //设置左前电机反方向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_9);                                                   //清除左前电机正方向控制引脚
    }
    PWM_SetCompare2(Motor_GetCompare(Speed));                                                //将左前电机速度绝对值写入TIM8通道二
}

/**
  * 函    数：设置右前电机速度
  * 参    数：Speed 正值前进方向，负值后退方向
  * 返 回 值：无
  */
void MotorC_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                                                          //判断右前电机是否请求正方向
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_11);                                                    //设置右前电机正方向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_10);                                                  //清除右前电机反方向控制引脚
    }
    else                                                                                     //处理右前电机负方向请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_10);                                                    //设置右前电机反方向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_11);                                                  //清除右前电机正方向控制引脚
    }
    PWM_SetCompare3(Motor_GetCompare(Speed));                                                //将右前电机速度绝对值写入TIM8通道三
}

/**
  * 函    数：设置右后电机速度
  * 参    数：Speed 正值前进方向，负值后退方向
  * 返 回 值：无
  */
void MotorD_SetSpeed(int16_t Speed)
{
    if (Speed >= 0)                                                                          //判断右后电机是否请求正方向
    {
        GPIO_SetBits(GPIOC, GPIO_Pin_5);                                                     //设置右后电机正方向控制引脚
        GPIO_ResetBits(GPIOB, GPIO_Pin_5);                                                   //清除右后电机反方向控制引脚
    }
    else                                                                                     //处理右后电机负方向请求
    {
        GPIO_SetBits(GPIOB, GPIO_Pin_5);                                                     //设置右后电机反方向控制引脚
        GPIO_ResetBits(GPIOC, GPIO_Pin_5);                                                   //清除右后电机正方向控制引脚
    }
    PWM_SetCompare4(Motor_GetCompare(Speed));                                                //将右后电机速度绝对值写入TIM8通道四
}

/**
  * 函    数：同时设置四个电机速度
  * 参    数：MotorA 左后速度，MotorB 左前速度，MotorC 右前速度，MotorD 右后速度
  * 返 回 值：无
  */
void Set_PWM(int16_t MotorA, int16_t MotorB, int16_t MotorC, int16_t MotorD)
{
    MotorA_SetSpeed(MotorA);                                                                 //更新左后电机方向和速度
    MotorB_SetSpeed(MotorB);                                                                 //更新左前电机方向和速度
    MotorC_SetSpeed(MotorC);                                                                 //更新右前电机方向和速度
    MotorD_SetSpeed(MotorD);                                                                 //更新右后电机方向和速度
}
