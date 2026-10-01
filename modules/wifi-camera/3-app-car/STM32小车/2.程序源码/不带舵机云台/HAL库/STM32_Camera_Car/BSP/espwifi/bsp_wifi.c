#include "bsp_wifi.h"


#if MODE_AP_STA
#define WIFI_MODE '2'

#elif MODE_STA
#define WIFI_MODE '1'

#elif MODE_AP
#define WIFI_MODE '0'

#else
#define WIFI_MODE '2'
#endif

#define delay_ms HAL_Delay

#define STAIP "sta_ip"
#define STA_WIFI_SSID "Yahboom2"  		//wifi名称
#define STA_WIFI_PD   "yahboom8" //wifi密码

#define APIP "ap_ip"
#define AP_WIFI_SSID "ESP32_WIFI_TEST"  		//wifi名称
#define AP_WIFI_PD   "" //wifi密码 -无密码 也可在双引号添加

uint8_t send_buf[35]={0}; //发送命令的buf
uint8_t recv_buf[50]={0}; //接收buf
uint8_t data_buff[50]={0}; //备份buf

uint8_t cmd_flag = 0;//发送命令的标志 0:没发送  1:以发送  2:进入网络透传

//设置sta模式的wifi
void SET_STA_WIFI(void)
{
	//发送ssid
	sprintf((char*)send_buf,"sta_ssid:%s",STA_WIFI_SSID);
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	delay_ms(300);

	//发送pd
	sprintf((char*)send_buf,"sta_pd:%s",STA_WIFI_PD);
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));

	FUN_delay_s(2);//等待复位重启成功

}

//设置ap模式的wifi
void SET_AP_WIFI(void)
{
	//发送ssid
	sprintf((char*)send_buf,"ap_ssid:%s",AP_WIFI_SSID);
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	delay_ms(300);

	//发送pd
	sprintf((char*)send_buf,"ap_pd:%s",AP_WIFI_PD);
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));

	FUN_delay_s(2);//等待复位重启成功

}

void SET_ESP_WIFI_MODE(void) //设置模式选择，需要把esp32-wifi断电复位
{
	//选择模式STA+AP模式共存
	sprintf((char*)send_buf,"wifi_mode:%c",WIFI_MODE);
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));

	FUN_delay_s(2);//等待复位重启成功
}


//查询sta模式的ip
void Get_STAIP(void)
{
	sprintf((char*)send_buf,STAIP);
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	cmd_flag = 1;
}

//查询ap模式的ip
void Get_APIP(void)
{
	sprintf((char*)send_buf,APIP);
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	cmd_flag = 1;
}

void Get_Version(void)
{
	//查询固件版本号
	sprintf((char*)send_buf,"wifi_ver");
	USART2_DataString(send_buf,strlen((char*)send_buf));
	memset(send_buf,0,sizeof(send_buf));
	cmd_flag = 1;
}


uint8_t end_falg = 0;
uint8_t i_index= 0; //数组索引

//处理串口接收到的信息
void Data_Deal(uint8_t RXdata)
{
	//查询wifi的ip地址情况
	if(cmd_flag == 1)
	{
		recv_buf[i_index] = RXdata;

		//当接收到换行符,一包的数据接收完成
		if(RXdata == 0x0D)
		{
			end_falg = 1;
		}

		if(end_falg == 1 && RXdata == 0x0A)
		{
				cmd_flag = 0;
				end_falg = 0;
				memcpy(data_buff,recv_buf,i_index);
				memset(recv_buf,0,sizeof(recv_buf));//接收数据缓存清0
				i_index = 0;//索引清0
		}
		else
			i_index ++;

	}

	//网络透传的数据
	else if(cmd_flag == 2 )
	{
		recv_tcp_data(RXdata);
	}
}




//判断数据是STA的ip还是AP的ip
uint8_t IP_identify(uint8_t* data)
{
	if(memcmp(data,"sta_ip",6)==0)
	{
		printf("%s\n",data); //这个库会把\n去掉，所以要加上
//		memset(data_buff,0,sizeof(data_buff));
//		return 0;
	}
	else if(memcmp(data,"ap_ip",5)==0)
	{
		printf("%s\n",data); //这个库会把\n去掉，所以要加上
//		memset(data_buff,0,sizeof(data_buff));
//		return 0;
	}
	else if(memcmp(data,"YAHBOOM VerSion",15)==0)
	{
		printf("%s\n",data); ////这个库会把\n去掉，所以要加上
//		memset(data_buff,0,sizeof(data_buff));
//		return 0;
	}

	memset(data_buff,0,sizeof(data_buff));
	return 1;
}



uint8_t g_new_flag = 0;
uint8_t g_index = 0;
uint8_t newlines = 0; //1:接收新的数据 0:没合适数据

//接收数据透传的数据
void recv_tcp_data(uint8_t tcpdata)
{
//	printf("%c",tcpdata); //可以自己定制控制协议做解析
	if (tcpdata == '$' && g_new_flag == 0)
	{
		g_new_flag = 1;
		memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据
		return;
	}
	if(g_new_flag == 1)
	{
		if (tcpdata == '#')
		{
			g_new_flag = 0;
			g_index = 0;
			newlines = 1; //新数据接收完毕
			memcpy(data_buff,recv_buf,sizeof(recv_buf));

			memset(recv_buf, 0, sizeof(recv_buf)); // 清除旧数据
		}

		else if (tcpdata == '$')//中途出现丢包
		{
			g_index = 0;
			g_new_flag = 0;
			memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据
		}

		else
		{
			recv_buf[g_index++] = tcpdata;
		}

		if(g_index > 50) //数组溢出
		{
			g_index = 0;
			g_new_flag = 0;
			memset(recv_buf, 0, sizeof(recv_buf)); //清除旧数据
		}

	}
}


//放在主函数处理
void Change_motor(void)
{
	if (data_buff[0] == '1') //前进
	{
		wheel_State(MOTION_RUN,350);
	}
	else if (data_buff[0] == '2')//后退
	{
		wheel_State(MOTION_BACK,350);
	}
	else if (data_buff[0] == '3')//左转
	{
		wheel_State(MOTION_LEFT,450);
	}
	else if (data_buff[0] == '4')//右转
	{
		wheel_State(MOTION_RIGHT,450);
	}
	else if (data_buff[0] == '5')//左旋
	{
		wheel_State(MOTION_SPIN_LEFT,500);
	}
	else if (data_buff[0] == '6')//右旋
	{
		wheel_State(MOTION_SPIN_RIGHT,500);
	}
	else if (data_buff[0] == '7')//停止
	{
		wheel_State(MOTION_STOP,0);
	}

}

