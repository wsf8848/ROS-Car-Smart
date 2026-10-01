#include "stm32f10x.h"                                // 提供STM32F1寄存器与标准外设库定义
#include "ServoTest.h"                                // 提供六路舵机测试接口声明

#define SERVO_TIMER_PRESCALER 71U                       // 将72MHz定时器时钟分频为1MHz计数频率
#define SERVO_TIMER_PERIOD 19999U                       // 产生20ms周期即50Hz舵机控制帧
#define SERVO_MIN_PULSE_US 500U                         // 定义零度对应的最短脉宽
#define SERVO_PULSE_RANGE_US 2000U                      // 定义零至180度覆盖的脉宽范围
#define SERVO_MAX_ANGLE 180U                            // 定义舵机允许的最大角度

/**
  * 函    数：配置舵机定时器时基
  * 参    数：Timer 需要配置的TIM1或TIM8
  * 返 回 值：无
  */
static void ServoTest_InitTimeBase(TIM_TypeDef *Timer)
{
    TIM_TimeBaseInitTypeDef TimeBaseStructure;          // 定义舵机PWM时基配置结构体

    TIM_TimeBaseStructInit(&TimeBaseStructure);         // 使用标准库默认值初始化时基结构体
    TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 设置定时器采样时钟不分频
    TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 设置定时器向上计数
    TimeBaseStructure.TIM_Period = SERVO_TIMER_PERIOD;  // 设置20ms舵机帧周期
    TimeBaseStructure.TIM_Prescaler = SERVO_TIMER_PRESCALER; // 设置每个计数对应一微秒
    TIM_TimeBaseInit(Timer, &TimeBaseStructure);        // 将时基参数写入目标定时器
}

/**
  * 函    数：配置舵机PWM输出通道
  * 参    数：Timer 目标定时器
  * 返 回 值：无
  */
static void ServoTest_InitChannels(TIM_TypeDef *Timer)
{
    TIM_OCInitTypeDef OutputCompareStructure;           // 定义舵机PWM输出比较结构体

    TIM_OCStructInit(&OutputCompareStructure);          // 使用标准库默认值初始化输出比较结构体
    OutputCompareStructure.TIM_OCMode = TIM_OCMode_PWM1; // 设置高有效PWM1输出模式
    OutputCompareStructure.TIM_OutputState = TIM_OutputState_Enable; // 使能主输出通道
    OutputCompareStructure.TIM_Pulse = 1500U;           // 初始化为90度中位脉宽
    OutputCompareStructure.TIM_OCPolarity = TIM_OCPolarity_High; // 设置舵机脉冲高电平有效
    TIM_OC1Init(Timer, &OutputCompareStructure);        // 初始化目标定时器通道1
    TIM_OC2Init(Timer, &OutputCompareStructure);        // 初始化目标定时器通道2
    TIM_OC1PreloadConfig(Timer, TIM_OCPreload_Enable);  // 使能通道1比较值预装载
    TIM_OC2PreloadConfig(Timer, TIM_OCPreload_Enable);  // 使能通道2比较值预装载
    if (Timer == TIM1)                                  // 判断是否正在配置四通道TIM1
    {
        TIM_OC3Init(Timer, &OutputCompareStructure);    // 初始化TIM1通道3
        TIM_OC4Init(Timer, &OutputCompareStructure);    // 初始化TIM1通道4
        TIM_OC3PreloadConfig(Timer, TIM_OCPreload_Enable); // 使能TIM1通道3比较值预装载
        TIM_OC4PreloadConfig(Timer, TIM_OCPreload_Enable); // 使能TIM1通道4比较值预装载
    }
    TIM_ARRPreloadConfig(Timer, ENABLE);                // 使能自动重装值预装载
    TIM_Cmd(Timer, ENABLE);                             // 启动目标定时器
    TIM_CtrlPWMOutputs(Timer, ENABLE);                  // 打开高级定时器主输出开关
}

/**
  * 函    数：初始化六路舵机测试信号
  * 参    数：无
  * 返 回 值：无
  */
