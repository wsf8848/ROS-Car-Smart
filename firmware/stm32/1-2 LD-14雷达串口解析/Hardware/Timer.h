#ifndef __TIMER_H                       //防止定时器头文件被重复包含
#define __TIMER_H                       //定义定时器头文件包含保护宏

void Timer_Init(void);                  //初始化TIM2为10ms周期更新中断并等待主函数启动

#endif                                  //结束定时器头文件包含保护
