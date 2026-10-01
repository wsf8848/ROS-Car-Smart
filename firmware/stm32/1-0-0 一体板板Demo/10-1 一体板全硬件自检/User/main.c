/**
 * @user    Bee
 * @核心功能 一体板LED、按键、蜂鸣器、OLED、电池、IMU、四电机编码器、六舵机和四串口分阶段自检
 * @时间   2026-09-16
 * @版本   1.0.0
 * @修改历史 :V1.0.0 新建一体板全硬件安全自检工程
 */

#include "stm32f10x.h"                                // 提供STM32F103RCT6寄存器和标准外设库定义
#include "Delay.h"                                    // 提供自检动作所需毫秒延时
#include "LED.h"                                      // 提供红绿蓝三色板载LED控制接口
#include "Key.h"                                      // 提供KEY1至KEY3按键读取接口
#include "Buzzer.h"                                   // 提供板载蜂鸣器控制接口
#include "OLED.h"                                     // 提供板载OLED状态显示接口
#include "Battery.h"                                  // 提供PC4电池电压ADC采样接口
#include "ICM_42670.h"                                // 提供板载ICM42670惯性传感器接口
#include "Serial.h"                                   // 提供USART2调试输出和命令接收接口
#include "PortTest.h"                                 // 提供四路串口回环检测接口
#include "Motor.h"                                    // 提供四路电机方向和速度控制接口
#include "Encoder.h"                                  // 提供四路电机编码器增量读取接口
#include "ServoTest.h"                                // 提供六路舵机分阶段测试接口

#define MOTOR_TEST_PWM 900                             // 定义电机测试占空比为满量程的四分之一
#define MOTOR_TEST_RUN_MS 600U                         // 定义每个电机单方向运行六百毫秒
#define MOTOR_TEST_MIN_PULSES 2                        // 定义编码器判定有效所需最少脉冲数
#define HOME_REFRESH_LOOPS 100U                        // 定义主页面约一秒刷新一次状态

static uint8_t ImuDeviceId = 0U;                       // 保存ICM42670的WHO_AM_I寄存器值
extern GYRO Gyro_Get;                                  // 引用ICM42670驱动维护的三轴角速度数据
extern ACC Acc_Get;                                    // 引用ICM42670驱动维护的三轴加速度数据
extern TEMP Temp;                                      // 引用ICM42670驱动维护的温度数据

/* ==================== 通用显示与数值处理 ==================== */
static int16_t Test_Abs16(int16_t Value)
{
    if (Value < 0)                                     // 判断编码器增量是否为负数
    {
        return (int16_t)(-Value);                       // 返回负数对应的正幅值
    }
    return Value;                                      // 正数和零直接返回原值
}

static void Test_ShowHome(void)
{
    uint16_t VoltageMv = Battery_GetVoltageMv();       // 读取最近一次滤波后的电池电压

    OLED_Clear();                                      // 清除上一个测试页面的残留字符
    OLED_ShowString(1U, 1U, "ALL HW TEST");           // 显示统一硬件自检标题
    OLED_ShowString(2U, 1U, "IMU:");                  // 显示IMU检测字段名称
    if (ImuDeviceId == 0x47U)                          // 判断ICM42670芯片ID是否正确
    {
        OLED_ShowString(2U, 5U, "OK 47");             // 显示IMU识别通过
    }
    else                                               // 处理未识别到正确IMU的情况
    {
        OLED_ShowString(2U, 5U, "FAIL ");             // 显示IMU识别失败
        OLED_ShowHexNum(2U, 10U, ImuDeviceId, 2U);     // 显示实际读取到的芯片ID
    }
    OLED_ShowString(3U, 1U, "BAT:");                  // 显示电池电压字段名称
    OLED_ShowNum(3U, 5U, VoltageMv / 1000U, 2U);       // 显示电池电压整数伏部分
    OLED_ShowChar(3U, 7U, '.');                        // 显示电池电压小数点
    OLED_ShowNum(3U, 8U, (VoltageMv % 1000U) / 100U, 1U); // 显示电池电压十分位
    OLED_ShowString(3U, 9U, "V");                     // 显示电压单位
    OLED_ShowString(4U, 1U, "K1:M K2:S K3:U");        // 显示三个按键对应的测试入口
}

