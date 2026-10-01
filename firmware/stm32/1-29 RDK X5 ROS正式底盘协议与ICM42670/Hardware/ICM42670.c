#include "stm32f10x.h"                                 // 提供STM32F103RCT6寄存器与标准外设库定义
#include "ICM42670.h"                                  // 提供ICM42670驱动接口和采样结构体
#include "Delay.h"                                     // 提供软件I2C微秒延时和芯片启动延时

#define ICM42670_REG_DEVICE_CONFIG 0x11U                // 定义设备复位和时钟配置寄存器地址
#define ICM42670_REG_DRIVE_CONFIG 0x01U                 // 定义I2C驱动速度配置寄存器地址
#define ICM42670_REG_INT_CONFIG 0x06U                   // 定义中断电气配置寄存器地址
#define ICM42670_REG_INT_CONFIG1 0x05U                  // 定义中断扩展配置寄存器地址
#define ICM42670_REG_PWR_MGMT0 0x1FU                    // 定义陀螺仪和加速度计电源模式寄存器地址
#define ICM42670_REG_INT_SOURCE0 0x2BU                  // 定义中断源配置寄存器地址
#define ICM42670_REG_GYRO_CONFIG0 0x20U                 // 定义陀螺仪量程和输出频率寄存器地址
#define ICM42670_REG_ACCEL_CONFIG0 0x21U                // 定义加速度计量程和输出频率寄存器地址
#define ICM42670_REG_GYRO_CONFIG1 0x23U                 // 定义陀螺仪滤波配置寄存器地址
#define ICM42670_REG_GYRO_ACCEL_CONFIG0 0x24U           // 定义六轴低通滤波配置寄存器地址
#define ICM42670_REG_TEMP_DATA1 0x09U                   // 定义连续温度和六轴数据起始寄存器地址
#define ICM42670_REG_WHO_AM_I 0x75U                     // 定义芯片身份寄存器地址
#define ICM42670_SAMPLE_BYTE_COUNT 14U                  // 定义温度加六轴连续数据总字节数
#define ICM42670_ACCEL_RANGE_G 16.0f                    // 定义参考工程使用的正负十六g量程
#define ICM42670_GYRO_RANGE_DPS 2000.0f                 // 定义参考工程使用的正负两千度每秒量程
#define ICM42670_GRAVITY 9.80665f                       // 定义标准重力加速度换算常量
#define ICM42670_DEG_TO_RAD 0.01745329251994329577f     // 定义角度每秒到弧度每秒的换算常量
#define ICM42670_RAW_FULL_SCALE 32768.0f                // 定义有符号十六位传感器满量程计数

static uint8_t ICM42670Ready = 0U;                      // 保存ICM42670初始化是否成功
static uint8_t ICM42670DeviceId = 0U;                   // 保存最近一次读取到的WHO_AM_I值
static uint32_t ICM42670ReadErrorCount = 0UL;           // 累计记录I2C无应答和参数错误次数

/**
  * 函    数：设置软件I2C时钟线电平
  * 参    数：Level 零为低电平，非零为释放高电平
  * 返 回 值：无
  */
static void ICM42670_WriteScl(uint8_t Level)
{
    GPIO_WriteBit(GPIOB, GPIO_Pin_4, Level != 0U ? Bit_SET : Bit_RESET); // 通过PB4开漏输出设置SCL电平
    Delay_us(5U);                                        // 延时五微秒限制软件I2C频率并满足建立时间
}

/**
  * 函    数：设置软件I2C数据线电平
  * 参    数：Level 零为低电平，非零为释放高电平
  * 返 回 值：无
  */
static void ICM42670_WriteSda(uint8_t Level)
{
    GPIO_WriteBit(GPIOA, GPIO_Pin_12, Level != 0U ? Bit_SET : Bit_RESET); // 通过PA12开漏输出设置SDA电平
    Delay_us(5U);                                        // 延时五微秒限制软件I2C频率并满足建立时间
}

/**
  * 函    数：读取软件I2C数据线电平
  * 参    数：无
  * 返 回 值：零为低电平，一为高电平
  */
static uint8_t ICM42670_ReadSda(void)
{
    uint8_t Level = (uint8_t)GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_12); // 读取PA12当前输入电平
    Delay_us(5U);                                        // 延时五微秒保持读取时序稳定
    return Level;                                        // 返回从机当前输出的数据位
}

/**
  * 函    数：产生软件I2C起始条件
  * 参    数：无
  * 返 回 值：无
  */
