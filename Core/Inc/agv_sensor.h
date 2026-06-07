/*
 * agv_sensor.h
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#ifndef AGV_SENSOR_H
#define AGV_SENSOR_H

#include "main.h"

// --- Sensör Adresleri ve Register Tanımları ---
// Renk Sensörü (TCS34725)
#define TCS34725_ADDR         (0x29 << 1)
#define TCS34725_COMMAND_BIT  0x80
#define TCS34725_ENABLE       0x00
#define TCS34725_CDATAL       0x14

// Manyetik Açı Sensörü (AS5600)
#define AS5600_ADDR           (0x40 << 1)
#define AS5600_RAW_ANGLE_REG  0x0C

// --- Fonksiyon Prototipleri ---
void RGB_Sensor_Init(void);
void RGB_Sensor_Read(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);
void AS5600_Read_Angle(float *angle_deg);
void Test_LineSensors(void);
void AGV_FollowLine(void);

#endif /* AGV_SENSOR_H */
