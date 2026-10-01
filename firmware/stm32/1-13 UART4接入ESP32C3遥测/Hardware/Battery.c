#include "Battery.h"                              // 提供电池电压采样和百分比接口

static uint16_t BatteryVoltageMv = BATTERY_EMPTY_MV; // 保存滤波后的电池电压，单位毫伏
static uint8_t BatteryPercent = 0U;                 // 保存换算后的电量百分比

/**
  * 函数：初始化电池电压ADC采样
  * 参数：无
  * 返回值：无
  */
void Battery_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;             // 保存PC4模拟输入配置
    ADC_InitTypeDef ADC_InitStructure;               // 保存ADC1单通道配置

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_ADC1, ENABLE); // 开启PC4和ADC1时钟
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);                // 将ADC时钟设置为规范允许的分频值
    GPIO_InitStructure.GPIO_Pin = BATTERY_ADC_GPIO_PIN; // 选择PC4电池分压输入引脚
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;    // 将PC4配置为模拟输入，关闭数字输入路径
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; // 设置GPIO配置速度
    GPIO_Init(BATTERY_ADC_GPIO_PORT, &GPIO_InitStructure); // 应用PC4模拟输入配置

    ADC_DeInit(ADC1);                                 // 复位ADC1到默认状态
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent; // 设置ADC1独立工作模式
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;     // 设置为单通道采样
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE; // 使用软件触发单次转换
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None; // 禁止外部触发
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right; // 设置转换结果右对齐
    ADC_InitStructure.ADC_NbrOfChannel = 1U;           // 设置规则组仅包含一个通道
    ADC_Init(ADC1, &ADC_InitStructure);                // 应用ADC1配置
    ADC_Cmd(ADC1, ENABLE);                             // 开启ADC1
    ADC_ResetCalibration(ADC1);                        // 启动ADC复位校准
    while (ADC_GetResetCalibrationStatus(ADC1) != RESET) {} // 等待复位校准完成
    ADC_StartCalibration(ADC1);                        // 启动ADC校准
    while (ADC_GetCalibrationStatus(ADC1) != RESET) {} // 等待ADC校准完成
    BatteryVoltageMv = BATTERY_EMPTY_MV;              // 初始化电压显示为电量下限
    BatteryPercent = 0U;                               // 初始化电量百分比为0
}

/**
  * 函数：采样并更新电池电压
  * 参数：无
  * 返回值：无
  */
void Battery_Update(void)
{
    uint16_t AdcValue;                                // 保存本次ADC原始采样值
    uint32_t SampleVoltageMv;                         // 保存本次未滤波的电池电压
    uint32_t FilteredVoltageMv;                       // 保存本次滤波后的电池电压
    int32_t VoltageDifferenceMv;                      // 保存本次采样与滤波电压的有符号差值

    ADC_RegularChannelConfig(ADC1, BATTERY_ADC_CHANNEL, 1U, ADC_SampleTime_239Cycles5); // 配置PC4为规则组首通道
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);           // 软件触发一次ADC转换
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {} // 等待转换完成
    AdcValue = ADC_GetConversionValue(ADC1);          // 读取本次12位转换结果
    SampleVoltageMv = ((uint32_t)AdcValue * BATTERY_ADC_REF_MV * BATTERY_DIVIDER_RATIO_NUM) / BATTERY_ADC_RESOLUTION / BATTERY_DIVIDER_RATIO_DEN; // 按11.1倍分压比换算真实电池电压
    VoltageDifferenceMv = (int32_t)SampleVoltageMv - (int32_t)BatteryVoltageMv; // 计算本次采样相对滤波值的变化方向
    FilteredVoltageMv = (uint32_t)((int32_t)BatteryVoltageMv + (VoltageDifferenceMv >> BATTERY_FILTER_SHIFT)); // 使用一阶滤波抑制ADC抖动
    if (FilteredVoltageMv < BATTERY_EMPTY_MV)       // 限制滤波结果不低于电量映射下限
    {
        FilteredVoltageMv = BATTERY_EMPTY_MV;         // 将低于下限的结果钳位到10500毫伏
    }
    BatteryVoltageMv = (uint16_t)(FilteredVoltageMv > 65535U ? 65535U : FilteredVoltageMv); // 保存不超过16位范围的电压结果
    if (BatteryVoltageMv >= BATTERY_FULL_MV)         // 判断电池是否达到满电电压
    {
        BatteryPercent = 100U;                        // 满电时固定显示100%
    }
    else if (BatteryVoltageMv <= BATTERY_EMPTY_MV)   // 判断电池是否达到空电电压
    {
        BatteryPercent = 0U;                          // 低于下限时固定显示0%
    }
    else
    {
        BatteryPercent = (uint8_t)(((uint32_t)(BatteryVoltageMv - BATTERY_EMPTY_MV) * 100U) / (BATTERY_FULL_MV - BATTERY_EMPTY_MV)); // 在线性区间内换算百分比
    }
}

/**
  * 函数：获取滤波后的电池电压
  * 参数：无
  * 返回值：电池电压，单位毫伏
  */
uint16_t Battery_GetVoltageMv(void)
{
    return BatteryVoltageMv;                          // 返回最近一次滤波后的电池电压
}

/**
  * 函数：获取协议使用的电池电压
  * 参数：无
  * 返回值：电池电压，单位0.1伏
  */
uint8_t Battery_GetVoltageDeciV(void)
{
    return (uint8_t)((BatteryVoltageMv + 50U) / 100U); // 将毫伏四舍五入为0.1伏并返回
}

/**
  * 函数：获取电池百分比
  * 参数：无
  * 返回值：0至100的电量百分比
  */
uint8_t Battery_GetPercent(void)
{
    return BatteryPercent;                             // 返回最近一次换算后的电量百分比
}