static void ICM42670_Start(void)
{
    ICM42670_WriteSda(1U);                              // 先释放SDA保证总线处于空闲状态
    ICM42670_WriteScl(1U);                              // 再释放SCL进入起始条件准备状态
    ICM42670_WriteSda(0U);                              // 在SCL高电平期间拉低SDA产生起始条件
    ICM42670_WriteScl(0U);                              // 拉低SCL占用总线并准备发送首字节
}

/**
  * 函    数：产生软件I2C停止条件
  * 参    数：无
  * 返 回 值：无
  */
static void ICM42670_Stop(void)
{
    ICM42670_WriteSda(0U);                              // 先保持SDA低电平准备停止条件
    ICM42670_WriteScl(1U);                              // 释放SCL进入停止条件有效窗口
    ICM42670_WriteSda(1U);                              // 在SCL高电平期间释放SDA产生停止条件
}

/**
  * 函    数：发送一个软件I2C字节
  * 参    数：Byte 待发送的八位数据
  * 返 回 值：无
  */
static void ICM42670_SendByte(uint8_t Byte)
{
    uint8_t BitIndex;                                    // 保存当前发送的数据位下标
    for (BitIndex = 0U; BitIndex < 8U; BitIndex++)       // 从最高位开始依次发送八个数据位
    {
        ICM42670_WriteSda((uint8_t)((Byte & (uint8_t)(0x80U >> BitIndex)) != 0U)); // 把当前数据位放到SDA线上
        ICM42670_WriteScl(1U);                          // 释放SCL让从机在高电平期间采样数据
        ICM42670_WriteScl(0U);                          // 拉低SCL并准备发送下一数据位
    }
}

/**
  * 函    数：接收一个软件I2C字节
  * 参    数：无
  * 返 回 值：接收到的八位数据
  */
static uint8_t ICM42670_ReceiveByte(void)
{
    uint8_t BitIndex;                                    // 保存当前接收的数据位下标
    uint8_t Byte = 0U;                                   // 保存逐位拼接后的接收字节
    ICM42670_WriteSda(1U);                              // 释放SDA让从机驱动数据线
    for (BitIndex = 0U; BitIndex < 8U; BitIndex++)       // 从最高位开始依次接收八个数据位
    {
        ICM42670_WriteScl(1U);                          // 释放SCL进入数据有效高电平窗口
        if (ICM42670_ReadSda() != 0U)                   // 判断从机当前是否输出高电平
        {
            Byte |= (uint8_t)(0x80U >> BitIndex);       // 将当前接收位写入结果字节
        }
        ICM42670_WriteScl(0U);                          // 拉低SCL并准备接收下一数据位
    }
    return Byte;                                        // 返回完整接收到的八位数据
}

/**
  * 函    数：读取从机应答位
  * 参    数：无
  * 返 回 值：零表示应答成功，一表示无应答
  */
static uint8_t ICM42670_ReceiveAck(void)
{
    uint8_t AckBit;                                      // 保存从机返回的应答位
    ICM42670_WriteSda(1U);                              // 释放SDA让从机输出应答位
    ICM42670_WriteScl(1U);                              // 释放SCL进入应答采样窗口
    AckBit = ICM42670_ReadSda();                        // 读取从机应答电平
    ICM42670_WriteScl(0U);                              // 拉低SCL结束应答位时序
    return AckBit;                                      // 返回应答采样结果
}

/**
  * 函    数：发送主机应答位
  * 参    数：AckBit 零表示继续读取，一表示结束读取
  * 返 回 值：无
  */
static void ICM42670_SendAck(uint8_t AckBit)
{
    ICM42670_WriteSda(AckBit);                          // 把主机应答位放到SDA线上
    ICM42670_WriteScl(1U);                              // 释放SCL让从机采样应答位
    ICM42670_WriteScl(0U);                              // 拉低SCL结束应答位时序
}

/**
  * 函    数：写入一个ICM42670寄存器
  * 参    数：RegisterAddress 寄存器地址，Data 待写入值
  * 返 回 值：零表示成功，非零表示从机无应答
  */
