#include "stm32f10x.h"                                // 提供STM32F1寄存器与标准外设库定义
#include "Timer.h"                                    // 提供TIM6毫秒时基驱动接口声明
#include "Buzzer.h"                                   // 提供蜂鸣器一毫秒无阻塞状态机接口

#define TIMER_COUNTER_CLOCK_HZ 1000000U                // 定义TIM6预分频后的计数频率为一兆赫兹
#define TIMER_SYSTEM_PERIOD_MS 1U                     // 定义TIM6系统节拍周期为一毫秒
#define TIMER_BLUETOOTH_PERIOD_MS 10U                 // 定义蓝牙遥测发送周期为十毫秒

static volatile uint32_t TimerMilliseconds = 0UL;      // 保存系统上电后累计运行的毫秒数
static volatile uint8_t TimerBluetooth10msFlag = 0U;   // 标记蓝牙遥测数据包已经达到十毫秒发送周期
static volatile uint8_t TimerBluetoothDivider = 0U;    // 累计TIM6一毫秒中断并生成十毫秒蓝牙节拍

/**
  * 函    数：TIM6一毫秒系统时基初始化
  * 参    数：无
  * 返 回 值：无
  */
void Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef TimeBaseStructure;         // 定义TIM6时基初始化结构体
    NVIC_InitTypeDef NVIC_InitStructure;               // 定义TIM6中断初始化结构体

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE); // 开启APB1总线上的TIM6外设时钟
    TIM_InternalClockConfig(TIM6);                      // 选择TIM6内部时钟作为计数时钟源
    TIM_TimeBaseStructInit(&TimeBaseStructure);         // 填充TIM6时基结构体默认值
    TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 配置定时器数字滤波采样时钟不分频
    TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 配置TIM6向上计数模式
    TimeBaseStructure.TIM_Period = TIMER_SYSTEM_PERIOD_MS * TIMER_COUNTER_CLOCK_HZ / 1000U - 1U; // 设置一毫秒更新周期
    TimeBaseStructure.TIM_Prescaler = 72U - 1U;         // 将七十二兆赫兹定时器时钟预分频为一兆赫兹
    TimeBaseStructure.TIM_RepetitionCounter = 0U;       // 基本定时器不使用重复计数功能
    TIM_TimeBaseInit(TIM6, &TimeBaseStructure);          // 将一毫秒时基参数写入TIM6

    TIM_ClearFlag(TIM6, TIM_FLAG_Update);               // 清除初始化过程中产生的更新标志
    TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);          // 开启TIM6更新中断请求
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);     // 使用两位抢占和两位响应优先级分组
    NVIC_InitStructure.NVIC_IRQChannel = TIM6_IRQn;     // 选择TIM6全局中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;     // 使能TIM6中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2U; // 设置TIM6低于雷达串口接收的抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0U; // 设置TIM6响应优先级为零
    NVIC_Init(&NVIC_InitStructure);                      // 将TIM6中断参数写入NVIC

    TimerMilliseconds = 0UL;                             // 清除系统毫秒累计值
    TimerBluetooth10msFlag = 0U;                         // 清除蓝牙十毫秒发送请求标志
    TimerBluetoothDivider = 0U;                          // 清除蓝牙十毫秒分频计数值
    TIM_Cmd(TIM6, ENABLE);                               // 启动TIM6并持续产生一毫秒更新中断
}

/**
  * 函    数：读取系统毫秒时间
  * 参    数：无
  * 返 回 值：系统上电后累计运行的毫秒数
  */
uint32_t Timer_GetMs(void)
{
    return TimerMilliseconds;                            // 返回单次原子读取的三十二位系统毫秒时间
}

/**
  * 函    数：取得蓝牙十毫秒发送请求
  * 参    数：无
  * 返 回 值：一表示需要发送一个数据包，零表示尚未达到发送周期
  */
uint8_t Timer_TakeBluetooth10msFlag(void)
{
    uint8_t RequestFlag;                                 // 保存进入临界区后读取的十毫秒请求状态
    uint32_t InterruptState = __get_PRIMASK();           // 保存调用前的全局中断屏蔽状态

    __disable_irq();                                     // 短暂屏蔽中断以避免读取清除过程丢失新请求
    RequestFlag = TimerBluetooth10msFlag;                // 读取TIM6发布的十毫秒请求标志
    TimerBluetooth10msFlag = 0U;                         // 清除已由主循环领取的发送请求
    if (InterruptState == 0UL)                           // 判断调用前是否允许全局中断
    {
        __enable_irq();                                  // 恢复调用前允许中断的运行状态
    }
    return RequestFlag;                                  // 返回本次是否需要发送蓝牙遥测数据包
}

/**
  * 函    数：TIM6一毫秒更新中断服务
  * 参    数：无
  * 返 回 值：无
  */
void TIM6_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM6, TIM_IT_Update) != RESET)  // 判断TIM6是否产生更新中断
    {
        TIM_ClearITPendingBit(TIM6, TIM_IT_Update);     // 清除TIM6更新中断挂起标志
        TimerMilliseconds++;                             // 累计一个系统运行毫秒
        Buzzer_Tick1ms();                                // 推进蜂鸣器单响或双响无阻塞状态机
        TimerBluetoothDivider++;                         // 累计一个蓝牙遥测发送分频毫秒
        if (TimerBluetoothDivider >= TIMER_BLUETOOTH_PERIOD_MS) // 判断是否达到十毫秒蓝牙发送周期
        {
            TimerBluetoothDivider = 0U;                 // 清零分频计数并开始下一个十毫秒周期
            TimerBluetooth10msFlag = 1U;                // 通知主循环发送一个最新状态数据包
        }
    }
}
