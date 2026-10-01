#ifndef __MOTOR_CAR_H
#define __MOTOR_CAR_H

#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include "esp32_wifi.hpp"

#ifdef __cplusplus
extern "C" {
#endif


void motor_car_init(void);
void run(uint8_t speed);
void brake(void);
void left(uint8_t speed);
void right(uint8_t speed);
void spin_left(uint8_t speed);
void spin_right(uint8_t speed);
void back(uint8_t speed);



#ifdef __cplusplus
}
#endif

#endif