static uint8_t ICM42670_WriteRegister(uint8_t RegisterAddress, uint8_t Data)
{
    uint8_t Error = 0U;                                  // 保存本次三个发送阶段的无应答状态
    ICM42670_Start();                                    // 产生I2C起始条件
    ICM42670_SendByte(ICM42670_I2C_WRITE_ADDRESS);       // 发送ICM42670写地址
    Error |= ICM42670_ReceiveAck();                      // 记录从机地址应答结果
    ICM42670_SendByte(RegisterAddress);                  // 发送目标寄存器地址
    Error |= ICM42670_ReceiveAck();                      // 记录寄存器地址应答结果
    ICM42670_SendByte(Data);                             // 发送目标寄存器配置值
    Error |= ICM42670_ReceiveAck();                      // 记录寄存器数据应答结果
    ICM42670_Stop();                                     // 产生I2C停止条件并释放总线
    if (Error != 0U)                                     // 判断本次写入是否存在任一无应答阶段
    {
        ICM42670ReadErrorCount++;                        // 累计一次I2C写入错误
    }
    return Error;                                        // 返回本次寄存器写入结果
}

/**
  * 函    数：连续读取ICM42670寄存器
  * 参    数：RegisterAddress 起始寄存器，Data 结果数组，Length 读取字节数
  * 返 回 值：零表示成功，非零表示参数无效或从机无应答
  */
static uint8_t ICM42670_ReadRegisters(uint8_t RegisterAddress, uint8_t *Data, uint8_t Length)
{
    uint8_t Index;                                       // 保存连续读取的当前字节下标
    if (Data == 0 || Length == 0U)                       // 判断调用者是否提供有效接收缓冲区和长度
    {
        ICM42670ReadErrorCount++;                        // 累计一次无效参数错误
        return 1U;                                       // 返回读取失败防止访问无效地址
    }
    ICM42670_Start();                                    // 产生第一次起始条件准备写入寄存器地址
    ICM42670_SendByte(ICM42670_I2C_WRITE_ADDRESS);       // 发送ICM42670写地址
    if (ICM42670_ReceiveAck() != 0U)                     // 判断从机是否应答写地址
    {
        ICM42670_Stop();                                 // 地址无应答时立即释放总线，禁止继续时钟出无效数据
        ICM42670ReadErrorCount++;                        // 累计一次写地址无应答错误
        return 2U;                                       // 返回写地址阶段失败
    }
    ICM42670_SendByte(RegisterAddress);                  // 发送连续读取的起始寄存器地址
    if (ICM42670_ReceiveAck() != 0U)                     // 判断从机是否接收起始寄存器地址
    {
        ICM42670_Stop();                                 // 寄存器地址无应答时立即释放总线
        ICM42670ReadErrorCount++;                        // 累计一次寄存器地址无应答错误
        return 3U;                                       // 返回寄存器地址阶段失败
    }
    ICM42670_Start();                                    // 产生重复起始条件切换到读取方向
    ICM42670_SendByte(ICM42670_I2C_READ_ADDRESS);        // 发送ICM42670读地址
    if (ICM42670_ReceiveAck() != 0U)                     // 判断从机是否应答读地址
    {
        ICM42670_Stop();                                 // 读地址无应答时立即释放总线
        ICM42670ReadErrorCount++;                        // 累计一次读地址无应答错误
        return 4U;                                       // 返回读地址阶段失败
    }
    for (Index = 0U; Index < Length; Index++)            // 依次读取请求的连续寄存器数据
    {
        Data[Index] = ICM42670_ReceiveByte();            // 保存当前寄存器读取结果
        ICM42670_SendAck((uint8_t)(Index + 1U == Length)); // 最后一个字节发送非应答，其余字节发送应答
    }
    ICM42670_Stop();                                     // 产生I2C停止条件并释放总线
    return 0U;                                           // 返回本次连续寄存器读取成功
}

/**
  * 函    数：读取一个有符号十六位大端传感器字段
  * 参    数：Data 高字节和低字节首地址
  * 返 回 值：拼接后的有符号原始计数
  */
static int16_t ICM42670_ReadBigEndianInt16(const uint8_t *Data)
{
    uint16_t Raw = ((uint16_t)Data[0] << 8) | Data[1];   // 按传感器高字节优先格式拼接无符号原始值
    return (int16_t)Raw;                                  // 按二补码解释并返回有符号原始计数
}

/**
  * 函    数：初始化ICM42670及软件I2C
  * 参    数：无
  * 返 回 值：零表示初始化成功，非零表示身份或配置失败
  */