/* ==================== 无动作外设自检 ==================== */
static void Test_RunBasic(void)
{
    uint8_t SampleIndex;                               // 保存电池ADC预采样次数下标

    OLED_Clear();                                      // 清屏并准备显示上电测试阶段
    OLED_ShowString(1U, 1U, "BASIC TEST");            // 显示基础外设测试标题
    OLED_ShowString(2U, 1U, "LED R G B");             // 提示即将依次点亮三色LED
    LED_R_ON();                                        // 点亮红色LED供目视检查
    Delay_ms(300U);                                    // 保持红灯三百毫秒
    LED_R_OFF();                                       // 熄灭红色LED
    LED_G_ON();                                        // 点亮绿色LED供目视检查
    Delay_ms(300U);                                    // 保持绿灯三百毫秒
    LED_G_OFF();                                       // 熄灭绿色LED
    LED_B_ON();                                        // 点亮蓝色LED供目视检查
    Delay_ms(300U);                                    // 保持蓝灯三百毫秒
    LED_B_OFF();                                       // 熄灭蓝色LED
    OLED_ShowString(3U, 1U, "BUZZER");                // 提示即将测试板载蜂鸣器
    Buzzer_ON();                                       // 开启蜂鸣器供听音检查
    Delay_ms(250U);                                    // 保持蜂鸣器鸣响二百五十毫秒
    Buzzer_OFF();                                      // 关闭蜂鸣器
    for (SampleIndex = 0U; SampleIndex < 12U; SampleIndex++) // 多次采样使电池滤波值快速收敛
    {
        Battery_Update();                              // 更新一次PC4电池电压采样结果
        Delay_ms(10U);                                 // 相邻ADC采样间隔十毫秒
    }
    OLED_ShowString(4U, 1U, "IMU CHECK");             // 提示正在读取板载IMU
    ICM_42670_Init();                                  // 初始化ICM42670的软件I2C和工作模式
    ImuDeviceId = ICM_42670_GetID();                   // 读取WHO_AM_I寄存器作为在线判据
    Serial_Printf("\r\n[BOARD TEST] basic: imu_id=0x%02X, battery=%umV\r\n", ImuDeviceId, Battery_GetVoltageMv()); // 通过USART2输出基础检测数据
    Test_ShowHome();                                   // 返回统一自检主页面
}

/* ==================== 电机与编码器自检 ==================== */
static int16_t Test_ReadEncoder(uint8_t MotorIndex)
{
    switch (MotorIndex)                                // 按电机编号选择对应编码器计数器
    {
        case 0U: return EncoderA_Get();                 // 读取左后轮TIM3编码器增量
        case 1U: return EncoderB_Get();                 // 读取左前轮TIM2编码器增量
        case 2U: return EncoderC_Get();                 // 读取右前轮TIM4编码器增量
        case 3U: return EncoderD_Get();                 // 读取右后轮TIM5编码器增量
        default: return 0;                              // 无效电机编号返回零
    }
}

static void Test_SetOneMotor(uint8_t MotorIndex, int16_t Speed)
{
    Motor_AllOff();                                    // 每次切换前先确保其余三个电机完全停止
    switch (MotorIndex)                                // 按电机编号选择唯一动作通道
    {
        case 0U: MotorA_SetSpeed(Speed); break;         // 设置左后轮A通道测试速度
        case 1U: MotorB_SetSpeed(Speed); break;         // 设置左前轮B通道测试速度
        case 2U: MotorC_SetSpeed(Speed); break;         // 设置右前轮C通道测试速度
        case 3U: MotorD_SetSpeed(Speed); break;         // 设置右后轮D通道测试速度
        default: break;                                 // 无效编号保持全部电机关闭
    }
}

