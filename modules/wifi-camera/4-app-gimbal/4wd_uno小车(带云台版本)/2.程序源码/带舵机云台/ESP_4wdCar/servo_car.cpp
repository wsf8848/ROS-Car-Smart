#include "servo_car.hpp"
#include <Servo.h>

Servo servo_vflip;
Servo servo_mirror;

//初始化舵机
void init_servo(void)
{
    servo_vflip.attach(3);
    servo_mirror.attach(15);
}
