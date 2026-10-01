#include "WS2812.h"                                   // 提供灯带参数、模式类型和公开接口声明
#include "Delay.h"                                    // 提供帧结束后的微秒级复位锁存延时

#define WS2812_GPIO_PORT GPIOA                          // 定义数据输出端口为GPIOA
#define WS2812_GPIO_CLOCK RCC_APB2Periph_GPIOA          // 定义GPIOA外设时钟位
#define WS2812_GPIO_PIN GPIO_Pin_11                     // 定义数据输出引脚为PA11
#define WS2812_TIMER TIM1                               // 定义波形发生器使用TIM1
#define WS2812_TIMER_CLOCK RCC_APB2Periph_TIM1          // 定义TIM1外设时钟位
#define WS2812_TIMER_DMA_SOURCE TIM_DMA_CC4             // 定义TIM1通道4比较事件触发DMA
#define WS2812_DMA_CHANNEL DMA1_Channel4                // 定义TIM1通道4对应DMA1通道4
#define WS2812_DMA_TRANSFER_FLAG DMA1_FLAG_TC4          // 定义DMA1通道4传输完成标志
#define WS2812_DMA_CLEAR_FLAGS DMA1_FLAG_GL4            // 定义DMA1通道4全部标志清除掩码
#define WS2812_SIGNAL_FREQUENCY_HZ 800000UL             // 定义单线协议码元频率为八百千赫兹
#define WS2812_RESET_TIME_US 280U                       // 定义已验证的二百八十微秒低电平锁存时间
#define WS2812_ANIMATION_PERIOD_MS 100UL                // 定义动画每帧间隔一百毫秒
#define WS2812_COLOR_LEVELS 16U                         // 定义每段颜色渐变使用十六个等级
#define WS2812_COLOR_STEP (255U / WS2812_COLOR_LEVELS) // 定义每帧颜色亮度变化步长
#define WS2812_COLOR_BITS 8U                            // 定义每个颜色分量占用八位数据
#define WS2812_MSB_MASK 0x80U                           // 定义颜色字节最高位掩码
#define WS2812_DMA_WAIT_LIMIT 1000000UL                 // 定义DMA异常时的防死锁等待上限

typedef struct                                          // 定义流水灯头部颜色渐变状态
{
    uint8_t Green;                                      // 保存当前绿色亮度
    uint8_t Red;                                        // 保存当前红色亮度
    uint8_t Blue;                                       // 保存当前蓝色亮度
    uint8_t Phase;                                      // 保存三段循环渐变的当前阶段
} WS2812ColorStateTypeDef;                              // 结束流水灯颜色状态类型定义

static uint16_t WS2812Data[WS2812_DATA_LENGTH];         // 保存八颗灯珠GRB数据和帧尾复位码元
static WS2812ModeTypeDef WS2812Mode = WS2812_MODE_WATER; // 保存当前模式并默认选择流水灯
static WS2812ColorStateTypeDef WS2812Color = {255U, 0U, 0U, 0U}; // 保存从绿色开始的流水颜色
static uint8_t WS2812FlashState = 0U;                   // 保存全灯闪烁的当前亮灭状态
static uint32_t WS2812LastFrameMs = 0UL;                // 保存上一次动画刷新时间

/**
  * 函    数：清空全部灯珠数据并生成帧尾复位低电平
  * 参    数：无
  * 返 回 值：无
  */
static void WS2812_ClearPixels(void)
{
    uint16_t DataIndex;                                 // 保存当前处理的DMA数组下标

    for (DataIndex = 0U; DataIndex < WS2812_LED_COUNT * WS2812_BITS_PER_LED; DataIndex++) // 遍历全部灯珠颜色码元
    {
        WS2812Data[DataIndex] = WS2812_PWM_DUTY_0;      // 写入逻辑零占空比使颜色位清零
    }
    for (; DataIndex < WS2812_DATA_LENGTH; DataIndex++) // 遍历灯珠数据后的复位码元
    {
        WS2812Data[DataIndex] = 0U;                     // 写入零占空比形成帧尾持续低电平
    }
}