static void Test_RunMotors(void)
{
    uint8_t MotorIndex;                                 // 保存当前正在测试的电机编号
    uint8_t PassedCount = 0U;                           // 统计正反转编码器均有效的电机数量
    int16_t ForwardCount;                               // 保存当前电机正转编码器脉冲
    int16_t ReverseCount;                               // 保存当前电机反转编码器脉冲

    OLED_Clear();                                      // 清除主页面并进入电机安全提示页
    OLED_ShowString(1U, 1U, "LIFT ALL WHEELS");       // 再次提醒测试前必须架空四轮
    OLED_ShowString(2U, 1U, "MOTOR TEST 2S");         // 显示电机测试即将开始
    Buzzer_ON();                                       // 用短鸣提示危险动作即将开始
    Delay_ms(150U);                                    // 保持短鸣一百五十毫秒
    Buzzer_OFF();                                      // 关闭危险动作提示音
    Delay_ms(2000U);                                   // 留出两秒确认车辆已经架空
    MotorABCD_Init();                                  // 初始化TIM8四路电机PWM与方向引脚
    Motor_AllOff();                                    // 初始化完成后再次强制全部电机停止
    EncoderA_Init();                                   // 初始化左后轮编码器
    EncoderB_Init();                                   // 初始化左前轮编码器
    EncoderC_Init();                                   // 初始化右前轮编码器
    EncoderD_Init();                                   // 初始化右后轮编码器
    for (MotorIndex = 0U; MotorIndex < 4U; MotorIndex++) // 依次测试A至D四个电机避免车辆大幅动作
    {
        OLED_Clear();                                  // 清除上一电机的结果页面
        OLED_ShowString(1U, 1U, "MOTOR:");            // 显示当前电机字段名称
        OLED_ShowChar(1U, 7U, (char)('A' + MotorIndex)); // 显示A至D电机编号
        OLED_ShowString(2U, 1U, "FORWARD");           // 显示正转测试阶段
        (void)Test_ReadEncoder(MotorIndex);             // 清除测试前残留的编码器计数
        Test_SetOneMotor(MotorIndex, MOTOR_TEST_PWM);  // 以四分之一占空比驱动当前电机正转
        Delay_ms(MOTOR_TEST_RUN_MS);                    // 保持正转六百毫秒
        Motor_AllOff();                                // 正转采样结束后立即关闭全部电机
        ForwardCount = Test_ReadEncoder(MotorIndex);   // 读取正转阶段编码器增量
        OLED_ShowString(3U, 1U, "REVERSE");           // 显示反转测试阶段
        Test_SetOneMotor(MotorIndex, -MOTOR_TEST_PWM); // 以相同占空比驱动当前电机反转
        Delay_ms(MOTOR_TEST_RUN_MS);                    // 保持反转六百毫秒
        Motor_AllOff();                                // 反转采样结束后立即关闭全部电机
        ReverseCount = Test_ReadEncoder(MotorIndex);   // 读取反转阶段编码器增量
        if ((Test_Abs16(ForwardCount) >= MOTOR_TEST_MIN_PULSES) && (Test_Abs16(ReverseCount) >= MOTOR_TEST_MIN_PULSES)) // 判断双方向编码器是否均有有效脉冲
        {
            PassedCount++;                             // 当前电机与编码器组合判定通过
            OLED_ShowString(4U, 1U, "ENCODER OK");    // 在OLED显示当前编码器通过
        }
        else                                           // 处理电机未转或编码器无反馈的情况
        {
            OLED_ShowString(4U, 1U, "ENCODER FAIL");  // 在OLED显示当前编码器失败
        }
        Serial_Printf("[MOTOR %c] forward=%d reverse=%d %s\r\n", 'A' + MotorIndex, ForwardCount, ReverseCount, ((Test_Abs16(ForwardCount) >= MOTOR_TEST_MIN_PULSES) && (Test_Abs16(ReverseCount) >= MOTOR_TEST_MIN_PULSES)) ? "PASS" : "FAIL"); // 输出当前轮详细脉冲与判定
        Delay_ms(800U);                                // 保留结果页面便于现场观察
    }
    Motor_AllOff();                                    // 测试结束后保持全部电机方向脚和PWM关闭
    OLED_Clear();                                      // 清除单轮结果并显示综合结论
    OLED_ShowString(1U, 1U, "MOTOR RESULT");          // 显示电机综合结果标题
    OLED_ShowString(2U, 1U, "PASS:");                 // 显示通过数量字段名称
    OLED_ShowNum(2U, 6U, PassedCount, 1U);             // 显示通过的电机数量
    OLED_ShowString(2U, 7U, "/4");                    // 显示电机总数量
    if (PassedCount == 4U)                             // 判断四套电机与编码器是否全部通过
    {
        LED_G_ON();                                    // 全部通过时点亮绿色LED
    }
    else                                               // 处理至少一套电机或编码器失败的情况
    {
        LED_R_ON();                                    // 存在失败时点亮红色LED
    }
    Delay_ms(2500U);                                   // 保留综合结果页面两点五秒
    LED_G_OFF();                                       // 关闭绿色结果指示灯
    LED_R_OFF();                                       // 关闭红色结果指示灯
    Test_ShowHome();                                   // 返回统一自检主页面
}

