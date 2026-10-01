#include "stm32f10x.h"                  //提供STM32F10x寄存器与标准外设库定义
#include "OLED.h"                       //提供OLED显示驱动接口声明
#include "OLED_Font.h"                  //提供OLED使用的ASCII与中文字模数据

#define OLED_W_SCL(Level) GPIO_WriteBit(GPIOA, GPIO_Pin_5, (BitAction)(Level)) //控制PA5软件I2C时钟线电平
#define OLED_W_SDA(Level) GPIO_WriteBit(GPIOA, GPIO_Pin_4, (BitAction)(Level)) //控制PA4软件I2C数据线电平
#define OLED_I2C_ADDRESS 0x78U           //定义OLED写操作使用的八位从机地址
#define OLED_PAGE_COUNT 8U               //定义128乘64 OLED的页面数量
#define OLED_COLUMN_COUNT 128U           //定义OLED每页的像素列数量
#define OLED_ASCII_WIDTH 8U              //定义ASCII字符的显示宽度
#define OLED_CHINESE_WIDTH 20U           //定义当前中文字模的显示宽度

/**
  * 函    数：OLED软件I2C引脚初始化
  * 参    数：无
  * 返 回 值：无
  */
static void OLED_I2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;                                                    //定义GPIO初始化结构体

    /*开启GPIO时钟*/
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);                                    //开启OLED所在GPIOA端口时钟

    /*配置软件I2C引脚*/
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;                                        //配置为开漏输出以符合I2C总线电气特性
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;                                  //选择SDA对应PA4和SCL对应PA5引脚
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;                                       //设置GPIO输出速度为50MHz
    GPIO_Init(GPIOA, &GPIO_InitStructure);                                                   //将软件I2C引脚配置写入GPIOA

    /*释放总线*/
    OLED_W_SCL(1);                                                                           //释放SCL并使时钟线回到高电平
    OLED_W_SDA(1);                                                                           //释放SDA并使数据线回到高电平
}

/**
  * 函    数：产生软件I2C起始条件
  * 参    数：无
  * 返 回 值：无
  */
static void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);                                                                           //先释放数据线准备产生下降沿
    OLED_W_SCL(1);                                                                           //保持时钟线高电平以满足起始条件定义
    OLED_W_SDA(0);                                                                           //时钟高电平期间拉低数据线产生起始条件
    OLED_W_SCL(0);                                                                           //拉低时钟线并进入数据传输阶段
}

/**
  * 函    数：产生软件I2C停止条件
  * 参    数：无
  * 返 回 值：无
  */
static void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);                                                                           //先拉低数据线准备产生上升沿
    OLED_W_SCL(1);                                                                           //释放时钟线以满足停止条件定义
    OLED_W_SDA(1);                                                                           //时钟高电平期间释放数据线产生停止条件
}

/**
  * 函    数：软件I2C发送一个字节
  * 参    数：Byte 待发送的八位数据
  * 返 回 值：无
  */
static void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t BitIndex;                                                                        //保存当前发送的数据位下标

    for (BitIndex = 0U; BitIndex < 8U; BitIndex++)                                           //按从高位到低位顺序发送八个数据位
    {
        OLED_W_SDA(Byte & (0x80U >> BitIndex));                                              //根据当前数据位设置SDA电平
        OLED_W_SCL(1);                                                                       //释放SCL并让OLED采样当前数据位
        OLED_W_SCL(0);                                                                       //拉低SCL并准备发送下一数据位
    }
    OLED_W_SDA(1);                                                                           //释放SDA并忽略OLED应答位
    OLED_W_SCL(1);                                                                           //产生第九个时钟脉冲传输应答位
    OLED_W_SCL(0);                                                                           //结束应答时钟并继续后续传输
}

/**
  * 函    数：OLED写入命令
  * 参    数：Command 待写入的控制命令
  * 返 回 值：无
  */
static void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_Start();                                                                         //产生一次软件I2C起始条件
    OLED_I2C_SendByte(OLED_I2C_ADDRESS);                                                     //发送OLED写操作从机地址
    OLED_I2C_SendByte(0x00U);                                                                //发送命令流控制字节
    OLED_I2C_SendByte(Command);                                                              //发送指定OLED控制命令
    OLED_I2C_Stop();                                                                          //产生软件I2C停止条件并结束传输
}

/**
  * 函    数：OLED写入显示数据
  * 参    数：Data 待写入的八位像素数据
  * 返 回 值：无
  */
