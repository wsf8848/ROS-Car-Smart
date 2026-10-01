#include "stm32f10x.h"                  // Device header
#include "My_ADC.h"                 

void AD_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC|RCC_APB2Periph_ADC1,ENABLE);
	
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);
	
	GPIO_InitTypeDef GPIO_InitStrusture;
	GPIO_InitStrusture.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStrusture.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStrusture.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_Init(GPIOC,&GPIO_InitStrusture);
	
	ADC_InitTypeDef ADC_InitStrusture;
	ADC_DeInit(ADC1);
	ADC_InitStrusture.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStrusture.ADC_ScanConvMode = DISABLE;
	ADC_InitStrusture.ADC_ExternalTrigConv  = ADC_ExternalTrigConv_None;
	ADC_InitStrusture.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStrusture.ADC_NbrOfChannel = 1;
	ADC_Init(ADC1,&ADC_InitStrusture);
	
	ADC_Cmd(ADC1,ENABLE);
	ADC_ResetCalibration(ADC1);
	
	while(ADC_GetResetCalibrationStatus(ADC1));
	
	ADC_StartCalibration(ADC1);
	
	while(ADC_GetCalibrationStatus(ADC1));
}

uint16_t Get_Adc(uint16_t ch)
{
	ADC_RegularChannelConfig(ADC1,ch,1,ADC_SampleTime_239Cycles5);
	ADC_SoftwareStartConvCmd(ADC1,ENABLE);
	while(!ADC_GetFlagStatus(ADC1,ADC_FLAG_EOC));
	
	return ADC_GetConversionValue(ADC1);
}
/**
 * @brief 将ADC采样值转换为锂电池电压值（带简单滤波）
 *
 * 该函数接收原始ADC采样值，通过简单的移动平均滤波算法处理，
 * 并根据电路参数计算出实际的锂电池电压值。
 *
 * @param adc_value 原始ADC采样值
 * @return float 转换后的锂电池电压值（单位：V）
 *
 * @note 电路参数（分压比、参考电压、ADC分辨率）需要根据实际情况调整
 */
float convert_battery_voltage(uint16_t adc_value)
{
  // 电路参数配置（根据实际电路调整）
  const float ADC_REF_VOLTAGE = 3.3f;       // ADC参考电压（单位：V）
  const float VOLTAGE_DIVIDER_RATIO = 11.10f; // 分压比（例如：3.0表示实际电压是采样电压的3倍） 11.10
  const uint16_t ADC_RESOLUTION = 4096;     // ADC分辨率（例如：12位ADC为4096）

  // 滤波系数（0.0-1.0，越大滤波效果越弱，响应越快）
  const float FILTER_FACTOR = 0.5f;

  // 静态变量存储上一次的电压值（用于滤波）
  static float last_voltage = 0.0f;

  // 计算本次原始电压值
  float current_voltage = (float)adc_value * ADC_REF_VOLTAGE / ADC_RESOLUTION * VOLTAGE_DIVIDER_RATIO;

  // 简单滤波：使用一阶低通滤波器
  float filtered_voltage = last_voltage + FILTER_FACTOR * (current_voltage - last_voltage);

  // 更新上一次的电压值
  last_voltage = filtered_voltage;

  return filtered_voltage;
}
