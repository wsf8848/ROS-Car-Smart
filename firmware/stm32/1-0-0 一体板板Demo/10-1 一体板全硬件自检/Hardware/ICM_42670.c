#include "stm32f10x.h"                  // Device header
#include "MyI2C.h"
#include "ICM_42670_Reg.h"
#include "Delay.h"
#include "ICM_42670.h"

#define ICM_42670_ADDRESS		0x75				//ICM_42670的I2C从机地址

GYRO Gyro_Get;
ACC Acc_Get;
TEMP Temp;

/**
  * 函    数：ICM_42670写寄存器
  * 参    数：RegAddress 寄存器地址，范围：参考ICM_42670手册的寄存器描述
  * 参    数：Data 要写入寄存器的数据，范围：0x00~0xFF
  * 返 回 值：无
  */
unsigned char ICM_42670_WriteReg(unsigned char RegAddress, unsigned char Data)
{
	MyI2C_Start();						//I2C起始
	MyI2C_SendByte(0xD0);				//发送从机地址，读写位为0，表示即将写入 
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_SendByte(RegAddress);			//发送寄存器地址
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_SendByte(Data);				//发送要写入寄存器的数据
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_Stop();						//I2C终止
	return 0U;							//保持旧接口并返回写寄存器成功状态
}

/**
  * 函    数：ICM_42670读寄存器
  * 参    数：RegAddress 寄存器地址，范围：参考ICM_42670手册的寄存器描述
  * 返  回 值：读取的数据
  */
uint8_t ICM_42670_ReadReg(uint8_t RegAddress)
{
	uint8_t Data;
	
	MyI2C_Start();						//I2C起始
	MyI2C_SendByte(0xD0);				//发送从机地址，读写位为0，表示即将写入
	MyI2C_ReceiveAck();					//接收应答
	MyI2C_SendByte(RegAddress);			//发送寄存器地址
	MyI2C_ReceiveAck();					//接收应答
	
	MyI2C_Start();						//I2C重复起始
	MyI2C_SendByte(0xD1);				//发送从机地址，读写位为1，表示即将读取
	MyI2C_ReceiveAck();					//接收应答
	Data = MyI2C_ReceiveByte();			//接收指定寄存器的数据
	MyI2C_SendAck(1);					//发送应答，给从机非应答，终止从机的数据输出
	MyI2C_Stop();						//I2C终止
	
	return Data;
}

/******************************************************************/
/*函数名：ICM_ACC_INIT;***************************************/
/*功能：ICM加速度计初始化;*************/
unsigned char ICM_ACC_INIT()
{
	if(ICM_42670_WriteReg(ACCEL_CONFIG0,0x06)) return 1;//调整采样率和ODR
	Delay_ms(50);
	if(ICM_42670_WriteReg(ACCEL_CONFIG1,0x0D)) return 1;//调整带宽和滤波次数
	Delay_ms(50);
	return 0;
	
}
/******************************************************************/
/*函数名：ICM_Gyroscope_INIT;***************************************/
/*功能：ICM陀螺仪初始化;*************/
/*输入：无;********************************************************/
/*输出：0 成功 1 失败;***************************************/
/******************************************************************/
unsigned char ICM_Gyroscope_INIT(void)
{
	if(ICM_42670_WriteReg(GYRO_CONFIG0,0x06)) return 1;//调整采样率和ODR
	Delay_ms(50);
	if(ICM_42670_WriteReg(GYRO_CONFIG1,0x16)) return 1;//调整带宽和滤波次数
	Delay_ms(50);
	if(ICM_42670_WriteReg(GYRO_ACCEL_CONFIG0,0x11)) return 1;//调整陀螺仪和加速度计的低通滤波器带宽    
	Delay_ms(50);
	return 0;
}

/******************************************************************/
/*函数名：ICM_INIT;***************************************/
/*功能：ICM芯片初始化;*************/
/*输入：无;********************************************************/
/*输出：0 成功 1 失败;***************************************/
/******************************************************************/
unsigned char ICM_INIT(void)
{
	if(ICM_42670_ReadReg(WHO_AM_I) != 0x47) return 1;	//ID校验
	
	ICM_42670_WriteReg(DEVICE_CONFIG,0x00);	//软件复位配置
	Delay_ms(50);
	ICM_42670_WriteReg(DRIVE_CONFIG,0x05);	//通信速度驱动配置
	Delay_ms(50);
	ICM_42670_WriteReg(INT_CONFIG,0x02);	//中断设置
	Delay_ms(50);
	ICM_42670_WriteReg(PWR_MGMT0,0x0F);		//传感器电源使能
	Delay_ms(50);
	ICM_42670_WriteReg(INT_CONFIG1,0x00);	//中断脉冲配置
	Delay_ms(50);
	ICM_42670_WriteReg(INT_SOURCE0,0x08);	//中断源设置
	Delay_ms(50);
	if(ICM_Gyroscope_INIT()) return 1;//陀螺仪初始化
	Delay_ms(50);
	if(ICM_ACC_INIT()) return 1;//加速度计初始化
	Delay_ms(50);
	
	return 0;
}

/**
  * 函    数：ICM_42670初始化
  * 参    数：无
  * 返 回 值：无
  */
