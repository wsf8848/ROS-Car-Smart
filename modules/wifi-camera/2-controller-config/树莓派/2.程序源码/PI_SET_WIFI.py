#!/usr/bin/python
# -*- coding:utf-8 -*-
import serial
import time

#打开串口
ser = serial.Serial("/dev/ttyAMA0",115200,8,'N',1,timeout = 0.5) #树莓派
print("serial start ...")

Sta_wifi_ssid = "Yahboom"       #sta的wifi名称
Sta_wifi_pd = "yahboom"         #sta的wifi名称

AP_wifi_ssid = "ESP_WIFI_TEST"  #ap的WiFi名称
AP_wifi_pd = ""                 #ap的wifi密码

wifi_mode = '2'  #wifi模式选择  '0':AP模式  '1':STA模式  '2':AP+STA模式

#sta模式配置
def SET_STA_WIFI():
    global ser
    print("set_sta_wifi")
    #发送wifi名称
    STA_buf = "sta_ssid:"+Sta_wifi_ssid
    ser.write(bytes(STA_buf,'utf-8'))
    time.sleep(0.1) #100ms

    STA_buf = "sta_pd:"+Sta_wifi_pd
    ser.write(bytes(STA_buf,'utf-8'))
    time.sleep(2) #因为模块会自动重启

#ap模式配置
def SET_AP_WIFI():
    global ser
    print("set_ap_wifi")
    #发送wifi名称
    AP_buf = "ap_ssid:"+AP_wifi_ssid
    ser.write(bytes(AP_buf,'utf-8'))
    time.sleep(0.1) #100ms

    #发送wifi密码
    AP_buf = "ap_pd:"+AP_wifi_pd
    ser.write(bytes(AP_buf,'utf-8'))
    time.sleep(2) #因为模块会自动重启

#wifi模式选择
def SET_WIFI_MODE():
    global ser
    print("set_wifi_mode")
    #发送wifi名称
    MODE_buf = "wifi_mode:"+ wifi_mode
    ser.write(bytes(MODE_buf,'utf-8'))
    time.sleep(2) #因为模块会自动重启


#获取STA的ip
def GET_STA_IP():
    global ser
    ser.flushInput() #先清空缓存
    IP_buf = "sta_ip"
    ser.write(bytes(IP_buf,'utf-8'))
    #接收
    recv = ser.readline()
    recv = recv.decode('utf-8')
    print(recv)
    


#获取AP的ip
def GET_AP_IP():
    global ser
    
    ser.flushInput() #先清空缓存
    IP_buf = "ap_ip"
    ser.write(bytes(IP_buf,'utf-8'))
    #接收
    recv = ser.readline()
    recv = recv.decode('utf-8')
    print(recv)


#获取版本信息
def GET_ESP_VERSION():
    global ser
    ser.flushInput() #先清空缓存
    IP_buf = "wifi_ver"
    ser.write(bytes(IP_buf,'utf-8'))
    #接收
    recv = ser.readline()
    recv = recv.decode('utf-8')
    print(recv)



try:
    
    SET_WIFI_MODE() #模式配置
    time.sleep(2) #等待重启
    
    if wifi_mode != '0':  #'0':只开AP
        SET_STA_WIFI() #设置STA的wifi
        time.sleep(2)

    if wifi_mode != '1':
        SET_AP_WIFI() #设置AP的wifi
        time.sleep(2)

    GET_ESP_VERSION()
    time.sleep(0.5)

    #获取AP的ip地址 3次
    for i in range(3):
        GET_AP_IP()
        time.sleep(0.5)

    #获取STA的ip地址 3次
    for i in range(3):
        GET_STA_IP()
        time.sleep(0.5)
    


    ser.flushInput() #先清空缓存
    while True:
        data = b'' #透传的一包数据缓存
        count = ser.inWaiting() #获取缓存
        while count :
            data += ser.read(count)
            time.sleep(0.1)
            count = ser.inWaiting() #获取缓存
        if data:
            print(data.decode('utf-8'))#打印数据到终端

        
    

except KeyboardInterrupt:
    if ser != None:
        ser.close()