static void OLED_WriteData(uint8_t Data)
{
    OLED_I2C_Start();                                                                         //产生一次软件I2C起始条件
    OLED_I2C_SendByte(OLED_I2C_ADDRESS);                                                     //发送OLED写操作从机地址
    OLED_I2C_SendByte(0x40U);                                                                //发送显示数据流控制字节
    OLED_I2C_SendByte(Data);                                                                 //发送指定显示像素数据
    OLED_I2C_Stop();                                                                          //产生软件I2C停止条件并结束传输
}

/**
  * 函    数：设置OLED页列坐标
  * 参    数：Page 页面编号0至7，Column 列编号0至127
  * 返 回 值：无
  */
static void OLED_SetCursor(uint8_t Page, uint8_t Column)
{
    OLED_WriteCommand((uint8_t)(0xB0U | Page));                                              //设置目标显示页面
    OLED_WriteCommand((uint8_t)(0x10U | ((Column & 0xF0U) >> 4U)));                           //设置列地址高四位
    OLED_WriteCommand((uint8_t)(Column & 0x0FU));                                            //设置列地址低四位
}

/**
  * 函    数：计算无符号整数次方
  * 参    数：Base 底数，Exponent 指数
  * 返 回 值：Base的Exponent次方
  */
static uint32_t OLED_Pow(uint32_t Base, uint32_t Exponent)
{
    uint32_t Result = 1U;                                                                    //将乘方结果初始化为1

    while (Exponent > 0U)                                                                    //按指数次数循环执行乘法
    {
        Result *= Base;                                                                      //将底数累乘到结果中
        Exponent--;                                                                          //减少一次尚未执行的乘法次数
    }

    return Result;                                                                           //返回计算完成的整数乘方结果
}

/**
  * 函    数：清空OLED显示
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Clear(void)
{
    uint8_t Page;                                                                            //保存当前清屏页面编号
    uint8_t Column;                                                                          //保存当前清屏列编号

    for (Page = 0U; Page < OLED_PAGE_COUNT; Page++)                                          //依次遍历OLED全部八个页面
    {
        OLED_SetCursor(Page, 0U);                                                            //将写入坐标移动到当前页面首列
        for (Column = 0U; Column < OLED_COLUMN_COUNT; Column++)                               //依次遍历当前页面全部128列
        {
            OLED_WriteData(0x00U);                                                           //写入全零像素数据并清除当前列
        }
    }
}

/**
  * 函    数：OLED显示一个ASCII字符
  * 参    数：Line 行号1至4，Column 列号1至16，Character 可见ASCII字符
  * 返 回 值：无
  */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Character)
{
    uint8_t ByteIndex;                                                                       //保存当前写入的字模字节下标
    uint8_t CharacterIndex = (uint8_t)(Character - ' ');                                     //将ASCII码转换为字模数组下标

    OLED_SetCursor((uint8_t)((Line - 1U) * 2U), (uint8_t)((Column - 1U) * OLED_ASCII_WIDTH)); //设置字符上半部分显示坐标
    for (ByteIndex = 0U; ByteIndex < OLED_ASCII_WIDTH; ByteIndex++)                           //依次写入字符上半部分八个字节
    {
        OLED_WriteData(OLED_F8x16[CharacterIndex][ByteIndex]);                               //写入当前字符上半部分字模数据
    }
    OLED_SetCursor((uint8_t)((Line - 1U) * 2U + 1U), (uint8_t)((Column - 1U) * OLED_ASCII_WIDTH)); //设置字符下半部分显示坐标
    for (ByteIndex = 0U; ByteIndex < OLED_ASCII_WIDTH; ByteIndex++)                           //依次写入字符下半部分八个字节
    {
        OLED_WriteData(OLED_F8x16[CharacterIndex][ByteIndex + OLED_ASCII_WIDTH]);             //写入当前字符下半部分字模数据
    }
}

/**
  * 函    数：OLED显示ASCII字符串
  * 参    数：Line 行号1至4，Column 起始列号1至16，String 字符串首地址
  * 返 回 值：无
  */
void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String)
{
    uint8_t Index;                                                                           //保存当前显示的字符串字符下标

    for (Index = 0U; String[Index] != '\0'; Index++)                                         //依次遍历字符串直至遇到结束符
    {
        OLED_ShowChar(Line, (uint8_t)(Column + Index), String[Index]);                       //在相邻显示列写入当前字符
    }
}

