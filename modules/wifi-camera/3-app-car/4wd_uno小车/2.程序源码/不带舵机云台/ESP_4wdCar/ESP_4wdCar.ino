#include <stdio.h>
#include "esp32_wifi.hpp"
#include "motor_car.h"

extern uint8_t cmd_flag;
void setup()
{
  serial_init();
  SET_AP_WIFI();//设置热点的名称和密码 "esp32_wifi.cpp里定义"
  SET_ESP_WIFI_MODE();//设置模式
  motor_car_init();

  cmd_flag = 2;//进入透传模式
}



void loop()
{
  recv_data();
}
