#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "Buzzer.h"                     //提供蜂鸣器驱动与无阻塞提示接口声明

typedef enum                             //定义蜂鸣器当前需要执行的提示模式
{
    BUZZER_ALERT_NONE = 0,               //表示当前没有障碍物提示请求
    BUZZER_ALERT_FRONT_SINGLE,           //表示前方障碍物每周期鸣响一次
    BUZZER_ALERT_REAR_DOUBLE             //表示后方障碍物每周期连续鸣响两次
} BuzzerAlertTypeDef;                    //结束蜂鸣器提示模式枚举定义

static volatile BuzzerAlertTypeDef BuzzerAlertMode = BUZZER_ALERT_NONE;                     //保存主循环提交给TIM6的当前提示模式
static volatile uint16_t BuzzerPatternTimeMs = 0U;                                          //保存当前一千毫秒提示周期内的时间位置

/**
  * 函    数：蜂鸣器初始化
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义GPIO初始化结构体

    /*开启GPIO时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);                                    //开启蜂鸣器控制引脚所在GPIOB端口时钟

    /*配置蜂鸣器控制引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;                                        //配置为推挽输出以驱动蜂鸣器三极管
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;                                               //选择蜂鸣器对应的PB14引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //设置GPIO输出速度为五十兆赫兹
    GPIO_Init(GPIOB, &GPIO_InitStructure);                                                   //将蜂鸣器引脚配置写入GPIOB

    /*设置上电默认状态*/
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);                                                      //PB14输出低电平并关闭蜂鸣器
    BuzzerAlertMode = BUZZER_ALERT_NONE;                                                     //清除上电时的蜂鸣器提示模式
    BuzzerPatternTimeMs = 0U;                                                                //清除蜂鸣器提示周期计时值
}

/**
  * 函    数：蜂鸣器开启
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_ON(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_14);                                                        //PB14输出高电平并开启蜂鸣器
}

/**
  * 函    数：蜂鸣器关闭
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_OFF(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_14);                                                      //PB14输出低电平并关闭蜂鸣器
}

/**
  * 函    数：蜂鸣器状态翻转
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Turn(void)
{
    if (GPIO_ReadOutputDataBit(GPIOB, GPIO_Pin_14) == Bit_RESET)                             //判断蜂鸣器当前是否关闭
    {
        Buzzer_ON();                                                                         //当前关闭时开启蜂鸣器
    }
    else                                                                                     //处理蜂鸣器当前已经开启的情况
    {
        Buzzer_OFF();                                                                        //当前开启时关闭蜂鸣器
    }
}

/**
  * 函    数：兼容旧接口的蜂鸣器定时闪鸣处理
  * 参    数：PeriodTicks 闪鸣半周期对应的调用次数，零表示常鸣
  * 返 回 值：无
  */
void Buzzer_Flash(uint16_t PeriodTicks)
{
    static uint16_t TickCount = 0U;                                                          //保存距离上次蜂鸣器翻转的调用次数

    if (PeriodTicks == 0U)                                                                   //判断是否请求蜂鸣器常鸣
    {
        Buzzer_ON();                                                                         //周期为零时保持蜂鸣器开启
        TickCount = 0U;                                                                      //清除累计值以便下次重新计时
    }
    else                                                                                     //处理正常的周期闪鸣请求
    {
        TickCount++;                                                                         //累计一次旧接口定时调用
        if (TickCount >= PeriodTicks)                                                        //判断是否达到设定的闪鸣半周期
        {
            TickCount = 0U;                                                                  //清零累计值并开始下一个半周期
            Buzzer_Turn();                                                                   //翻转蜂鸣器状态形成断续提示音
        }
    }
}

/**
  * 函    数：启动上电双响提示音
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Start(void)
{
    BuzzerAlertMode = BUZZER_ALERT_REAR_DOUBLE;                                             //复用双响模式输出两声上电提示音
    BuzzerPatternTimeMs = 0U;                                                                //从双响提示周期起点开始计时
}

/**
  * 函    数：兼容旧命名的蜂鸣器启动提示音
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_start(void)
{
    Buzzer_Start();                                                                          //调用规范命名接口并保持旧工程兼容性
}

/**
  * 函    数：设置前后障碍物蜂鸣器提示
  * 参    数：FrontBlocked 前方受阻标志，RearBlocked 后方受阻标志
  * 返 回 值：无
  */
void Buzzer_SetObstacleAlert(uint8_t FrontBlocked, uint8_t RearBlocked)
{
    BuzzerAlertTypeDef RequestedMode = BUZZER_ALERT_NONE;                                   //保存本次距离判断请求的提示模式

    if (RearBlocked != 0U)                                                                   //判断后方是否存在提示距离内障碍物
    {
        RequestedMode = BUZZER_ALERT_REAR_DOUBLE;                                           //后方受阻时选择优先级更高的循环双响模式
    }
    else if (FrontBlocked != 0U)                                                             //判断前方是否存在提示距离内障碍物
    {
        RequestedMode = BUZZER_ALERT_FRONT_SINGLE;                                          //前方受阻时选择循环单响模式
    }
    else                                                                                     //处理前后方向均没有近距离障碍物的情况
    {
        RequestedMode = BUZZER_ALERT_NONE;                                                  //没有前后障碍物时请求关闭蜂鸣器
    }

    if (RequestedMode != BuzzerAlertMode)                                                    //判断提示模式是否发生变化
    {
        BuzzerAlertMode = RequestedMode;                                                     //原子更新TIM6中断读取的提示模式
        BuzzerPatternTimeMs = 0U;                                                            //从新提示模式的周期起点重新计时
    }
}

/**
  * 函    数：蜂鸣器一毫秒无阻塞状态机
  * 参    数：无
  * 返 回 值：无
  */
void Buzzer_Tick1ms(void)
{
    BuzzerAlertTypeDef CurrentMode = BuzzerAlertMode;                                       //读取本次中断需要执行的稳定提示模式

    if (CurrentMode == BUZZER_ALERT_FRONT_SINGLE)                                            //判断是否执行前方障碍物单响提示
    {
        if (BuzzerPatternTimeMs < 100U)                                                      //判断是否位于每周期第一段一百毫秒鸣响窗口
        {
            Buzzer_ON();                                                                     //开启蜂鸣器产生一声短鸣
        }
        else                                                                                 //处理当前周期剩余静音时间
        {
            Buzzer_OFF();                                                                    //关闭蜂鸣器形成单响间隔
        }
    }
    else if (CurrentMode == BUZZER_ALERT_REAR_DOUBLE)                                        //判断是否执行后方障碍物双响提示
    {
        if (BuzzerPatternTimeMs < 100U || (BuzzerPatternTimeMs >= 200U && BuzzerPatternTimeMs < 300U)) //判断是否位于两段一百毫秒鸣响窗口
        {
            Buzzer_ON();                                                                     //开启蜂鸣器产生双响中的当前一声
        }
        else                                                                                 //处理两声之间和周期末尾的静音时间
        {
            Buzzer_OFF();                                                                    //关闭蜂鸣器形成双响间隔
        }
    }
    else                                                                                     //处理当前没有前后障碍物提示请求的情况
    {
        Buzzer_OFF();                                                                        //保持蜂鸣器关闭并避免阻塞主程序
    }

    BuzzerPatternTimeMs++;                                                                   //推进当前提示模式一毫秒时间位置
    if (BuzzerPatternTimeMs >= 1000U)                                                        //判断是否完成一千毫秒提示周期
    {
        BuzzerPatternTimeMs = 0U;                                                            //回到周期起点并重复对应单响或双响模式
    }
}
