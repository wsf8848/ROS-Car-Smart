#ifndef __ESP32_WIFI_H
#define __ESP32_WIFI_H


#include <stdio.h>
#include <string.h>
#include <Arduino.h>
#include "motor_car.h"

#ifdef __cplusplus
extern "C" {
#endif


#define MODE_AP 			0 
#define MODE_STA 			0	
#define MODE_AP_STA 	1

void serial_init(void);
void recv_data(void);

void SET_ESP_WIFI_MODE(void);
void SET_STA_WIFI(void);
void SET_AP_WIFI(void);
void Get_STAIP(void);
void Get_APIP(void);
void Get_Version(void);

void Data_Deal(char RXdata);
void IP_identify(char* data);
void recv_tcp_data(char tcpdata);

void Change_motor(void);

#ifdef __cplusplus
}
#endif

#endif