uint8_t ICM42670_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                 // 定义软件I2C引脚初始化结构体
    uint8_t Error = 0U;                                  // 保存芯片身份和寄存器配置结果
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE); // 开启PA12和PB4所在GPIO端口时钟
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;     // 配置软件I2C引脚为开漏输出以允许总线上拉
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;    // 设置软件I2C引脚输出速度为五十兆赫兹
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;            // 选择PB4作为ICM42670软件I2C时钟线
    GPIO_Init(GPIOB, &GPIO_InitStructure);               // 将SCL引脚配置写入GPIOB
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;           // 选择PA12作为ICM42670软件I2C数据线
    GPIO_Init(GPIOA, &GPIO_InitStructure);               // 将SDA引脚配置写入GPIOA
    ICM42670_WriteScl(1U);                               // 释放SCL并恢复软件I2C空闲电平
    ICM42670_WriteSda(1U);                               // 释放SDA并恢复软件I2C空闲电平
    ICM42670Ready = 0U;                                  // 初始化开始前清除传感器可用状态
    ICM42670ReadErrorCount = 0UL;                        // 清除上电时的I2C错误统计
    Error |= ICM42670_ReadRegisters(ICM42670_REG_WHO_AM_I, &ICM42670DeviceId, 1U); // 读取芯片身份寄存器
    if (Error != 0U || ICM42670DeviceId != ICM42670_EXPECTED_ID) // 判断I2C通信和芯片身份是否符合参考工程
    {
        return 1U;                                       // 身份不匹配时保持不可用并结束初始化
    }
    Error |= ICM42670_WriteRegister(ICM42670_REG_DEVICE_CONFIG, 0x01U); // 执行ICM42670软复位
    Delay_ms(50U);                                       // 等待软复位和内部时钟稳定
    Error |= ICM42670_WriteRegister(ICM42670_REG_DEVICE_CONFIG, 0x00U); // 退出复位并使用默认时钟配置
    Error |= ICM42670_WriteRegister(ICM42670_REG_DRIVE_CONFIG, 0x05U); // 使用参考工程验证过的接口驱动配置
    Error |= ICM42670_WriteRegister(ICM42670_REG_INT_CONFIG, 0x02U); // 使用参考工程验证过的中断电气配置
    Error |= ICM42670_WriteRegister(ICM42670_REG_PWR_MGMT0, 0x0FU); // 开启陀螺仪和加速度计低噪声模式
    Error |= ICM42670_WriteRegister(ICM42670_REG_INT_CONFIG1, 0x00U); // 使用参考工程验证过的扩展中断配置
    Error |= ICM42670_WriteRegister(ICM42670_REG_INT_SOURCE0, 0x08U); // 使用参考工程验证过的数据就绪中断源配置
    Error |= ICM42670_WriteRegister(ICM42670_REG_GYRO_CONFIG0, 0x06U); // 配置陀螺仪正负两千度每秒和参考输出频率
    Error |= ICM42670_WriteRegister(ICM42670_REG_ACCEL_CONFIG0, 0x06U); // 配置加速度计正负十六g和参考输出频率
    Error |= ICM42670_WriteRegister(ICM42670_REG_GYRO_CONFIG1, 0x16U); // 配置参考工程验证过的陀螺仪滤波参数
    Error |= ICM42670_WriteRegister(ICM42670_REG_GYRO_ACCEL_CONFIG0, 0x11U); // 配置参考工程验证过的六轴低通滤波参数
    Delay_ms(50U);                                       // 等待六轴传感器启动并输出稳定数据
    ICM42670DeviceId = 0U;                               // 清除复位前读取的身份缓存
    Error |= ICM42670_ReadRegisters(ICM42670_REG_WHO_AM_I, &ICM42670DeviceId, 1U); // 复位配置后再次读取芯片身份
    if (Error != 0U || ICM42670DeviceId != ICM42670_EXPECTED_ID) // 判断最终身份与全部配置是否成功
    {
        return 2U;                                       // 最终检查失败时保持不可用并返回配置错误
    }
    ICM42670Ready = 1U;                                  // 标记ICM42670已经完成身份和配置检查
    return 0U;                                           // 返回初始化成功
}

/**
  * 函    数：读取并换算ICM42670六轴和温度
  * 参    数：Sample 用于保存标准单位采样结果的结构体地址
  * 返 回 值：零表示成功，非零表示未就绪或读取失败
  */
