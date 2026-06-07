/*
 * agv_motor.c
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#include "agv_motor.h"
#include "tim.h"  // htim2 değişkenine (PWM için) erişebilmek için ekliyoruz
#include "agv_telemetry.h"
#include "gpio.h"
#include "agv_motor.h"
#include "tim.h"   // htim2 için gerekli
#include "gpio.h"
#include "agv_telemetry.h"

// --- DC MOTOR KONTROL FONKSİYONLARI ---

void AGV_Motor_Init(void)
{
    // Motor sürüşüne başlamadan önce PWM kanallarını aktif etmeliyiz
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1); // ENA (Motor 1 Hız)
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2); // ENB (Motor 2 Hız)
}

void AGV_Stop(void)
{
    // Hızları sıfırla (PWM Duty Cycle = 0)
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);

    // Tüm yön pinlerini LOW seviyesine çek (L298N Fren Modu)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET); // M1_YON1
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET); // M1_YON2
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET); // M2_YON1
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); // M2_YON2
}

void AGV_Forward(uint16_t speed)
{
    // Motor 1 (Sol) İleri
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);   // M1_YON1 HIGH
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET); // M1_YON2 LOW

    // Motor 2 (Sağ) İleri
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);   // M2_YON1 HIGH
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET); // M2_YON2 LOW

    // Hızı PWM'e yaz
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed);
}

void AGV_Backward(uint16_t speed)
{
    // Motor 1 (Sol) Geri (Pinler Tersleniyor)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);

    // Motor 2 (Sağ) Geri (Pinler Tersleniyor)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed);
}

// Nokta Dönüşü (Tank Dönüşü) - Biri İleri, Biri Geri
void AGV_TurnLeft(uint16_t speed)
{
    // Motor 1 Geri
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);

    // Motor 2 İleri
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed);
}

void AGV_TurnRight(uint16_t speed)
{
    // Motor 1 İleri
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

    // Motor 2 Geri
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed);
}

void StepMotor_Drive(int16_t steps)
{
    if (steps == 0) return;

    // 1. Yön Belirleme (DIR Pini -> PB13)
    if (steps > 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);   // İleri
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET); // Geri
        steps = -steps; // Döngü için mutlak değere çevir
    }

    // 2. Adım Sinyali Üretme (STEP Pini -> PB12)
    for(int i = 0; i < steps; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);

        // Pulse genişliği beklemesi
        for(volatile int j=0; j<2000; j++);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);

        // İki adım arası bekleme
        for(volatile int j=0; j<2000; j++);

        // Adım sayacını (telemetri) yeni DIR pinine (PB13) göre güncelle
        if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_SET) agv_packet.step_pos++;
        else agv_packet.step_pos--;
    }
}