/**
  * 函    数：OLED显示无符号十进制数字
  * 参    数：Line 行号，Column 起始列号，Number 数值，Length 显示位数
  * 返 回 值：无
  */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t Index;                                                                           //保存当前显示的十进制数字位下标

    for (Index = 0U; Index < Length; Index++)                                                //按指定长度遍历每一位十进制数字
    {
        OLED_ShowChar(Line, (uint8_t)(Column + Index), (char)(Number / OLED_Pow(10U, Length - Index - 1U) % 10U + '0')); //显示当前十进制位
    }
}

/**
  * 函    数：OLED显示带符号十进制数字
  * 参    数：Line 行号，Column 起始列号，Number 数值，Length 数字位数
  * 返 回 值：无
  */
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
    uint32_t Magnitude;                                                                      //保存去除符号后的数字绝对值

    if (Number >= 0)                                                                         //判断待显示数字是否为非负数
    {
        OLED_ShowChar(Line, Column, '+');                                                    //显示正号字符
        Magnitude = (uint32_t)Number;                                                        //直接保存非负数数值
    }
    else                                                                                     //处理待显示数字为负数的情况
    {
        OLED_ShowChar(Line, Column, '-');                                                    //显示负号字符
        Magnitude = (uint32_t)(-Number);                                                     //取负数绝对值用于后续显示
    }
    OLED_ShowNum(Line, (uint8_t)(Column + 1U), Magnitude, Length);                            //在符号之后显示指定长度的绝对值
}

/**
  * 函    数：OLED显示无符号十六进制数字
  * 参    数：Line 行号，Column 起始列号，Number 数值，Length 显示位数
  * 返 回 值：无
  */
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t Index;                                                                           //保存当前显示的十六进制数字位下标
    uint8_t Digit;                                                                           //保存当前十六进制数字位的数值

    for (Index = 0U; Index < Length; Index++)                                                //按指定长度遍历每一位十六进制数字
    {
        Digit = (uint8_t)(Number / OLED_Pow(16U, Length - Index - 1U) % 16U);                 //计算当前十六进制位数值
        if (Digit < 10U)                                                                      //判断当前位是否为0至9
        {
            OLED_ShowChar(Line, (uint8_t)(Column + Index), (char)(Digit + '0'));              //将0至9转换为数字字符并显示
        }
        else                                                                                 //处理当前位为10至15的情况
        {
            OLED_ShowChar(Line, (uint8_t)(Column + Index), (char)(Digit - 10U + 'A'));        //将10至15转换为A至F并显示
        }
    }
}

/**
  * 函    数：OLED显示无符号二进制数字
  * 参    数：Line 行号，Column 起始列号，Number 数值，Length 显示位数
  * 返 回 值：无
  */
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
    uint8_t Index;                                                                           //保存当前显示的二进制数字位下标

    for (Index = 0U; Index < Length; Index++)                                                //按指定长度遍历每一位二进制数字
    {
        OLED_ShowChar(Line, (uint8_t)(Column + Index), (char)(Number / OLED_Pow(2U, Length - Index - 1U) % 2U + '0')); //显示当前二进制位
    }
}

/**
  * 函    数：OLED显示带符号浮点数字
  * 参    数：Line 行号，Column 起始列号，Number 数值，Length 总数字位数，FractionLength 小数位数
  * 返 回 值：无
  */
void OLED_ShowFloat(uint8_t Line, uint8_t Column, float Number, uint8_t Length, uint8_t FractionLength)
{
    uint32_t ScaledNumber;                                                                   //保存按小数位数放大后的整数值
    uint8_t IntegerLength = (uint8_t)(Length - FractionLength);                              //计算小数点前的数字位数

    if (Number >= 0.0f)                                                                      //判断待显示浮点数是否为非负数
    {
        OLED_ShowChar(Line, Column, '+');                                                    //显示正号字符
    }
    else                                                                                     //处理待显示浮点数为负数的情况
    {
        OLED_ShowChar(Line, Column, '-');                                                    //显示负号字符
        Number = -Number;                                                                    //将数值转换为正的绝对值
    }
    ScaledNumber = (uint32_t)(Number * (float)OLED_Pow(10U, FractionLength));                //将浮点数按小数位数放大为整数
    OLED_ShowNum(Line, (uint8_t)(Column + 1U), ScaledNumber / OLED_Pow(10U, FractionLength), IntegerLength); //显示整数部分
    OLED_ShowChar(Line, (uint8_t)(Column + IntegerLength + 1U), '.');                         //在整数部分之后显示小数点
    OLED_ShowNum(Line, (uint8_t)(Column + IntegerLength + 2U), ScaledNumber % OLED_Pow(10U, FractionLength), FractionLength); //显示小数部分
}

