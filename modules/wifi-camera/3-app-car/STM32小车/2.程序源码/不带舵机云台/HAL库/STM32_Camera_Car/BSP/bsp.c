#include "bsp.h"


extern uint8_t data_buff[50];
extern uint8_t cmd_flag; //发送命令的标志 0:没发送  1:以发送  2:进入网络透传
extern uint8_t newlines;


//秒的延迟
void FUN_delay_s(float s)
{
	int SZ = (int)s;      //整数部分
	int SX = (s-SZ)*1000; //小数部分

	while(SZ >0)
	{
		HAL_Delay(1000);
		SZ--;
	}

	if(SX != 0)
	{
		HAL_Delay(SX);//延迟
	}



}

void BSP_Init(void)
{
	USART2_UART_Init();

	printf("waiting for wifi start!\r\n");

	SET_STA_WIFI();//配置STA
	SET_AP_WIFI();//配置AP
	SET_ESP_WIFI_MODE();//模式配置

	Get_Version();
	IP_identify(data_buff);
	HAL_Delay(100);

	Bsp_Tim_Init();
	PID_Param_Init();//电机PID初始化 Motor PID initialization


	#if !MODE_STA
		for (uint8_t num =0;num<3; num++ )//查3次ap的IP
		{
			FUN_delay_s(1);
			Get_APIP();
			IP_identify(data_buff);
		}
	#endif

	#if !MODE_AP
		for(uint8_t num =0;num<3; num++ )//查3次sta的IP
		{
			FUN_delay_s(2);
			Get_STAIP();
			IP_identify(data_buff);
		}
	#endif

	cmd_flag = 2;
}


void BSP_Loop(void)
{
	if(newlines == 1)
	{
		newlines = 0;
		Change_motor();
		memset(data_buff,0,sizeof(data_buff));//清空数据
	}
}
