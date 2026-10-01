#ifndef __WS2812_H__                                  // 防止WS2812驱动头文件被重复包含
#define __WS2812_H__                                  // 定义WS2812驱动头文件包含保护宏

#include "stm32f10x.h"                               // 提供STM32F103RCT6外设定义和固定宽度整数类型

#define WS2812_LED_COUNT 8U                            // 定义车辆灯带实际串联的八颗WS2812灯珠
#define WS2812_BITS_PER_LED 24U                        // 定义每颗灯珠使用G、R、B各八位共二十四位数据
#define WS2812_RESET_SLOTS 50U                         // 定义DMA帧尾追加五十个零占空比复位码元
#define WS2812_DATA_LENGTH (WS2812_LED_COUNT * WS2812_BITS_PER_LED + WS2812_RESET_SLOTS) // 定义完整DMA发送缓冲区长度
#define WS2812_PWM_DUTY_0 30U                          // 定义逻辑零高电平约占九十计数周期的三十计数
#define WS2812_PWM_DUTY_1 60U                          // 定义逻辑一高电平约占九十计数周期的六十计数

typedef enum                                           // 定义车辆灯带支持的工作模式
{
    WS2812_MODE_OFF = 0U,                              // 关闭全部灯珠并保持灯带熄灭
    WS2812_MODE_WATER = 1U,                            // 运行彩色流水灯效果
    WS2812_MODE_FLASH = 2U                             // 运行全灯绿色闪烁效果
} WS2812ModeTypeDef;                                   // 结束WS2812灯效模式类型定义

void WS2812_Init(void);                                // 初始化PA11、TIM1通道4和DMA1通道4，默认选择流水灯
void WS2812_SetMode(WS2812ModeTypeDef Mode);           // 切换关闭、流水或闪烁灯效模式
WS2812ModeTypeDef WS2812_GetMode(void);                // 读取当前灯效模式
void WS2812_Task(uint32_t NowMs);                      // 按一百毫秒周期非阻塞更新灯带动画帧

#endif                                                 // 结束WS2812驱动头文件包含保护
