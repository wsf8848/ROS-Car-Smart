#include "stm32f10x.h"                  // Device header
#include "Timer.h"  

void Timer_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);  //务必开启的是APB2的时钟

    TIM_InternalClockConfig(TIM2);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 1000-1;  //ARR
    TIM_TimeBaseInitStructure.TIM_Prescaler = 720-1;  //PSC
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_ClearFlag(TIM2, TIM_FLAG_Update);
    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitTypeDef NVIC_InitStructure; 
    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;  //优先级为2
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0; //子优先级为1
    NVIC_Init(&NVIC_InitStructure);

//    TIM_Cmd(TIM2, ENABLE);
}

