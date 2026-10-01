#include "stm32f10x.h"                 
#include "Delay.h"
#include "MyI2C.h"

void MyI2C_W_SCL(uint8_t BitValue)   //类似宏定义实现IO操作
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_0, (BitAction)BitValue);		//根据BitValue，设置SCL引脚的电平
	Delay_us(10);												//延时10us，防止时序频率超过要求
}

void MyI2C_W_SDA(uint8_t BitValue)
{
	GPIO_WriteBit(GPIOB, GPIO_Pin_1, (BitAction)BitValue);		//根据BitValue，设置SDA引脚的电平，BitValue要实现非0即1的特性
	Delay_us(10);												//延时10us，防止时序频率超过要求
}

uint8_t MyI2C_R_SDA(void)
{
	uint8_t BitValue;
	BitValue = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_1);		//读取SDA电平
	Delay_us(10);												//延时10us，防止时序频率超过要求
	return BitValue;											//返回SDA电平
}

void MyI2C_Init(void)    //I2C初始化
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; //开漏输出悬空默认不接任何电平，而推挽输出悬空默认输入高电平
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1; //将B0，B1引脚初始化为开漏输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);					

	GPIO_SetBits(GPIOB, GPIO_Pin_0 | GPIO_Pin_1);			//设置PB10和PB11引脚初始化后默认为高电平（释放总线状态）
}

void MyI2C_Start(void)
{
	MyI2C_W_SDA(1);							//释放SDA，确保SDA为高电平
	MyI2C_W_SCL(1);							//释放SCL，确保SCL为高电平
	MyI2C_W_SDA(0);							//在SCL高电平期间，拉低SDA，产生起始信号
	MyI2C_W_SCL(0);							//起始后把SCL也拉低，即为了占用总线，也为了方便总线时序的拼接
}

void MyI2C_Stop(void)
{
	MyI2C_W_SDA(0);							//拉低SDA，确保SDA为低电平
	MyI2C_W_SCL(1);							//释放SCL，使SCL呈现高电平
	MyI2C_W_SDA(1);							//在SCL高电平期间，释放SDA，产生终止信号
}

void MyI2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i ++)				//循环8次，主机依次发送数据的每一位，即依次为0x80,0x40,0x20,0x10
	{
		MyI2C_W_SDA(Byte & (0x80 >> i));	//1000 0000 &1高位意思
		MyI2C_W_SCL(1);						//释放SCL，从机自动在SCL高电平期间读取SDA
		MyI2C_W_SCL(0);						//读取完成后拉低SCL，主机开始发送下一位数据
	}
}

uint8_t MyI2C_ReceiveByte(void)
{
	uint8_t i, Byte = 0x00;					//定义接收的数据，并赋初值0x00，此处必须赋初值0x00，后面会用到
	MyI2C_W_SDA(1);							//接收前，主机先确保释放SDA，避免干扰从机的数据发送
	for (i = 0; i < 8; i ++)				//循环8次，主机依次接收数据的每一位
	{
		MyI2C_W_SCL(1);						//释放SCL，主机机在SCL高电平期间读取SDA
		if (MyI2C_R_SDA() == 1){Byte |= (0x80 >> i);}	//读取SDA数据，并存储到Byte变量
														//当SDA为1时，置变量指定位为1，当SDA为0时，不做处理，指定位为默认的初值0
		MyI2C_W_SCL(0);						//拉低SCL，从机在SCL低电平期间写入SDA
	}
	return Byte;							//返回接收到的一个字节数据
}

void MyI2C_SendAck(uint8_t AckBit)
{
	MyI2C_W_SDA(AckBit);				    //主机把应答位数据放到SDA线
	MyI2C_W_SCL(1);							//释放SCL，从机在SCL高电平期间，读取应答位
	MyI2C_W_SCL(0);							//拉低SCL，开始下一个时序模块
}

uint8_t MyI2C_ReceiveAck(void)
{
	uint8_t AckBit;							//定义应答位变量
	MyI2C_W_SDA(1);							//接收前，主机先确保释放SDA，避免干扰从机的数据发送
	MyI2C_W_SCL(1);							//释放SCL，主机机在SCL高电平期间读取SDA
	AckBit = MyI2C_R_SDA();					//将应答位存储到变量里
	MyI2C_W_SCL(0);							//拉低SCL，开始下一个时序模块
	return AckBit;							//返回定义应答位变量
}
