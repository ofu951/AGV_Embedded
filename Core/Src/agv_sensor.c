/*
 * agv_sensor.c
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#include "agv_sensor.h"
#include "i2c.h"       // hi2c1 için gerekli
#include "gpio.h"      // Çizgi sensörlerini okumak için gerekli
#include "agv_motor.h" // AGV_FollowLine içinde motorları sürmek için gerekli
#include "agv_telemetry.h"
/**
 * @brief RGB Sensörünü Uyandırır
 */
void RGB_Sensor_Init(void)
{
    uint8_t enable_data = 0x03; // PON ve AEN bitlerini 1 yap
    HAL_I2C_Mem_Write(&hi2c1, TCS34725_ADDR, (TCS34725_COMMAND_BIT | TCS34725_ENABLE), 1, &enable_data, 1, 100);
    HAL_Delay(50);
}

/**
 * @brief Sensörden R, G, B ve Clear verilerini okur (Hata Toleranslı)
 */
/**
 * @brief Sensörden R, G, B ve Clear verilerini okur (Hata Toleranslı)
 */
void RGB_Sensor_Read(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c)
{
    uint8_t buffer[8];

    // DİKKAT: '1' yerine 'I2C_MEMADD_SIZE_8BIT' makrosu eklendi
    if(HAL_I2C_Mem_Read(&hi2c1, TCS34725_ADDR, (TCS34725_COMMAND_BIT | TCS34725_CDATAL), I2C_MEMADD_SIZE_8BIT, buffer, 8, 10) == HAL_OK)
    {
        *c = (buffer[1] << 8) | buffer[0];
        *r = (buffer[3] << 8) | buffer[2];
        *g = (buffer[5] << 8) | buffer[4];
        *b = (buffer[7] << 8) | buffer[6];
    }
    else
    {
        *c = 0; *r = 0; *g = 0; *b = 0;
        uint8_t enable_data = 0x03;
        // DİKKAT: '1' yerine 'I2C_MEMADD_SIZE_8BIT' eklendi
        HAL_I2C_Mem_Write(&hi2c1, TCS34725_ADDR, (TCS34725_COMMAND_BIT | TCS34725_ENABLE), I2C_MEMADD_SIZE_8BIT, &enable_data, 1, 10);
    }
}

/**
 * @brief AS5600 Manyetik Açı Sensöründen Derece cinsinden okuma yapar
 */


// ============================================================
// AS5600 - Düzeltilmiş versiyon
// ============================================================
/**
 * @brief AS5600 Manyetik Açı Sensöründen I2C2 üzerinden okuma yapar
 */
void AS5600_Read_Angle(float *angle_deg)
{
    uint8_t buffer[2] = {0, 0};

    // DİKKAT: &hi2c1 yerine &hi2c2 kullanıyoruz
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(
        &hi2c2,                 // <--- I2C2 oldu
        AS5600_ADDR,
        AS5600_RAW_ANGLE_REG,
        I2C_MEMADD_SIZE_8BIT,
        buffer,
        2,
        20
    );

    if (status == HAL_OK)
    {
        uint16_t raw_angle = (buffer[0] << 8) | buffer[1];
        *angle_deg = (float)(raw_angle) * 360.0f / 4096.0f;
    }
    else
    {
        *angle_deg = -1.0f;  // Okuma hatası durumunda arayüze -1 gönder
    }
}
/**
 * @brief IR Çizgi İzleme Test Fonksiyonu
 */
void Test_LineSensors(void)
{
    uint16_t left_val = agv_packet.ir_left;
   // uint16_t right_val = agv_packet.ir_right;

    if(left_val < 200) {
        AGV_Stop();
    }
}

/**
 * @brief Basit Şerit Takip (Line Follower) Algoritması
 */
void AGV_FollowLine(void)
{
    uint16_t left_val = agv_packet.ir_left;
    uint16_t right_val = agv_packet.ir_right;

    uint16_t threshold = 2000; // Siyah/Beyaz ayrımı için eşik değeri (kendi zeminine göre ayarlarsın)

    // Durum 1: İki sensör de beyazda (Değerler eşikten düşük). Çizgi ortada.
    if(left_val < threshold && right_val < threshold) {
        AGV_Forward(600);
    }
    // Durum 2: Sol sensör siyah gördü (Değer eşikten yüksek). Sola düzelt.
    else if(left_val > threshold && right_val < threshold) {
        AGV_TurnLeft(400);
    }
    // Durum 3: Sağ sensör siyah gördü. Sağa düzelt.
    else if(left_val < threshold && right_val > threshold) {
        AGV_TurnRight(400);
    }
    // Durum 4: İkisi de siyah gördü (Kavşak veya durma noktası).
    else if(left_val > threshold && right_val > threshold) {
        AGV_Stop();
    }
}