/**
  * 函    数：填写单颗灯珠的GRB颜色数据
  * 参    数：Green绿色，Red红色，Blue蓝色，Position灯珠位置
  * 返 回 值：无
  */
static void WS2812_FillColor(uint8_t Green, uint8_t Red, uint8_t Blue, uint16_t Position)
{
    uint8_t BitIndex;                                   // 保存当前转换的颜色位下标
    uint16_t DataStart;                                 // 保存目标灯珠的DMA数据起点

    if (Position >= WS2812_LED_COUNT)                   // 判断目标位置是否超出灯带范围
    {
        return;                                         // 忽略越界位置以保护DMA缓冲区
    }
    DataStart = Position * WS2812_BITS_PER_LED;         // 计算目标灯珠二十四位数据的起点
    for (BitIndex = 0U; BitIndex < WS2812_COLOR_BITS; BitIndex++) // 按最高位在前顺序转换三个颜色分量
    {
        WS2812Data[DataStart + BitIndex] = (((uint16_t)Green << BitIndex) & WS2812_MSB_MASK) != 0U ? WS2812_PWM_DUTY_1 : WS2812_PWM_DUTY_0; // 编码绿色数据位
        WS2812Data[DataStart + WS2812_COLOR_BITS + BitIndex] = (((uint16_t)Red << BitIndex) & WS2812_MSB_MASK) != 0U ? WS2812_PWM_DUTY_1 : WS2812_PWM_DUTY_0; // 编码红色数据位
        WS2812Data[DataStart + WS2812_COLOR_BITS * 2U + BitIndex] = (((uint16_t)Blue << BitIndex) & WS2812_MSB_MASK) != 0U ? WS2812_PWM_DUTY_1 : WS2812_PWM_DUTY_0; // 编码蓝色数据位
    }
}

/**
  * 函    数：使用同一种颜色填充全部灯珠
  * 参    数：Green绿色，Red红色，Blue蓝色
  * 返 回 值：无
  */
static void WS2812_FillAll(uint8_t Green, uint8_t Red, uint8_t Blue)
{
    uint16_t Position;                                  // 保存当前填充的灯珠位置

    for (Position = 0U; Position < WS2812_LED_COUNT; Position++) // 遍历全部八颗灯珠
    {
        WS2812_FillColor(Green, Red, Blue, Position);   // 将指定颜色写入当前灯珠
    }
}

/**
  * 函    数：通过TIM1通道4和DMA发送一帧灯带数据
  * 参    数：无
  * 返 回 值：无
  */
static void WS2812_Show(void)
{
    uint32_t WaitCount = 0UL;                           // 保存DMA等待次数并用于异常防死锁

    TIM_Cmd(WS2812_TIMER, DISABLE);                     // 关闭TIM1以便从确定状态开始发送
    DMA_Cmd(WS2812_DMA_CHANNEL, DISABLE);               // 关闭DMA以允许重新装载传输计数
    DMA_ClearFlag(WS2812_DMA_CLEAR_FLAGS);              // 清除上一次传输留下的全部DMA标志
    DMA_SetCurrDataCounter(WS2812_DMA_CHANNEL, WS2812_DATA_LENGTH); // 装载本帧DMA码元数量
    TIM_SetCounter(WS2812_TIMER, 0U);                   // 清零TIM1计数器统一首个码元相位
    TIM_SetCompare4(WS2812_TIMER, 0U);                  // 先将通道4保持为零占空比
    DMA_Cmd(WS2812_DMA_CHANNEL, ENABLE);                // 使能DMA向TIM1比较寄存器传输
    TIM_Cmd(WS2812_TIMER, ENABLE);                      // 启动TIM1输出八百千赫兹波形
    while (DMA_GetFlagStatus(WS2812_DMA_TRANSFER_FLAG) == RESET && WaitCount < WS2812_DMA_WAIT_LIMIT) // 等待整帧发送完成或超时
    {
        WaitCount++;                                    // 累计等待次数且不关闭系统中断
    }
    TIM_SetCompare4(WS2812_TIMER, 0U);                  // 将通道4比较值恢复为零
    TIM_GenerateEvent(WS2812_TIMER, TIM_EventSource_Update); // 立即装载零占空比确保输出低电平
    TIM_Cmd(WS2812_TIMER, DISABLE);                     // 关闭TIM1停止本帧输出
    DMA_Cmd(WS2812_DMA_CHANNEL, DISABLE);               // 关闭DMA等待下一帧
    DMA_ClearFlag(WS2812_DMA_CLEAR_FLAGS);              // 清除传输完成和错误状态标志
    GPIO_ResetBits(WS2812_GPIO_PORT, WS2812_GPIO_PIN);  // 清除PA11输出数据寄存器默认值
    Delay_us(WS2812_RESET_TIME_US);                     // 保持低电平二百八十微秒锁存整帧
}

