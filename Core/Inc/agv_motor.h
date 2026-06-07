/*
 * agv_motor.h
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#ifndef AGV_MOTOR_H
#define AGV_MOTOR_H

#include "main.h"

void AGV_Forward(uint16_t speed);
void AGV_Backward(uint16_t speed);
void AGV_TurnRight(uint16_t speed);
void AGV_TurnLeft(uint16_t speed);
void AGV_Stop(void);
void StepMotor_Drive(int16_t steps);

#endif /* AGV_MOTOR_H */
