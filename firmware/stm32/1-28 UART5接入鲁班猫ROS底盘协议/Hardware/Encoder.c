#include "stm32f10x.h"                                // 提供STM32F1寄存器与标准外设库定义
#include "Encoder.h"                                  // 提供四路编码器接口声明

#define ENCODER_COUNTER_PERIOD 65535U                  // 定义十六位编码器计数器自动重装值
#define ENCODER_INPUT_FILTER 0x0FU                     // 定义编码器输入数字滤波等级

/**
  * 函    数：配置通用定时器编码器接口
  * 参    数：Timer 目标定时器，GPIO 目标端口，Pins 两路输入引脚
  * 返 回 值：无
  */
static void Encoder_InitTimer(TIM_TypeDef *Timer, GPIO_TypeDef *GPIO, uint16_t Pins)
{
    GPIO_InitTypeDef GPIO_InitStructure;               // 定义编码器GPIO初始化结构体
    TIM_TimeBaseInitTypeDef TimeBaseStructure;         // 定义编码器时基初始化结构体
    TIM_ICInitTypeDef InputCaptureStructure;            // 定义编码器输入捕获初始化结构体

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;      // 配置编码器输入为内部上拉模式
    GPIO_InitStructure.GPIO_Pin = Pins;                 // 选择当前编码器两路输入引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  // 设置GPIO输入端口速度
    GPIO_Init(GPIO, &GPIO_InitStructure);               // 写入当前编码器GPIO配置

    TIM_TimeBaseStructInit(&TimeBaseStructure);        // 填充编码器时基结构体默认值
    TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 设置编码器采样时钟不分频
    TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 设置编码器接口计数模式
    TimeBaseStructure.TIM_Period = ENCODER_COUNTER_PERIOD; // 设置十六位计数器周期
    TimeBaseStructure.TIM_Prescaler = 0U;               // 不对编码器脉冲进行预分频
    TIM_TimeBaseInit(Timer, &TimeBaseStructure);        // 写入编码器定时器时基配置

    TIM_ICStructInit(&InputCaptureStructure);           // 填充输入捕获结构体默认值
    InputCaptureStructure.TIM_ICFilter = ENCODER_INPUT_FILTER; // 设置输入滤波抑制机械抖动
    InputCaptureStructure.TIM_Channel = TIM_Channel_1;  // 选择编码器第一路输入通道
    TIM_ICInit(Timer, &InputCaptureStructure);          // 初始化第一路编码器输入
    InputCaptureStructure.TIM_Channel = TIM_Channel_2;  // 选择编码器第二路输入通道
    TIM_ICInit(Timer, &InputCaptureStructure);          // 初始化第二路编码器输入
    TIM_EncoderInterfaceConfig(Timer, TIM_EncoderMode_TI1, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising); // 配置正交编码器接口模式
    TIM_SetCounter(Timer, 0U);                          // 清除上电时可能存在的计数残留
    TIM_Cmd(Timer, ENABLE);                             // 启动当前编码器定时器
}

/**
  * 函    数：初始化左后轮编码器
  * 参    数：无
  * 返 回 值：无
  */
void EncoderA_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); // 开启TIM3编码器时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 开启左后轮编码器GPIOA时钟
    Encoder_InitTimer(TIM3, GPIOA, GPIO_Pin_6 | GPIO_Pin_7); // 使用PA6和PA7初始化TIM3编码器
}

/**
  * 函    数：读取左后轮编码器增量
  * 参    数：无
  * 返 回 值：本次读取到的TIM3计数增量
  */
int16_t EncoderA_Get(void)
{
    int16_t Count = (int16_t)TIM_GetCounter(TIM3);      // 读取左后轮当前十六位计数值
    TIM_SetCounter(TIM3, 0U);                           // 清零TIM3以开始下一采样窗口
    return Count;                                       // 返回左后轮编码器增量
}

/**
  * 函    数：初始化左前轮编码器
  * 参    数：无
  * 返 回 值：无
  */
void EncoderB_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); // 开启TIM2编码器时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE); // 开启TIM2编码器端口与复用时钟
    GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2, ENABLE); // 将TIM2_CH1映射到PA15、TIM2_CH2映射到PB3
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 关闭JTAG并保留SWD下载调试功能
    Encoder_InitTimer(TIM2, GPIOA, GPIO_Pin_15);        // 初始化TIM2第一路PA15输入
    {                                                   // 开始配置TIM2第二路跨端口输入
        GPIO_InitTypeDef GPIO_InitStructure;            // 定义PB3编码器输入初始化结构体
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 配置PB3为内部上拉输入
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;      // 选择TIM2_CH2对应PB3引脚
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 设置PB3输入端口速度
        GPIO_Init(GPIOB, &GPIO_InitStructure);         // 写入PB3编码器输入配置
    }                                                   // 结束TIM2第二路输入配置
}

/**
  * 函    数：读取左前轮编码器增量
  * 参    数：无
  * 返 回 值：本次读取到的TIM2计数增量
  */
int16_t EncoderB_Get(void)
{
    int16_t Count = (int16_t)TIM_GetCounter(TIM2);      // 读取左前轮当前十六位计数值
    TIM_SetCounter(TIM2, 0U);                           // 清零TIM2以开始下一采样窗口
    return Count;                                       // 返回左前轮编码器增量
}

/**
  * 函    数：初始化右前轮编码器
  * 参    数：无
  * 返 回 值：无
  */
void EncoderC_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE); // 开启TIM4编码器时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // 开启右前轮编码器GPIOB时钟
    Encoder_InitTimer(TIM4, GPIOB, GPIO_Pin_6 | GPIO_Pin_7); // 使用PB6和PB7初始化TIM4编码器
}

/**
  * 函    数：读取右前轮编码器增量
  * 参    数：无
  * 返 回 值：本次读取到的TIM4计数增量
  */
int16_t EncoderC_Get(void)
{
    int16_t Count = (int16_t)TIM_GetCounter(TIM4);      // 读取右前轮当前十六位计数值
    TIM_SetCounter(TIM4, 0U);                           // 清零TIM4以开始下一采样窗口
    return Count;                                       // 返回右前轮编码器增量
}

/**
  * 函    数：初始化右后轮编码器
  * 参    数：无
  * 返 回 值：无
  */
void EncoderD_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE); // 开启TIM5编码器时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 开启右后轮编码器GPIOA时钟
    Encoder_InitTimer(TIM5, GPIOA, GPIO_Pin_0 | GPIO_Pin_1); // 使用PA0和PA1初始化TIM5编码器
}

/**
  * 函    数：读取右后轮编码器增量
  * 参    数：无
  * 返 回 值：本次读取到的TIM5计数增量
  */
int16_t EncoderD_Get(void)
{
    int16_t Count = (int16_t)TIM_GetCounter(TIM5);      // 读取右后轮当前十六位计数值
    TIM_SetCounter(TIM5, 0U);                           // 清零TIM5以开始下一采样窗口
    return Count;                                       // 返回右后轮编码器增量
}
