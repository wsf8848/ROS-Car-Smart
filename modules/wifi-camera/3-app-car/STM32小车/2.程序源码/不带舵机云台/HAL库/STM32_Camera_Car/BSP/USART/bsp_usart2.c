#include "bsp_usart2.h"

// 函数功能:打开串口2接收中断功能
// 传入函数:无
// Function function: Enable serial port 2 to receive interrupt function
// Incoming function: None
void USART2_UART_Init(void)
{

	LL_USART_EnableIT_RXNE(USART2); // Start receiving interrupt 启动接收中断
}

/**
 * @brief This function handles USART2 global interrupt.
 */
void USART2_IRQHandler(void)
{
	uint8_t rx2_temp;
	if (LL_USART_IsEnabledIT_RXNE(USART2)) // Determine if there is any interruption information 判断是否有中断信息
	{
		// LL_USART_ClearFlag_RXNE(USART2); //clear interrupt 清除中断
		rx2_temp = LL_USART_ReceiveData8(USART2); // Read information and clear interrupts 读取信息并清除中断
		Data_Deal(rx2_temp);
		//		USART2_DataByte(rx2_temp);//send data 发送数据
	}
}

// Send a Byte 发送一个字节
// data_byte:Sent data 发送的数据
void USART2_DataByte(uint8_t data_byte)
{
	while (!LL_USART_IsActiveFlag_TXE(USART2))
	{
	};
	LL_USART_TransmitData8(USART2, data_byte);
}

// Set to send a string 设置发送一个字符串
// data_str :The first address of the data 数据的首地址
// datasize :The length of data 数据的长度
void USART2_DataString(uint8_t *data_str, uint16_t datasize)
{
	for (uint8_t len = 0; len < datasize; len++)
	{
		USART2_DataByte(*(data_str + len));
	}
}