/**
  * 函    数：推进流水灯头部的循环渐变颜色
  * 参    数：无
  * 返 回 值：无
  */
static void WS2812_UpdateWaterColor(void)
{
    if (WS2812Color.Phase < WS2812_COLOR_LEVELS)        // 判断是否处于绿色渐弱且红色渐强阶段
    {
        WS2812Color.Green = (uint8_t)(WS2812Color.Green - WS2812_COLOR_STEP); // 降低绿色一个步长
        WS2812Color.Red = (uint8_t)(WS2812Color.Red + WS2812_COLOR_STEP); // 提高红色一个步长
        WS2812Color.Phase++;                            // 推进当前渐变等级
        if (WS2812Color.Phase == WS2812_COLOR_LEVELS)   // 判断绿到红渐变是否完成
        {
            WS2812Color.Green = 0U;                     // 校正绿色端点为零
            WS2812Color.Red = 255U;                     // 校正红色端点为最大值
        }
    }
    else if (WS2812Color.Phase < WS2812_COLOR_LEVELS * 2U) // 判断是否处于红色渐弱且蓝色渐强阶段
    {
        WS2812Color.Red = (uint8_t)(WS2812Color.Red - WS2812_COLOR_STEP); // 降低红色一个步长
        WS2812Color.Blue = (uint8_t)(WS2812Color.Blue + WS2812_COLOR_STEP); // 提高蓝色一个步长
        WS2812Color.Phase++;                            // 推进当前渐变等级
        if (WS2812Color.Phase == WS2812_COLOR_LEVELS * 2U) // 判断红到蓝渐变是否完成
        {
            WS2812Color.Red = 0U;                       // 校正红色端点为零
            WS2812Color.Blue = 255U;                    // 校正蓝色端点为最大值
        }
    }
    else                                                // 处理蓝色渐弱且绿色渐强阶段
    {
        WS2812Color.Blue = (uint8_t)(WS2812Color.Blue - WS2812_COLOR_STEP); // 降低蓝色一个步长
        WS2812Color.Green = (uint8_t)(WS2812Color.Green + WS2812_COLOR_STEP); // 提高绿色一个步长
        WS2812Color.Phase++;                            // 推进当前渐变等级
        if (WS2812Color.Phase >= WS2812_COLOR_LEVELS * 3U) // 判断蓝到绿渐变是否完成
        {
            WS2812Color.Blue = 0U;                      // 校正蓝色端点为零
            WS2812Color.Green = 255U;                   // 校正绿色端点为最大值
            WS2812Color.Phase = 0U;                     // 返回第一阶段持续循环
        }
    }
}

/**
  * 函    数：生成下一帧彩色流水灯数据
  * 参    数：无
  * 返 回 值：无
  */
static void WS2812_UpdateWaterFrame(void)
{
    uint16_t Position;                                  // 保存当前向后搬移的灯珠位置
    uint8_t BitIndex;                                   // 保存当前搬移的码元下标

    for (Position = WS2812_LED_COUNT - 1U; Position > 0U; Position--) // 从灯带尾部向前搬移颜色
    {
        for (BitIndex = 0U; BitIndex < WS2812_BITS_PER_LED; BitIndex++) // 搬移当前灯珠二十四位数据
        {
            WS2812Data[Position * WS2812_BITS_PER_LED + BitIndex] = WS2812Data[(Position - 1U) * WS2812_BITS_PER_LED + BitIndex]; // 复制前一颗灯珠形成流水轨迹
        }
    }
    WS2812_UpdateWaterColor();                          // 推进头部的循环渐变颜色
    WS2812_FillColor(WS2812Color.Green, WS2812Color.Red, WS2812Color.Blue, 0U); // 写入第一颗灯珠的新颜色
}

