#include "WS2812.h"
#include "Delay.h"
#define WS2812_PORT GPIOA
#define WS2812_PIN GPIO_Pin_11
#define WS2812_TIMER TIM1
#define WS2812_DMA DMA1_Channel4
#define WS2812_DMA_FLAG DMA1_FLAG_TC4


static uint16_t Ws2812Data[WS2812_DATA_LENGTH];
static WS2812ModeTypeDef Ws2812Mode = WS2812_MODE_WATER;
static uint8_t Ws2812WaterIndex, Ws2812FlashState;
static int8_t Ws2812WaterDirection = 1;
static uint32_t Ws2812LastFrameMs;
static void WS2812_ClearData(void) { uint16_t i; for (i = 0U; i < WS2812_DATA_LENGTH; i++) Ws2812Data[i] = WS2812_DATA_0; }
static void WS2812_FillColor(uint8_t G, uint8_t R, uint8_t B, uint8_t Position) {
    uint8_t bit; uint16_t start;
    if (Position >= WS2812_LED_COUNT) return;
    start = (uint16_t)Position * WS2812_LED_BIT;
    for (bit = 0U; bit < 8U; bit++) {
        Ws2812Data[start + bit] = ((G << bit) & 0x80U) != 0U ? WS2812_DATA_1 : WS2812_DATA_0;
        Ws2812Data[start + 8U + bit] = ((R << bit) & 0x80U) != 0U ? WS2812_DATA_1 : WS2812_DATA_0;
        Ws2812Data[start + 16U + bit] = ((B << bit) & 0x80U) != 0U ? WS2812_DATA_1 : WS2812_DATA_0;
    }
}
static void WS2812_Show(void) {
    DMA_Cmd(WS2812_DMA, DISABLE); DMA_ClearFlag(WS2812_DMA_FLAG);
    DMA_SetCurrDataCounter(WS2812_DMA, WS2812_DATA_LENGTH); DMA_Cmd(WS2812_DMA, ENABLE); TIM_Cmd(WS2812_TIMER, ENABLE);
    while (DMA_GetFlagStatus(WS2812_DMA_FLAG) == RESET) { }
    TIM_Cmd(WS2812_TIMER, DISABLE); DMA_Cmd(WS2812_DMA, DISABLE); DMA_ClearFlag(WS2812_DMA_FLAG); GPIO_ResetBits(WS2812_PORT, WS2812_PIN); Delay_us(80U);
}
void WS2812_Init(void) {
    GPIO_InitTypeDef gpio; TIM_TimeBaseInitTypeDef timer; TIM_OCInitTypeDef oc; DMA_InitTypeDef dma;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1, ENABLE); RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    gpio.GPIO_Pin = WS2812_PIN; gpio.GPIO_Mode = GPIO_Mode_AF_PP; gpio.GPIO_Speed = GPIO_Speed_50MHz; GPIO_Init(WS2812_PORT, &gpio);
    TIM_TimeBaseStructInit(&timer); timer.TIM_Period = (uint16_t)(SystemCoreClock / 800000U - 1U); timer.TIM_CounterMode = TIM_CounterMode_Up; TIM_TimeBaseInit(WS2812_TIMER, &timer);
    TIM_OCStructInit(&oc); oc.TIM_OCMode = TIM_OCMode_PWM1; oc.TIM_OutputState = TIM_OutputState_Enable; oc.TIM_OCPolarity = TIM_OCPolarity_High; TIM_OC4Init(WS2812_TIMER, &oc); TIM_OC4PreloadConfig(WS2812_TIMER, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(WS2812_TIMER, ENABLE); TIM_CtrlPWMOutputs(WS2812_TIMER, ENABLE); TIM_DMACmd(WS2812_TIMER, TIM_DMA_CC4, ENABLE);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&WS2812_TIMER->CCR4; dma.DMA_MemoryBaseAddr = (uint32_t)Ws2812Data; dma.DMA_DIR = DMA_DIR_PeripheralDST; dma.DMA_BufferSize = WS2812_DATA_LENGTH; dma.DMA_PeripheralInc = DMA_PeripheralInc_Disable; dma.DMA_MemoryInc = DMA_MemoryInc_Enable; dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord; dma.DMA_Mode = DMA_Mode_Normal; dma.DMA_Priority = DMA_Priority_High; dma.DMA_M2M = DMA_M2M_Disable; DMA_Init(WS2812_DMA, &dma);
    TIM_Cmd(WS2812_TIMER, DISABLE); DMA_Cmd(WS2812_DMA, DISABLE); WS2812_ClearData(); Ws2812Mode = WS2812_MODE_WATER; Ws2812WaterIndex = 0U; Ws2812WaterDirection = 1; Ws2812LastFrameMs = 0UL; WS2812_Show();
}
void WS2812_SetMode(WS2812ModeTypeDef NewMode) { Ws2812Mode = NewMode; Ws2812WaterIndex = 0U; Ws2812WaterDirection = 1; Ws2812FlashState = 0U; Ws2812LastFrameMs = 0UL; if (NewMode == WS2812_MODE_OFF) { WS2812_ClearData(); WS2812_Show(); } }
WS2812ModeTypeDef WS2812_GetMode(void) { return Ws2812Mode; }
void WS2812_Task(uint32_t NowMs) {
    uint8_t i; if (Ws2812Mode == WS2812_MODE_OFF || (uint32_t)(NowMs - Ws2812LastFrameMs) < 100UL) return; Ws2812LastFrameMs = NowMs; WS2812_ClearData();
    if (Ws2812Mode == WS2812_MODE_WATER) { WS2812_FillColor(0U, 180U, 40U, Ws2812WaterIndex); if (Ws2812WaterIndex == 0U) Ws2812WaterDirection = 1; else if (Ws2812WaterIndex == WS2812_LED_COUNT - 1U) Ws2812WaterDirection = -1; Ws2812WaterIndex = (uint8_t)((int16_t)Ws2812WaterIndex + Ws2812WaterDirection); }
    else { Ws2812FlashState ^= 1U; if (Ws2812FlashState != 0U) for (i = 0U; i < WS2812_LED_COUNT; i++) WS2812_FillColor(0U, 255U, 255U, i); }
    WS2812_Show();
}
