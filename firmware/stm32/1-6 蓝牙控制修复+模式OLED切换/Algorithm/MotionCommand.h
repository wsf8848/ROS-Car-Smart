#ifndef __MOTION_COMMAND_H                       //防止标准化运动指令头文件被重复包含
#define __MOTION_COMMAND_H                       //定义标准化运动指令头文件包含保护宏

#include <stdint.h>                              //提供可移植的固定宽度整数类型

typedef struct                                   //定义与具体电机驱动无关的标准化运动指令
{                                                //开始标准化运动指令结构体定义
    int16_t LinearXPermille;                     //保存前后方向速度指令千分比，正值表示前进
    int16_t LinearYPermille;                     //保存左右方向速度指令千分比，正值表示左移
    int16_t AngularZPermille;                    //保存车体旋转速度指令千分比，正值表示左转
    uint8_t EmergencyStop;                       //保存紧急停车标志，非零时电机适配层必须立即停车
} MotionCommandTypeDef;                         //结束标准化运动指令结构体类型定义

#endif                                           //结束标准化运动指令头文件包含保护
