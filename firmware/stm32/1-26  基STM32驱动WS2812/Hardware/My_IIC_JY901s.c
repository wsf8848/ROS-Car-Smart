#include "My_IIC_JY901s.h"
#include "MyI2C.h"
#include "Delay.h"

/**
 * @brief  检测JY901S是否在线（通过发送设备地址并等待应答）
 * @return 1-在线，0-离线
 */
uint8_t JY901S_IsOnline(void) 
{
    uint8_t online = 0;
    
    // 起始信号
    MyI2C_Start();
    
    // 发送设备写地址（7位地址左移1位 + 0（写方向））
    MyI2C_SendByte((JY901S_SLAVE_ADDR << 1) | 0x00);
    
    // 等待应答（接收应答位）
    if (MyI2C_ReceiveAck() == 0) {  // 0表示应答成功
        online = 1;
    }
    
    // 停止信号
    MyI2C_Stop();
    
    return online;
}

/**
 * @brief  读取JY901S的设备ID（寄存器地址需根据手册确认）
 * @return 设备ID值，0xFF表示读取失败
 */
uint8_t JY901S_ReadID(void)
{
    uint8_t id = 0xFF;
    
    // 检测设备是否在线
    if (!JY901S_IsOnline()) {
        return 0xFF;
    }
    
    // 步骤1：发送设备写地址 + 寄存器地址
    MyI2C_Start();
    MyI2C_SendByte((JY901S_SLAVE_ADDR << 1) | 0x00);  // 写方向
    if (MyI2C_ReceiveAck() != 0) {  // 应答失败
        MyI2C_Stop();
        return 0xFF;
    }
    
    MyI2C_SendByte(JY901S_WHO_AM_I);  // 发送寄存器地址
    if (MyI2C_ReceiveAck() != 0) {
        MyI2C_Stop();
        return 0xFF;
    }
    
    // 步骤2：重新起始，发送设备读地址
    MyI2C_Start();
    MyI2C_SendByte((JY901S_SLAVE_ADDR << 1) | 0x01);  // 读方向
    if (MyI2C_ReceiveAck() != 0) {
        MyI2C_Stop();
        return 0xFF;
    }
    
    // 步骤3：读取数据（发送NACK表示停止接收）
    id = MyI2C_ReceiveByte();
    MyI2C_SendAck(1);  // 发送NACK（1表示非应答，结束读取）
    
    MyI2C_Stop();  // 停止信号
    
    return id;
}