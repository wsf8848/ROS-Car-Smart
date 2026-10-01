#include "stm32f10x.h"                  // Device header
#include "Pid.h"         

/***********************************增量式PI速度环******************************************/

//左
float Velcity_Kp = 80,  Velcity_Ki = 9; //左轮PID参数

int Left_FeedbackControl(int CurrentVelocity,int TargetVelocity) //依传入实际速度和目标速度
{
  int Bias;  //定义相关变量,存储误差值
  static int ControlVelocity, Last_bias; //静态变量，函数调用结束后其值依然存在!!

  Bias=TargetVelocity-CurrentVelocity; //求速度偏差

  ControlVelocity+=Velcity_Kp*(Bias-Last_bias)+Velcity_Ki*Bias;  //增量式PI控制器
  //Velcity_Kp*(Bias-Last_bias) 作用为限制加速度
  //Velcity_Ki*Bias             速度控制值由Bias不断积分得到 偏差越大加速度越大
	
  Last_bias=Bias;//更新误差
  return ControlVelocity; //返回速度控制值
}