/* ==================== 六路舵机信号自检 ==================== */
static void Test_RunServos(void)
{
    uint8_t Channel;                                    // 保存当前正在测试的舵机通道号

    Motor_AllOff();                                    // 先将与TIM8共享引脚的电机驱动方向脚全部关闭
    PortTest_DisableUSART1();                          // 关闭与S4和S5共享PA9和PA10的USART1
    ServoTest_Init();                                  // 将TIM8和TIM1切换为六路50Hz舵机模式
    ServoTest_SetAll(90U);                             // 让全部已连接舵机先回到中位
    Delay_ms(800U);                                    // 等待舵机到达中位并稳定供电
    for (Channel = 1U; Channel <= 6U; Channel++)       // 依次测试六个接口以降低电源瞬时负载
    {
        OLED_Clear();                                  // 清除上一舵机的测试页面
        OLED_ShowString(1U, 1U, "SERVO TEST");        // 显示舵机测试标题
        OLED_ShowString(2U, 1U, "CHANNEL:");          // 显示当前通道字段名称
        OLED_ShowNum(2U, 9U, Channel, 1U);             // 显示当前S1至S6通道编号
        OLED_ShowString(3U, 1U, "60>120>90 DEG");     // 显示本通道动作顺序
        ServoTest_SetAngle(Channel, 60U);              // 将当前舵机转到六十度
        Delay_ms(700U);                                // 等待当前舵机完成第一段动作
        ServoTest_SetAngle(Channel, 120U);             // 将当前舵机转到一百二十度
        Delay_ms(700U);                                // 等待当前舵机完成第二段动作
        ServoTest_SetAngle(Channel, 90U);              // 将当前舵机恢复到九十度中位
        Delay_ms(700U);                                // 等待当前舵机完成回中动作
        Serial_Printf("[SERVO S%u] 60 -> 120 -> 90 degrees sent; verify movement visually\r\n", Channel); // 输出当前舵机动作提示
    }
    ServoTest_DeInit();                                // 关闭舵机PWM避免测试后持续堵转耗电
    MotorABCD_Init();                                  // 将共享的TIM8和PC6至PC9恢复为电机模式
    Motor_AllOff();                                    // 恢复电机模式后保持四轮完全停止
    PortTest_InitExternal();                           // 将PA9和PA10恢复为USART1测试端口
    OLED_Clear();                                      // 清除最后一路舵机动作页面
    OLED_ShowString(1U, 1U, "SERVO DONE");            // 提示六路舵机信号测试完成
    OLED_ShowString(2U, 1U, "CHECK 6 MOVES");         // 提醒以实际动作作为通过依据
    OLED_ShowString(3U, 1U, "SIGNAL TEST ONLY");      // 明确程序无法自动判断舵机机械动作
    Delay_ms(2500U);                                   // 保留舵机结论页面两点五秒
    Test_ShowHome();                                   // 返回统一自检主页面
}

/* ==================== 四串口回环自检 ==================== */
static void Test_RunPorts(void)
{
    uint8_t PortResult;                                 // 保存四路串口回环测试位图

    OLED_Clear();                                      // 清除主页面并显示串口接线提示
    OLED_ShowString(1U, 1U, "UART LOOPBACK");         // 显示四串口回环测试标题
    OLED_ShowString(2U, 1U, "SHORT TX TO RX");        // 提示每一路需要短接发送与接收
    OLED_ShowString(3U, 1U, "THEN PRESS K3");         // 提示确认短接后再次按KEY3
    while (Key_GetNum() != 3U)                         // 等待操作者完成回环接线并再次确认
    {
        Delay_ms(10U);                                 // 降低等待期间的按键轮询频率
    }
    PortResult = PortTest_RunLoopback();               // 执行USART1、USART2、UART4和UART5回环测试
    OLED_Clear();                                      // 清除接线提示并显示回环结果
    OLED_ShowString(1U, 1U, "U1 U2 U4 U5");           // 显示四个串口结果列标题
    OLED_ShowString(2U, 1U, (PortResult & PORT_TEST_USART1_OK) != 0U ? "OK" : "NO"); // 显示USART1回环结果
    OLED_ShowString(2U, 4U, (PortResult & PORT_TEST_USART2_OK) != 0U ? "OK" : "NO"); // 显示USART2回环结果
    OLED_ShowString(2U, 7U, (PortResult & PORT_TEST_UART4_OK) != 0U ? "OK" : "NO"); // 显示UART4回环结果
    OLED_ShowString(2U, 10U, (PortResult & PORT_TEST_UART5_OK) != 0U ? "OK" : "NO"); // 显示UART5回环结果
    OLED_ShowString(4U, 1U, PortResult == 0x0FU ? "ALL PASS" : "CHECK WIRING"); // 显示四路综合判定
    Serial_Printf("[UART LOOPBACK] mask=0x%02X (U1=%s U2=%s U4=%s U5=%s)\r\n", PortResult, (PortResult & PORT_TEST_USART1_OK) != 0U ? "PASS" : "FAIL", (PortResult & PORT_TEST_USART2_OK) != 0U ? "PASS" : "FAIL", (PortResult & PORT_TEST_UART4_OK) != 0U ? "PASS" : "FAIL", (PortResult & PORT_TEST_UART5_OK) != 0U ? "PASS" : "FAIL"); // 输出四路串口详细结果
    Delay_ms(3000U);                                   // 保留串口结果页面三秒
    Test_ShowHome();                                   // 返回统一自检主页面
}