void ICM_42670_Init(void)
{
	MyI2C_Init();									//先初始化底层的I2C
	
	/*ICM_42670寄存器初始化，需要对照ICM_42670手册的寄存器描述配置，此处仅配置了部分重要的寄存器*/
	ICM_42670_WriteReg(DEVICE_CONFIG, 0x01);		// 软件复位
	Delay_ms(50);									// 延时
	
	ICM_42670_WriteReg(DEVICE_CONFIG, 0x00);		//电源管理寄存器1，取消睡眠模式，选择时钟源为X轴陀螺仪
	Delay_ms(50);									// 延时
	
	ICM_42670_WriteReg(PWR_MGMT0, 0x0F);			//电源管理寄存器2，保持默认值0，所有轴均不待机
	Delay_ms(50);									// 延时
	
	ICM_42670_WriteReg(GYRO_CONFIG0, 0x06);		    //采样率分频寄存器，配置采样率
	ICM_42670_WriteReg(ACCEL_CONFIG0, 0x06);		//配置寄存器，配置DLPF
	ICM_42670_WriteReg(GYRO_CONFIG1, 0x16);	        //陀螺仪配置寄存器，选择满量程为±2000°/s
	ICM_42670_WriteReg(ACCEL_CONFIG1, 0x0D);		//加速度计配置寄存器，选择满量程为±16g
	ICM_42670_WriteReg(GYRO_ACCEL_CONFIG0, 0x11);	//加速度计配置寄存器，选择满量程为±16g
	Delay_ms(50);
}

/**
  * 函    数：ICM_42670获取ID号
  * 参    数：无
  * 返 回 值：ID
  */
uint8_t ICM_42670_GetID(void)
{
	return ICM_42670_ReadReg(WHO_AM_I);		//返回WHO_AM_I寄存器的值
}

/******************************************************************/
/*函数名：GYRO_ACC_TEMP_GET;***************************************/
/*功能：获取陀螺仪，加速度计，温度数据;*************/
/******************************************************************/
unsigned char GYRO_ACC_TEMP_GET(void)
{
	unsigned char temp = 0;
	short Counting_Temp = 0;
	//温度读取
	temp = ICM_42670_ReadReg(TEMP_DATA1);
	Counting_Temp = temp;
	Counting_Temp = Counting_Temp << 8;
	temp = 0;
	temp = ICM_42670_ReadReg(TEMP_DATA0);
	Counting_Temp = Counting_Temp|temp ;temp = 0;
	Temp.T = (Counting_Temp / 132.48) + 25;
	
	//X轴陀螺仪读取
	temp = ICM_42670_ReadReg(GYRO_DATA_X1);
	Counting_Temp = temp;
	Counting_Temp = Counting_Temp << 8;
	temp = 0;
	temp = ICM_42670_ReadReg(GYRO_DATA_X0);
	Counting_Temp = Counting_Temp|temp ;temp = 0;
	Gyro_Get.X = (Counting_Temp*1.0)/32767.0*2000;
	
	//Y轴陀螺仪读取
	temp = ICM_42670_ReadReg(GYRO_DATA_Y1);
	Counting_Temp = temp;
	Counting_Temp = Counting_Temp << 8;
	temp = 0;
	temp = ICM_42670_ReadReg(GYRO_DATA_Y0);
	Counting_Temp = Counting_Temp|temp ;temp = 0;
	Gyro_Get.Y = (Counting_Temp*1.0)/32767.0*2000;
	
	//Z轴陀螺仪读取
	temp = ICM_42670_ReadReg(GYRO_DATA_Z1);
	Counting_Temp = temp;
	Counting_Temp = Counting_Temp << 8;
	temp = 0;
	temp = ICM_42670_ReadReg(GYRO_DATA_Z0);
	Counting_Temp = Counting_Temp|temp ;temp = 0;
	Gyro_Get.Z = (Counting_Temp*1.0)/32767.0*2000;
	
	//X轴加速度计读取 ±16g
	temp = ICM_42670_ReadReg(ACCEL_DATA_X1);
	Counting_Temp = temp;
	Counting_Temp = Counting_Temp << 8;
	temp = 0;
	temp = ICM_42670_ReadReg(ACCEL_DATA_X0);
	Counting_Temp = Counting_Temp|temp ;temp = 0;
	Acc_Get.X = (Counting_Temp*1.0)/32767.0*16.0*9.8;
	
	//Y轴加速度计读取 ±16g
	temp = ICM_42670_ReadReg(ACCEL_DATA_Y1);
	Counting_Temp = temp;
	Counting_Temp = Counting_Temp << 8;
	temp = 0;
	temp = ICM_42670_ReadReg(ACCEL_DATA_Y0);
	Counting_Temp = Counting_Temp|temp ;temp = 0;
	Acc_Get.Y = (Counting_Temp*1.0)/32767.0*16.0*9.8;
	
	//Z轴加速度计读取 ±16g
	temp = ICM_42670_ReadReg(ACCEL_DATA_Z1);
	Counting_Temp = temp;
	Counting_Temp = Counting_Temp << 8;
	temp = 0;
	temp = ICM_42670_ReadReg(ACCEL_DATA_Z0);
	Counting_Temp = Counting_Temp|temp ;temp = 0;
	Acc_Get.Z = (Counting_Temp*1.0)/32767.0*16.0*9.8;
	
	return 0;
}
