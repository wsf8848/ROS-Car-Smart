#ifndef __WS2812_H__
#define __WS2812_H__
#include "stm32f10x.h"
#define WS2812_LED_COUNT 8U
#define WS2812_LED_BIT 24U
#define WS2812_RESET_SLOTS 50U
#define WS2812_DATA_LENGTH (WS2812_LED_COUNT * WS2812_LED_BIT + WS2812_RESET_SLOTS)
#define WS2812_DATA_0 30U
#define WS2812_DATA_1 60U
typedef enum { WS2812_MODE_OFF = 0U, WS2812_MODE_WATER = 1U, WS2812_MODE_FLASH = 2U } WS2812ModeTypeDef;
void WS2812_Init(void);
void WS2812_SetMode(WS2812ModeTypeDef Mode);
WS2812ModeTypeDef WS2812_GetMode(void);
void WS2812_Task(uint32_t NowMs);
#endif
