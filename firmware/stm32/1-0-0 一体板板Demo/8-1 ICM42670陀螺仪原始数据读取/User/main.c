#include "stm32f10x.h"     // Device header
#include "LED.h" 
#include "Delay.h" 
#include "Key.h" 
#include "Buzzer.h"  
#include "OLED.h"
#include "ICM_42670.h"

// ICM42688原始数据
extern GYRO Gyro_Get;      // 角速度
extern ACC Acc_Get;        // 加速度
extern TEMP Temp;          // 温度

int main(void)
{
	/*模块初始化*/
	LED_Init();
	Key_Init();
	Buzzer_Init();
	OLED_Init();		//OLED初始化
	//ICM42688初始化
    ICM_42670_Init();		//ICM_42670初始化
    ICM_INIT();           // ICM芯片初始化
	
  /*显示ID号*/
  OLED_ShowString(1, 1, "ID:");		                //显示静态字符串
  OLED_ShowHexNum(1, 4, ICM_42670_GetID(), 2);		//OLED显示ID号	
	
	/*OLED显示*/
	while (1)
	{
		GYRO_ACC_TEMP_GET();
		OLED_ShowFloat(2, 1, Acc_Get.X, 5, 2);					//OLED显示数据
		OLED_ShowFloat(3, 1, Acc_Get.Y, 5, 2);
		OLED_ShowFloat(4, 1, Acc_Get.Z, 5, 2);
		  
		OLED_ShowFloat(2, 9, Gyro_Get.X, 5, 2);
		OLED_ShowFloat(3, 9, Gyro_Get.Y, 5, 2);
		OLED_ShowFloat(4, 9, Gyro_Get.Z, 5, 2);
	}
}
