#include "bsp.h"


void bsp_init(void)
{
}


//JTAG模式设置,用于设置JTAG的模式
//mode:jtag,swd模式设置;00,全使能;01,使能SWD;10,全关闭;
//#define JTAG_SWD_DISABLE   0X02
//#define SWD_ENABLE         0X01
//#define JTAG_SWD_ENABLE    0X00
void Bsp_JTAG_Set(uint8_t mode)
{
	uint32_t temp;
	temp = mode;
	temp <<= 25;
	RCC->APB2ENR |= 1 << 0;	  //开启辅助时钟
	AFIO->MAPR &= 0XF8FFFFFF; //清除MAPR的[26:24]
	AFIO->MAPR |= temp;		  //设置jtag模式
}
