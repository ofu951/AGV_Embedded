/*
 * agv_motor.h
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#ifndef INC_AGV_MOTOR_H_
#define INC_AGV_MOTOR_H_

#include "stm32f1xx_hal.h"


void AGV_Motor_Init(void);
void AGV_Stop(void);
void AGV_Forward(uint16_t speed);
void AGV_Backward(uint16_t speed);
void AGV_TurnLeft(uint16_t speed);
void AGV_TurnRight(uint16_t speed);

// Step Motor Kontrolü
void StepMotor_Drive(int16_t steps);

#endif /* INC_AGV_MOTOR_H_ */
