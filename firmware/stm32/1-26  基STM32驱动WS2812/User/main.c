#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LED.h"
#include "KEY.h"
#include "OLED.h"
#include "Buzzer.h"  
#include "Timer.h"  
#include "Delay.h" 
#include "WS2812.h" 

uint8_t VLFlag;//声光报警标志位
uint8_t Key_Num = 0;//按键值

// 声明外部函数（WS2812相关）
extern void ws2812_Init(void);
extern void WS2812_LED_PowerOff(void);
extern void WS2812_LED_Circle(void);
extern void Fill_WS2812_LED_Data(uint8_t G, uint8_t R, uint8_t B, uint16_t Pos);

// 绿色闪烁模式变量
static uint8_t greenFlashMode = 0;  // 绿色闪烁模式标志（0：正常模式，1：闪烁模式）
static uint8_t greenState = 0;      // 绿色灯状态（0：灭，1：亮）
static uint32_t flashTick = 0;      // 闪烁计时变量

int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);	
    ws2812_Init();
	OLED_Init();
	LED_Init();
	Key_Init();  
	Buzzer_Init();
	Timer_Init();//10ms定时器
	
	// 显示标题
	OLED_ShowString(1, 1, "HelloWorld!");
	
	Buzzer_start();//系统启动提示音
	TIM_Cmd(TIM2, ENABLE);//开启定时器
	
	
	while (1)
	{
		Key_Num = Key_GetNum(); // 获取按键值
		
		if(Key_Num == 1)
		{
			// 切换绿色闪烁模式
			greenFlashMode = !greenFlashMode;  
			
			// 进入闪烁模式时，清除原有流水灯数据
			if (greenFlashMode)
			{
				for (uint8_t i = 0; i < LED_NUMS; i++)
				{
					Fill_WS2812_LED_Data(0, 0, 0, i);
				}
				Change_WS2812LED_Status();
			}
		}
		// 注意：此处修正了原代码的大括号位置错误，将按键1的逻辑闭合
		else if(Key_Num == 2)
		{
			LED2_Turn();
			VLFlag = 1;
		}
		
		// 绿色闪烁模式逻辑（独立于按键检测，持续运行）
		if (greenFlashMode)
		{
			flashTick++;
			// 每500ms切换一次状态（Delay_ms(100) * 5 = 500ms）
			if (flashTick >= 5)
			{
				flashTick = 0;
				greenState = !greenState;  // 切换亮灭状态

				if (greenState)
				{
					// 所有LED亮绿色（G=255, R=0, B=0，符合WS2812的GRB格式）
					for (uint8_t i = 0; i < LED_NUMS; i++)
					{
						Fill_WS2812_LED_Data(0xFF, 0x00, 0x00, i);
					}
				}
				else
				{
					// 所有LED灭
					for (uint8_t i = 0; i < LED_NUMS; i++)
					{
						Fill_WS2812_LED_Data(0x00, 0x00, 0x00, i);
					}
				}
				Change_WS2812LED_Status();  // 更新LED状态
			}
			Delay_ms(100);
		}
		// 非闪烁模式时，执行原有流水灯逻辑
		else
		{
			WS2812_LED_Circle();
			Delay_ms(100);
		}
	}
}


void TIM2_IRQHandler(void) //10ms中断周期
{
  static uint8_t Count = 0;
  static uint8_t Count2 = 0;

  if (TIM_GetITStatus(TIM2, TIM_IT_Update) != RESET)
  {
    /*声光报警处理*/
    if (VLFlag == 1)
    {
      Buzzer_ON();
      Count++; 
      if (Count >= 20) // 20*10ms=200ms
      {
        Buzzer_OFF();
        Count = 0; 
        VLFlag = 0; 
      }
    }
	//秒闪效果
	Count2++;
	if(Count2 == 90)//900ms
	{
		LED2_Turn();
	}
	if(Count2 >= 100) //1S
	{
		LED2_Turn();
		Count2 = 0;
	}
   
    Led_Flash(10);
    TIM_ClearITPendingBit(TIM2, TIM_IT_Update); //清除中断标志位
  }
}