/**
  * 函    数：OLED显示一个中文字模
  * 参    数：Line 行号1至4，Column 起始中文字列号，Index 中文字模下标
  * 返 回 值：无
  */
void OLED_ShowCN(uint8_t Line, uint8_t Column, uint8_t Index)
{
    uint8_t ByteIndex;                                                                       //保存当前写入的中文字模字节下标

    OLED_SetCursor((uint8_t)((Line - 1U) * 2U), (uint8_t)((Column - 1U) * OLED_CHINESE_WIDTH)); //设置中文字模上半部分坐标
    for (ByteIndex = 0U; ByteIndex < OLED_CHINESE_WIDTH; ByteIndex++)                         //依次写入中文字模上半部分数据
    {
        OLED_WriteData(OLED_F10x16[Index][ByteIndex]);                                       //写入当前中文字模上半部分字节
    }
    OLED_SetCursor((uint8_t)((Line - 1U) * 2U + 1U), (uint8_t)((Column - 1U) * OLED_CHINESE_WIDTH)); //设置中文字模下半部分坐标
    for (ByteIndex = 0U; ByteIndex < OLED_CHINESE_WIDTH; ByteIndex++)                         //依次写入中文字模下半部分数据
    {
        OLED_WriteData(OLED_F10x16[Index][ByteIndex + OLED_CHINESE_WIDTH]);                   //写入当前中文字模下半部分字节
    }
}

/**
  * 函    数：OLED初始化
  * 参    数：无
  * 返 回 值：无
  */
void OLED_Init(void)
{
    volatile uint32_t DelayCount;                                                            //保存上电稳定等待循环计数

    /*等待OLED电源稳定*/
    for (DelayCount = 0U; DelayCount < 800000U; DelayCount++)                                //执行短暂空循环等待OLED上电稳定
    {
    }

    /*初始化总线与控制器*/
    OLED_I2C_Init();                                                                          //初始化PA5和PA4软件I2C引脚
    OLED_WriteCommand(0xAEU);                                                                //关闭显示以安全修改控制器参数
    OLED_WriteCommand(0xD5U);                                                                //选择显示时钟分频与振荡频率寄存器
    OLED_WriteCommand(0x80U);                                                                //设置默认显示时钟分频与振荡频率
    OLED_WriteCommand(0xA8U);                                                                //选择多路复用率寄存器
    OLED_WriteCommand(0x3FU);                                                                //设置64行显示对应的多路复用率
    OLED_WriteCommand(0xD3U);                                                                //选择显示偏移寄存器
    OLED_WriteCommand(0x00U);                                                                //设置显示偏移为零
    OLED_WriteCommand(0x40U);                                                                //设置显示起始行为零
    OLED_WriteCommand(0xA1U);                                                                //设置段重映射方向以匹配屏幕安装方向
    OLED_WriteCommand(0xC8U);                                                                //设置COM扫描方向以匹配屏幕安装方向
    OLED_WriteCommand(0xDAU);                                                                //选择COM引脚硬件配置寄存器
    OLED_WriteCommand(0x12U);                                                                //设置顺序COM引脚并关闭左右重映射
    OLED_WriteCommand(0x81U);                                                                //选择对比度控制寄存器
    OLED_WriteCommand(0xCFU);                                                                //设置OLED显示对比度
    OLED_WriteCommand(0xD9U);                                                                //选择预充电周期寄存器
    OLED_WriteCommand(0xF1U);                                                                //设置内部电荷泵模式的预充电周期
    OLED_WriteCommand(0xDBU);                                                                //选择VCOMH取消选择电平寄存器
    OLED_WriteCommand(0x30U);                                                                //设置VCOMH取消选择电平
    OLED_WriteCommand(0xA4U);                                                                //允许显存内容控制实际像素显示
    OLED_WriteCommand(0xA6U);                                                                //选择正常显示而非像素反相显示
    OLED_WriteCommand(0x8DU);                                                                //选择电荷泵配置寄存器
    OLED_WriteCommand(0x14U);                                                                //开启OLED内部电荷泵
    OLED_WriteCommand(0xAFU);                                                                //开启OLED面板显示输出
    OLED_Clear();                                                                             //清空显存并得到全黑初始画面
}
