#include "AllHeader.h"

extern u8 data_buff[50];
extern uint8_t cmd_flag; //发送命令的标志 0:没发送  1:以发送  2:进入网络透传
extern u8 newlines;


int main()
{
	
	SystemInit();
	
	delay_init();
	USART1_init(115200);//接PC的串口PA9(TX)  PA10(RX)
	USART2_init(115200);//接摄像头模块的 PD5(TX)  PD6(RX)
	
	FUN_delay_s(2);//等待摄像头正常开启
	
	printf("waiting for wifi start!\r\n");
	
	SET_STA_WIFI();//配置STA 
	SET_AP_WIFI();//配置AP
	SET_ESP_WIFI_MODE();//模式配置
	
	Get_Version();
	IP_identify(data_buff);
	delay_ms(100);
	
	//电机部分初始化
	motor_gpio_init();
	motor_pwm_init(MOTOR_MAX_PULSE,MOTOR_FREQ_DIVIDE);
	motor_encode_init();
	PID_Param_Init();
	
	//舵机初始化
	pwmServo_init_gpio();
	
	
#if !MODE_STA
	for (u8 num =0;num<3; num++ )//查3次ap的IP
	{
		FUN_delay_s(1);
		Get_APIP();
		IP_identify(data_buff);
	}
#endif	

#if !MODE_AP
	for(u8 num =0;num<3; num++ )//查3次sta的IP
	{
		FUN_delay_s(2);
		Get_STAIP();
		IP_identify(data_buff);
	}
#endif
	
	//TIM6_Init();//开始测速定时器
	TIM7_Init();//开始舵机控制定时器  测速定时器
	
	cmd_flag = 2; 
	
	PwmServo_Set_Angle_All(90,90,90,90);
	
	while(1)
	{
		if(newlines == 1)
		{
			newlines = 0;
			Change_motor();
			memset(data_buff,0,sizeof(data_buff));//清空数据
		}
		
	}
	
}

