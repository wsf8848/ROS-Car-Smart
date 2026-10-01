#ifndef _WS2812_H
#define _WS2812_H
#include "stm32f10x.h"

#ifndef _USE_TIMER_CLK
#define _USE_TIMER_CLK
#endif

#define			WS2812_GPIO_PORT			GPIOB			
#define			WS2812_GPIO_CLK				RCC_APB2Periph_GPIOB	
#define			WS2812_GPIO_PIN				GPIO_Pin_0
#define			WS2812_TIM					TIM3				
#define			WS2812_Freq					800				
#define         WS2812_RESET_TIME  280
#define         LED_NUMS  60
#define         WS2812_LED_BIT 24

#define         WS2812_LED_DATA_0	 30
#define         WS2812_LED_DATA_1  60
#define 		WS2812_RST_NUM	50
#define 		WS2812_LED_DATA_LEN (WS2812_LED_BIT * LED_NUMS + WS2812_RST_NUM )

#define 		WS2812_LED_HIGH()  GPIO_SetBits(WS2812_GPIO_PORT,WS2812_GPIO_PIN)
#define 		WS2812_LED_LOW()  GPIO_ResetBits(WS2812_GPIO_PORT,WS2812_GPIO_PIN)
enum {
	POWER_OFF = 0,
	POWER_ON
};

enum {
	GREEN_DATA_POS = 0,
	RED_DATA_POS,
	BLUE_DATA_POS
};

enum {
	WS2812_LED_LEVEL_0 = 0,
	WS2812_LED_LEVEL_1,
	WS2812_LED_LEVEL_2,
	WS2812_LED_LEVEL_3,
	WS2812_LED_LEVEL_4,
	WS2812_LED_LEVEL_5,
	WS2812_LED_LEVEL_6,
	WS2812_LED_LEVEL_7,
	WS2812_LED_LEVEL_8,
	WS2812_LED_LEVEL_9,
	WS2812_LED_LEVEL_10,
	WS2812_LED_LEVEL_11,
	WS2812_LED_LEVEL_12,
	WS2812_LED_LEVEL_13,
	WS2812_LED_LEVEL_14,
	WS2812_LED_LEVEL_15,
	WS2812_LED_LEVEL
};

typedef struct
{
	uint8_t G;
	uint8_t R;
	uint8_t B;
	uint8_t phase;
}WS2812_LED_CTRL;

	
void Change_WS2812LED_Status(void);
void Fill_WS2812_LED_Data(uint8_t G,uint8_t R,uint8_t B,uint16_t Pos);


#endif
