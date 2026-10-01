#ifndef __SERVO_CAR_H
#define __SERVO_CAR_H


#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include <Servo.h>
#include "esp32_wifi.hpp"

extern Servo servo_vflip;
extern Servo servo_mirror;

#ifdef __cplusplus
extern "C" {
#endif

void init_servo(void);




#ifdef __cplusplus
}
#endif

#endif
