#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Timer.h"                      //提供TIM2毫秒时基驱动接口声明
#include "Buzzer.h"                     //提供蜂鸣器一毫秒无阻塞状态机接口

static volatile uint32_t TimerMilliseconds = 0UL;                                          //保存系统上电后累计运行的毫秒数
static volatile uint8_t TimerBluetooth10msFlag = 0U;                                       //标记蓝牙遥测数据包已经达到十毫秒发送周期
static volatile uint8_t TimerBluetoothDivider = 0U;                                        //累计TIM2一毫秒中断并生成十毫秒蓝牙节拍

/**
  * 函    数：TIM2一毫秒系统时基初始化
  * 参    数：无
  * 返 回 值：无
  */
void Timer_Init(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;                                      //定义TIM2时基初始化结构体
    NVIC_InitTypeDef NVIC_InitStructure;                                                    //定义TIM2中断初始化结构体

    /*开启TIM2外设时钟*/
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);                                     //开启APB1总线上的TIM2外设时钟
    TIM_InternalClockConfig(TIM2);                                                           //选择TIM2内部时钟作为计数时钟源

    /*配置一毫秒更新周期*/
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;                              //配置定时器数字滤波采样时钟不分频
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;                          //配置TIM2向上计数模式
    TIM_TimeBaseInitStructure.TIM_Period = 1000U - 1U;                                      //设置一千次计数产生一次更新事件
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72U - 1U;                                     //将七十二兆赫兹定时器时钟预分频为一兆赫兹
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0U;                                   //通用定时器不使用重复计数功能
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);                                     //将一毫秒时基参数写入TIM2

    /*配置TIM2更新中断*/
    TIM_ClearFlag(TIM2, TIM_FLAG_Update);                                                    //清除初始化过程中产生的更新标志
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);                                               //开启TIM2更新中断请求
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);                                          //使用两位抢占和两位响应优先级分组
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;                                          //选择TIM2全局中断通道
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                                          //使能TIM2中断通道
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2U;                               //设置TIM2低于雷达串口接收的抢占优先级
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0U;                                      //设置TIM2响应优先级为零
    NVIC_Init(&NVIC_InitStructure);                                                          //将TIM2中断参数写入NVIC

    /*启动系统毫秒时基*/
    TimerMilliseconds = 0UL;                                                                //清除系统毫秒累计值
    TimerBluetooth10msFlag = 0U;                                                            //清除蓝牙十毫秒发送请求标志
    TimerBluetoothDivider = 0U;                                                             //清除蓝牙十毫秒分频计数值
    TIM_Cmd(TIM2, ENABLE);                                                                   //启动TIM2并持续产生一毫秒更新中断
}

/**
  * 函    数：读取系统毫秒时间
  * 参    数：无
  * 返 回 值：系统上电后累计运行的毫秒数
  */
uint32_t Timer_GetMs(void)
{
    return TimerMilliseconds;                                                               //返回单次原子读取的三十二位系统毫秒时间
}

/**
  * 函    数：取得蓝牙十毫秒发送请求
  * 参    数：无
  * 返 回 值：一表示需要发送一个数据包，零表示尚未达到发送周期
  */
uint8_t Timer_TakeBluetooth10msFlag(void)
{
    uint8_t RequestFlag;                                                                    //保存进入临界区后读取的十毫秒请求状态
    uint32_t InterruptState = __get_PRIMASK();                                               //保存调用前的全局中断屏蔽状态

    __disable_irq();                                                                         //短暂屏蔽中断以避免读取清除过程丢失新请求
    RequestFlag = TimerBluetooth10msFlag;                                                    //读取TIM2发布的十毫秒请求标志
    TimerBluetooth10msFlag = 0U;                                                            //清除已由主循环领取的发送请求
    if (InterruptState == 0UL)                                                               //判断调用前是否允许全局中断
    {
        __enable_irq();                                                                      //恢复调用前允许中断的运行状态
    }
    return RequestFlag;                                                                      //返回本次是否需要发送蓝牙遥测数据包
}

/**
  * 函    数：TIM2一毫秒更新中断服务
  * 参    数：无
  * 返 回 值：无
  */
void TIM2_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)                                      //判断TIM2是否产生更新中断
    {
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);                                         //清除TIM2更新中断挂起标志
        TimerMilliseconds++;                                                                //累计一个系统运行毫秒
        Buzzer_Tick1ms();                                                                    //推进蜂鸣器单响或双响无阻塞状态机
        TimerBluetoothDivider++;                                                            //累计一个蓝牙遥测发送分频毫秒
        if (TimerBluetoothDivider >= 100U)                                                   //判断是否达到十毫秒蓝牙发送周期
        {
            TimerBluetoothDivider = 0U;                                                     //清零分频计数并开始下一个十毫秒周期
            TimerBluetooth10msFlag = 1U;                                                    //通知主循环发送一个最新状态数据包
        }
    }
}
