/*
 * agv_telemetry.h
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#ifndef AGV_TELEMETRY_H
#define AGV_TELEMETRY_H

#include "main.h"

// 1. Struct Tanımı
#pragma pack(push, 1)
typedef struct {
    uint8_t  header[2];
    uint8_t  msg_id;
    uint16_t ir_left;
    uint16_t ir_right;
    uint16_t color_r;
    uint16_t color_g;
    uint16_t color_b;
    uint16_t color_c;
    float    mag_angle;
    int16_t  dc_speed;
    int32_t  step_pos;
    uint8_t  i2c_status;
    uint8_t  checksum;
} AGV_Telemetry_Packet_t;
#pragma pack(pop)

// 2. EXTERN KULLANIMI: "Bu değişken başka bir .c dosyasında var, haberdar ol" diyoruz.
extern AGV_Telemetry_Packet_t agv_packet;

// Fonksiyon prototipi
void Send_Telemetry_Binary(void);

#endif