/**
  * 函    数：WS2812灯带驱动初始化
  * 参    数：无
  * 返 回 值：无
  */
void WS2812_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                // 定义PA11初始化结构体
    TIM_TimeBaseInitTypeDef TimeBaseStructure;          // 定义TIM1时基初始化结构体
    TIM_OCInitTypeDef OutputCompareStructure;           // 定义TIM1通道4初始化结构体
    DMA_InitTypeDef DMA_InitStructure;                  // 定义DMA1通道4初始化结构体

    RCC_APB2PeriphClockCmd(WS2812_GPIO_CLOCK | RCC_APB2Periph_AFIO | WS2812_TIMER_CLOCK, ENABLE); // 开启GPIOA、AFIO和TIM1时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);  // 开启DMA1控制器时钟
    GPIO_InitStructure.GPIO_Pin = WS2812_GPIO_PIN;      // 选择TIM1通道4对应的PA11
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;     // 配置PA11为复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;   // 配置PA11为五十兆赫兹输出速度
    GPIO_Init(WS2812_GPIO_PORT, &GPIO_InitStructure);   // 写入GPIOA配置

    TIM_TimeBaseStructInit(&TimeBaseStructure);         // 填充TIM1时基默认值
    TimeBaseStructure.TIM_Prescaler = 0U;               // 使用七十二兆赫兹时钟直接计数
    TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 配置TIM1向上计数
    TimeBaseStructure.TIM_Period = (uint16_t)(SystemCoreClock / WS2812_SIGNAL_FREQUENCY_HZ - 1UL); // 生成八百千赫兹周期
    TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1; // 配置数字滤波时钟不分频
    TimeBaseStructure.TIM_RepetitionCounter = 0U;       // 配置每个PWM周期都更新
    TIM_TimeBaseInit(WS2812_TIMER, &TimeBaseStructure); // 写入TIM1时基参数

    TIM_OCStructInit(&OutputCompareStructure);          // 填充输出比较默认值
    OutputCompareStructure.TIM_OCMode = TIM_OCMode_PWM1; // 配置PWM模式一
    OutputCompareStructure.TIM_OutputState = TIM_OutputState_Enable; // 使能通道4主输出
    OutputCompareStructure.TIM_Pulse = 0U;              // 初始化为零占空比
    OutputCompareStructure.TIM_OCPolarity = TIM_OCPolarity_High; // 配置高电平有效
    TIM_OC4Init(WS2812_TIMER, &OutputCompareStructure); // 写入TIM1通道4配置
    TIM_OC4PreloadConfig(WS2812_TIMER, TIM_OCPreload_Enable); // 使能比较值预装载避免毛刺
    TIM_ARRPreloadConfig(WS2812_TIMER, ENABLE);         // 使能自动重装值预装载
    TIM_CtrlPWMOutputs(WS2812_TIMER, ENABLE);           // 开启高级定时器主输出
    TIM_DMACmd(WS2812_TIMER, WS2812_TIMER_DMA_SOURCE, ENABLE); // 使能通道4比较事件DMA请求

    DMA_StructInit(&DMA_InitStructure);                 // 填充DMA结构体默认值
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&WS2812_TIMER->CCR4; // 设置目标为TIM1通道4比较寄存器
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)WS2812Data; // 设置源地址为灯带缓冲区
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;  // 配置内存到外设传输方向
    DMA_InitStructure.DMA_BufferSize = WS2812_DATA_LENGTH; // 配置整帧传输长度
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable; // 禁止外设地址递增
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; // 允许内存地址递增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 配置外设数据宽度为十六位
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord; // 配置内存数据宽度为十六位
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;       // 配置普通单次传输模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_High; // 配置DMA高优先级
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;        // 禁止内存到内存模式
    DMA_Init(WS2812_DMA_CHANNEL, &DMA_InitStructure);   // 写入DMA1通道4配置

    TIM_Cmd(WS2812_TIMER, DISABLE);                     // 初始化后暂时关闭TIM1
    DMA_Cmd(WS2812_DMA_CHANNEL, DISABLE);               // 初始化后暂时关闭DMA
    WS2812_ClearPixels();                               // 准备全灭帧和复位码元
    WS2812Mode = WS2812_MODE_WATER;                     // 默认选择彩色流水灯
    WS2812Color.Green = 255U;                           // 设置初始绿色为最大值
    WS2812Color.Red = 0U;                               // 设置初始红色为零
    WS2812Color.Blue = 0U;                              // 设置初始蓝色为零
    WS2812Color.Phase = 0U;                             // 设置渐变阶段为起点
    WS2812FlashState = 0U;                              // 清除闪烁亮灭状态
    WS2812LastFrameMs = 0UL;                            // 清除动画时间基准
    WS2812_Show();                                      // 发送全灭帧清除上电随机颜色
}