uint8_t ICM42670_ReadSample(ICM42670SampleTypeDef *Sample)
{
    uint8_t Raw[ICM42670_SAMPLE_BYTE_COUNT];             // 保存温度、加速度和陀螺仪连续原始字节
    int16_t TemperatureRaw;                              // 保存温度有符号原始计数
    int16_t AccelXRaw;                                   // 保存X轴加速度有符号原始计数
    int16_t AccelYRaw;                                   // 保存Y轴加速度有符号原始计数
    int16_t AccelZRaw;                                   // 保存Z轴加速度有符号原始计数
    int16_t GyroXRaw;                                    // 保存X轴角速度有符号原始计数
    int16_t GyroYRaw;                                    // 保存Y轴角速度有符号原始计数
    int16_t GyroZRaw;                                    // 保存Z轴角速度有符号原始计数
    if (Sample == 0 || ICM42670Ready == 0U)              // 判断结果地址和传感器初始化状态是否有效
    {
        ICM42670ReadErrorCount++;                        // 累计一次未就绪或无效参数错误
        return 1U;                                       // 返回读取失败防止输出无效数据
    }
    if (ICM42670_ReadRegisters(ICM42670_REG_TEMP_DATA1, Raw, ICM42670_SAMPLE_BYTE_COUNT) != 0U) // 单次事务原子读取十四字节，防止高低字节跨采样撕裂
    {
        return 2U;                                       // I2C地址无应答时返回读取失败
    }
    TemperatureRaw = ICM42670_ReadBigEndianInt16(&Raw[0]); // 拼接温度高低字节
    AccelXRaw = ICM42670_ReadBigEndianInt16(&Raw[2]);    // 拼接X轴加速度高低字节
    AccelYRaw = ICM42670_ReadBigEndianInt16(&Raw[4]);    // 拼接Y轴加速度高低字节
    AccelZRaw = ICM42670_ReadBigEndianInt16(&Raw[6]);    // 拼接Z轴加速度高低字节
    GyroXRaw = ICM42670_ReadBigEndianInt16(&Raw[8]);     // 拼接X轴角速度高低字节
    GyroYRaw = ICM42670_ReadBigEndianInt16(&Raw[10]);    // 拼接Y轴角速度高低字节
    GyroZRaw = ICM42670_ReadBigEndianInt16(&Raw[12]);    // 拼接Z轴角速度高低字节
    Sample->TemperatureCelsius = ((float)TemperatureRaw / 132.48f) + 25.0f; // 按参考工程公式换算芯片温度
    Sample->AccelXMeterPerSecond2 = ((float)AccelXRaw / ICM42670_RAW_FULL_SCALE) * ICM42670_ACCEL_RANGE_G * ICM42670_GRAVITY; // 换算X轴加速度标准单位
    Sample->AccelYMeterPerSecond2 = ((float)AccelYRaw / ICM42670_RAW_FULL_SCALE) * ICM42670_ACCEL_RANGE_G * ICM42670_GRAVITY; // 换算Y轴加速度标准单位
    Sample->AccelZMeterPerSecond2 = ((float)AccelZRaw / ICM42670_RAW_FULL_SCALE) * ICM42670_ACCEL_RANGE_G * ICM42670_GRAVITY; // 换算Z轴加速度标准单位
    Sample->GyroXRadianPerSecond = ((float)GyroXRaw / ICM42670_RAW_FULL_SCALE) * ICM42670_GYRO_RANGE_DPS * ICM42670_DEG_TO_RAD; // 换算X轴角速度标准单位
    Sample->GyroYRadianPerSecond = ((float)GyroYRaw / ICM42670_RAW_FULL_SCALE) * ICM42670_GYRO_RANGE_DPS * ICM42670_DEG_TO_RAD; // 换算Y轴角速度标准单位
    Sample->GyroZRadianPerSecond = ((float)GyroZRaw / ICM42670_RAW_FULL_SCALE) * ICM42670_GYRO_RANGE_DPS * ICM42670_DEG_TO_RAD; // 换算Z轴角速度标准单位
    return 0U;                                           // 返回本次六轴与温度读取成功
}

/**
  * 函    数：查询ICM42670是否可用
  * 参    数：无
  * 返 回 值：一表示已通过初始化，零表示不可用
  */
uint8_t ICM42670_IsReady(void)
{
    return ICM42670Ready;                                // 返回传感器身份和配置检查结果
}

/**
  * 函    数：读取ICM42670身份值
  * 参    数：无
  * 返 回 值：最近一次读取到的WHO_AM_I值
  */
uint8_t ICM42670_GetDeviceId(void)
{
    return ICM42670DeviceId;                             // 返回最近一次芯片身份检查结果
}

/**
  * 函    数：读取ICM42670累计错误次数
  * 参    数：无
  * 返 回 值：I2C无应答、未就绪或无效参数累计次数
  */
uint32_t ICM42670_GetReadErrorCount(void)
{
    return ICM42670ReadErrorCount;                       // 返回传感器通信和调用错误统计
}