void ServoTest_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                // 定义舵机信号GPIO配置结构体

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1 | RCC_APB2Periph_TIM8, ENABLE); // 开启舵机端口和高级定时器时钟
    TIM_Cmd(TIM8, DISABLE);                             // 停止此前用于电机PWM的TIM8计数器
    TIM_CtrlPWMOutputs(TIM8, DISABLE);                  // 在重配前关闭TIM8主输出
    TIM_DeInit(TIM8);                                   // 清除电机PWM留下的TIM8配置
    TIM_DeInit(TIM1);                                   // 清除USART1阶段之后可能存在的TIM1配置
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;     // 配置舵机信号为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // 选择S1和S2对应的PC6与PC7
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 设置舵机信号引脚输出速度
    GPIO_Init(GPIOC, &GPIO_InitStructure);              // 写入S1和S2信号引脚配置
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11; // 选择S3至S6对应的PA8至PA11
    GPIO_Init(GPIOA, &GPIO_InitStructure);              // 写入S3至S6信号引脚配置
    ServoTest_InitTimeBase(TIM8);                       // 配置S1和S2使用的TIM8时基
    ServoTest_InitChannels(TIM8);                       // 开启TIM8前两路50Hz PWM输出
    ServoTest_InitTimeBase(TIM1);                       // 配置S3至S6使用的TIM1时基
    ServoTest_InitChannels(TIM1);                       // 开启TIM1四路50Hz PWM输出
}

/**
  * 函    数：设置指定舵机角度
  * 参    数：Channel 通道一至六，Angle 角度零至180度
  * 返 回 值：无
  */
void ServoTest_SetAngle(uint8_t Channel, uint16_t Angle)
{
    uint16_t PulseWidth;                                // 保存换算后的舵机脉宽微秒数

    if (Angle > SERVO_MAX_ANGLE)                        // 判断角度是否超过测试安全上限
    {
        Angle = SERVO_MAX_ANGLE;                        // 将超范围角度限制为180度
    }
    PulseWidth = (uint16_t)(SERVO_MIN_PULSE_US + ((uint32_t)Angle * SERVO_PULSE_RANGE_US) / SERVO_MAX_ANGLE); // 将角度线性换算为500至2500微秒脉宽
    switch (Channel)                                    // 按板载舵机接口编号选择输出比较寄存器
    {
        case 1U: TIM_SetCompare1(TIM8, PulseWidth); break; // 更新S1对应的PC6输出
        case 2U: TIM_SetCompare2(TIM8, PulseWidth); break; // 更新S2对应的PC7输出
        case 3U: TIM_SetCompare1(TIM1, PulseWidth); break; // 更新S3对应的PA8输出
        case 4U: TIM_SetCompare2(TIM1, PulseWidth); break; // 更新S4对应的PA9输出
        case 5U: TIM_SetCompare3(TIM1, PulseWidth); break; // 更新S5对应的PA10输出
        case 6U: TIM_SetCompare4(TIM1, PulseWidth); break; // 更新S6对应的PA11输出
        default: break;                                 // 忽略零或大于六的无效通道号
    }
}

/**
  * 函    数：设置全部舵机角度
  * 参    数：Angle 六路共用目标角度
  * 返 回 值：无
  */
void ServoTest_SetAll(uint16_t Angle)
{
    uint8_t Channel;                                    // 保存当前正在设置的舵机通道号

    for (Channel = 1U; Channel <= 6U; Channel++)        // 依次遍历六路舵机输出
    {
        ServoTest_SetAngle(Channel, Angle);             // 将当前舵机设置为指定角度
    }
}

/**
  * 函    数：关闭六路舵机测试信号
  * 参    数：无
  * 返 回 值：无
  */
void ServoTest_DeInit(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                // 定义舵机引脚释放配置结构体

    TIM_CtrlPWMOutputs(TIM8, DISABLE);                  // 关闭S1和S2对应的TIM8主输出
    TIM_CtrlPWMOutputs(TIM1, DISABLE);                  // 关闭S3至S6对应的TIM1主输出
    TIM_Cmd(TIM8, DISABLE);                             // 停止TIM8舵机计数器
    TIM_Cmd(TIM1, DISABLE);                             // 停止TIM1舵机计数器
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;       // 将停止后的舵机信号脚设为高阻模拟输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7; // 选择S1和S2信号脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 填写GPIO速度字段保持结构完整
    GPIO_Init(GPIOC, &GPIO_InitStructure);              // 释放PC6和PC7舵机信号脚
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11; // 选择S3至S6信号脚
    GPIO_Init(GPIOA, &GPIO_InitStructure);              // 释放PA8至PA11舵机信号脚
}
