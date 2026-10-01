#include "stm32f10x.h"     // Device header
#include "LED.h"
#include "Delay.h"
#include "Key.h"
#include "Buzzer.h"
#include "OLED.h"
#include "My_ADC.h"

uint16_t ADValue = 0;//定义AD值变量
float Voltage = 0; //电量
uint8_t Voltage_Show = 0; //电量百分比

int main(void)
{
  /*模块初始化*/
  LED_Init();
  Key_Init();
  Buzzer_Init();
  OLED_Init();		//OLED初始化
  AD_Init();

  /*OLED显示*/

  OLED_ShowString(1, 1, "ADC:");	//1行3列显示字符串HelloWorld!

  while (1)
  {
    ADValue = Get_Adc(14);//获取AD值
    Voltage = convert_battery_voltage(ADValue); //电量转化为电压形式
    //对电池电压处理成百分比形式
    Voltage_Show = (Voltage >= 12.6f) ? 100 : ((Voltage <= 10.5f) ? 0 : (uint8_t)((Voltage - 10.5f) / 2.1f * 100));

  }
}