/**
  * 函    数：切换WS2812灯带工作模式
  * 参    数：Mode关闭、流水或闪烁模式
  * 返 回 值：无
  */
void WS2812_SetMode(WS2812ModeTypeDef Mode)
{
    if (Mode > WS2812_MODE_FLASH)                       // 判断模式值是否超出有效范围
    {
        Mode = WS2812_MODE_OFF;                         // 将无效值安全转换为关闭模式
    }
    if (WS2812Mode == Mode)                             // 判断目标模式是否与当前相同
    {
        return;                                         // 保持当前动画进度并避免重复刷新
    }
    WS2812Mode = Mode;                                  // 保存新灯效模式
    WS2812FlashState = 0U;                              // 清除上一轮闪烁状态
    WS2812LastFrameMs = 0UL;                            // 清除动画时间基准
    WS2812_ClearPixels();                               // 清除上一模式留下的颜色数据
    if (Mode == WS2812_MODE_WATER)                      // 判断是否切换到流水模式
    {
        WS2812Color.Green = 255U;                       // 恢复初始绿色最大亮度
        WS2812Color.Red = 0U;                           // 恢复初始红色为零
        WS2812Color.Blue = 0U;                          // 恢复初始蓝色为零
        WS2812Color.Phase = 0U;                         // 恢复渐变起点
    }
    WS2812_Show();                                      // 立即显示全灭过渡帧或关闭灯带
}

/**
  * 函    数：读取WS2812灯带当前工作模式
  * 参    数：无
  * 返 回 值：当前灯效模式
  */
WS2812ModeTypeDef WS2812_GetMode(void)
{
    return WS2812Mode;                                  // 返回当前保存的灯效模式
}

/**
  * 函    数：周期更新WS2812流水或闪烁动画
  * 参    数：NowMs当前系统毫秒时间
  * 返 回 值：无
  */
void WS2812_Task(uint32_t NowMs)
{
    if (WS2812Mode == WS2812_MODE_OFF)                  // 判断灯带是否处于关闭模式
    {
        return;                                         // 关闭模式不重复发送全灭帧
    }
    if ((uint32_t)(NowMs - WS2812LastFrameMs) < WS2812_ANIMATION_PERIOD_MS) // 判断是否达到刷新周期
    {
        return;                                         // 周期未到时返回保持主循环调度
    }
    WS2812LastFrameMs = NowMs;                          // 记录本次动画更新时间
    if (WS2812Mode == WS2812_MODE_WATER)                // 判断是否运行彩色流水灯
    {
        WS2812_UpdateWaterFrame();                      // 生成下一帧流水颜色轨迹
    }
    else                                                // 处理全灯绿色闪烁模式
    {
        WS2812FlashState ^= 1U;                         // 翻转本次亮灭状态
        if (WS2812FlashState != 0U)                     // 判断本帧是否需要点亮
        {
            WS2812_FillAll(255U, 0U, 0U);              // 按GRB格式将全部灯珠设为绿色
        }
        else                                            // 处理本帧需要熄灭的状态
        {
            WS2812_ClearPixels();                       // 清除全部灯珠颜色
        }
    }
    WS2812_Show();                                      // 输出本次完整动画帧
}
