from machine import UART, Pin, Timer,PWM
from pico_car import pico_car
import time
import binascii

WIFI_MODE = 2 #0:AP 1:STA 2:AP+STA
AP_WIFI_SSID = "ESP32_WIFI_TEST"  #自发热点wifi名称
AP_WIFI_PD =  "" #自发热点wifi密码  -无密码

Motor = pico_car()
Motor.Car_Stop()

Servo = pico_car()

u1 = UART(0, 115200, bits=8, parity=None, stop=1, tx=Pin(16), rx=Pin(17))  # 设置波特率和串口号

rxdata = bytes()

def SET_WIFI_MODE():
    global u1
    if WIFI_MODE == 0:
        u1.write("wifi_mode:0")
    elif WIFI_MODE == 1:
        u1.write("wifi_mode:1")
    elif WIFI_MODE == 2:
        u1.write("wifi_mode:2")
    
    #等待重启
    time.sleep(2)
    

def SET_AP():
    global u1
    u1.write("ap_ssid:"+AP_WIFI_SSID)
    time.sleep(0.05)
    
    u1.write("ap_pd:"+AP_WIFI_PD)
    time.sleep(2)
    
def Ctrol_servo(RXdata):
    if RXdata[1] == 'A': #上下方向舵机 #S1接口
        angle = RXdata[2]+RXdata[3]+RXdata[4]
        angle = int(angle)
        Servo.servo180(1,angle) 

    elif RXdata[1] == "B": #左右方向舵机
        angle = RXdata[2]+RXdata[3]+RXdata[4]
        angle = int(angle)
        angle = 180-angle #因为app控制的方向相反
        Servo.servo180(2,angle) 
        
    
def Cotrol_Car(RXdata):
    if RXdata == "$1,0,0,0#": #前进
        Motor.Car_Run(255,255)
    elif RXdata == "$2,0,0,0#": #后退
        Motor.Car_Back(255,255)
    elif RXdata == "$3,0,0,0#": #左转
        Motor.Car_Run(0,255)
    elif RXdata == "$4,0,0,0#": #右转
        Motor.Car_Run(255,0)
    elif RXdata == "$5,0,0,0#": #左旋
        Motor.Car_Left(255,255)
    elif RXdata == "$6,0,0,0#": #右旋
        Motor.Car_Right(255,255)
    elif RXdata == "$7,0,0,0#": #停止
        Motor.Car_Stop()
    else : #舵机云台控制
        Ctrol_servo(RXdata)





try:
    #设置wifi模式
    SET_WIFI_MODE()
    #设置自发热点
    SET_AP()
    Servo.servo180(1,90) 
    Servo.servo180(2,90) 
    while True:
        if u1.any() > 0:
            rxdata=u1.read()
            rxdata=str(rxdata)
            rxdata = rxdata.replace("b'","") #去掉 b'
            rxdata = rxdata.replace("'","")#去掉 '
            #u1.write(rxdata)
            #rxdata = rxdata.decode('utf-8') #这句离线运行有影响
            Cotrol_Car(rxdata)
        
    
except KeyboardInterrupt:
    Motor.Car_Stop()
except Exception as e:
    Motor.Car_Stop()
    print("Program Error:", Exception)
