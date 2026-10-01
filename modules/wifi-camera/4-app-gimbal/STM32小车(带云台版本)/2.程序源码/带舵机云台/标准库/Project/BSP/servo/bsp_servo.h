#ifndef  __BSP_SERVO_H
#define  __BSP_SERVO_H

#include "AllHeader.h"



#define Servo_J1_PORT	GPIOB
#define Servo_J2_PORT	GPIOB
#define Servo_J3_PORT	GPIOD
#define Servo_J4_PORT	GPIOD

#define Servo_J1_PIN	GPIO_Pin_0
#define Servo_J2_PIN	GPIO_Pin_1
#define Servo_J3_PIN	GPIO_Pin_14
#define Servo_J4_PIN	GPIO_Pin_15

#define Servo_J1_CLK	RCC_APB2Periph_GPIOB
#define Servo_J2_CLK	RCC_APB2Periph_GPIOB
#define Servo_J3_CLK  RCC_APB2Periph_GPIOD
#define Servo_J4_CLK	RCC_APB2Periph_GPIOD

#define SERVO_1_HIGH()  PBout(0) = 1
#define SERVO_1_LOW()   PBout(0) = 0

#define SERVO_2_HIGH()  PBout(1) = 1
#define SERVO_2_LOW()   PBout(1) = 0

#define SERVO_3_HIGH()  PDout(14) = 1
#define SERVO_3_LOW()   PDout(14) = 0

#define SERVO_4_HIGH()  PDout(15) = 1
#define SERVO_4_LOW()   PDout(15) = 0

typedef enum _PWMServo_ID
{
    PWMServo_ID_S1 = 0,
    PWMServo_ID_S2,
    PWMServo_ID_S3,
    PWMServo_ID_S4,
    
    MAX_PWM_SERVO
} PWMServo_ID;

void PwmServo_Handle(void);
void pwmServo_init_gpio(void);
uint8_t* PwmServo_Get_Angle(void);
void PwmServo_Set_Angle(uint8_t index, uint8_t angle);
void PwmServo_Set_Angle_All(uint8_t angle_s1, uint8_t angle_s2, uint8_t angle_s3, uint8_t angle_s4);

#endif