/* ==================== 主程序 ==================== */
int main(void)
{
    uint8_t KeyNumber;                                  // 保存本轮扫描到的板载按键编号
    uint8_t SerialCommand;                              // 保存USART2收到的远程测试命令
    uint16_t RefreshCounter = 0U;                       // 保存主页面电池和串口刷新计数

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE); // 开启复用功能时钟以配置调试端口
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 关闭JTAG并保留SWD以释放PB3和PB4
    LED_Init();                                        // 初始化板载红绿蓝三色LED
    Key_Init();                                        // 初始化三个低电平有效按键
    Buzzer_Init();                                     // 初始化高电平有效蜂鸣器并默认关闭
    OLED_Init();                                       // 初始化PA4和PA5软件I2C OLED
    Battery_Init();                                    // 初始化PC4电池分压ADC采样
    Serial_Init();                                     // 初始化USART2调试口为115200-8-N-1
    PortTest_InitExternal();                           // 初始化USART1、UART4和UART5测试端口
    MotorABCD_Init();                                  // 初始化电机输出并建立上电安全状态
    Motor_AllOff();                                    // 强制四个电机保持完全停止
    Test_RunBasic();                                   // 执行LED、蜂鸣器、OLED、电池和IMU基础测试
    Serial_SendString("Commands: 1=motor, 2=servo, 3=uart-loopback\r\n"); // 输出USART2远程测试命令说明

    while (1)                                          // 持续响应按键和USART2测试命令
    {
        KeyNumber = Key_GetNum();                      // 扫描KEY1至KEY3并获取单次键值
        SerialCommand = 0U;                            // 默认本轮没有USART2命令
        (void)Serial_ReadByte(&SerialCommand);         // 尝试读取一个USART2命令字节
        if ((KeyNumber == 1U) || (SerialCommand == (uint8_t)'1')) // 判断是否请求电机与编码器测试
        {
            Test_RunMotors();                          // 运行四电机正反转和编码器自动判定
        }
        else if ((KeyNumber == 2U) || (SerialCommand == (uint8_t)'2')) // 判断是否请求六路舵机测试
        {
            Test_RunServos();                          // 运行六路舵机逐路动作测试
        }
        else if ((KeyNumber == 3U) || (SerialCommand == (uint8_t)'3')) // 判断是否请求四串口回环测试
        {
            Test_RunPorts();                           // 运行四路TX到RX回环测试
        }
        else                                           // 处理当前没有测试命令的待机状态
        {
            RefreshCounter++;                          // 累计主页面周期刷新计数
            if (RefreshCounter >= HOME_REFRESH_LOOPS)  // 判断是否达到约一秒刷新周期
            {
                RefreshCounter = 0U;                   // 清零下一轮页面刷新计数
                Battery_Update();                      // 更新一次电池电压和百分比
                (void)GYRO_ACC_TEMP_GET();             // 更新ICM42670三轴加速度、角速度和温度数据
                Test_ShowHome();                       // 刷新主页面电池与IMU状态
                Serial_Printf("[IDLE] imu=0x%02X bat=%umV acc=%d,%d,%d gyro=%d,%d,%d temp=%d; K1/M K2/S K3/U\r\n", ImuDeviceId, Battery_GetVoltageMv(), (int16_t)Acc_Get.X, (int16_t)Acc_Get.Y, (int16_t)Acc_Get.Z, (int16_t)Gyro_Get.X, (int16_t)Gyro_Get.Y, (int16_t)Gyro_Get.Z, (int16_t)Temp.T); // 输出电池与IMU实时数据心跳
            }
            Delay_ms(10U);                             // 主循环待机十毫秒以降低占用
        }
    }
}
