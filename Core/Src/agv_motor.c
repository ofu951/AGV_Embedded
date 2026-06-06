/*
 * agv_motor.c
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#include "agv_motor.h"
#include "tim.h"  // htim2 değişkenine (PWM için) erişebilmek için ekliyoruz
#include "agv_telemetry.h"
void AGV_Forward(uint16_t speed)
{
    // Sol Motor İleri (IN1=1, IN2=0)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

    // Sağ Motor İleri (IN3=1, IN4=0) - IN3/IN4 PB0 ve PB1'de
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    // PWM Hızlarını Uygula
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed); // Sol Motor
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed); // Sağ Motor
}

/**
 * @brief AGV'yi geri yönde hareket ettirir.
 * @param speed: 0 ile 999 arası hız değeri
 */
void AGV_Backward(uint16_t speed)
{
    // Sol Motor Geri (IN1=0, IN2=1)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);

    // Sağ Motor Geri (IN3=0, IN4=1)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed);
}

/**
 * @brief AGV'yi olduğu yerde sağa döndürür (Tank Turn).
 * @param speed: 0 ile 999 arası hız değeri
 */
void AGV_TurnRight(uint16_t speed)
{
    // Sol Motor İleri
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

    // Sağ Motor Geri
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed);
}

/**
 * @brief AGV'yi olduğu yerde sola döndürür (Tank Turn).
 * @param speed: 0 ile 999 arası hız değeri
 */
void AGV_TurnLeft(uint16_t speed)
{
    // Sol Motor Geri
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);

    // Sağ Motor İleri
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, speed);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, speed);
}

/**
 * @brief Motorları tamamen durdurur.
 */
void AGV_Stop(void)
{
    // Yön pinlerini sıfırla (Serbest duruş)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);

    // PWM'i sıfırla
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 0);
}

void StepMotor_Drive(int16_t steps)
{
    if (steps == 0) return;

    // 1. Yön Belirleme (DIR Pini - PB11)
    if (steps > 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_SET);   // İleri
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_11, GPIO_PIN_RESET); // Geri
        steps = -steps; // Döngü için mutlak değere çevir
    }

    // 2. Adım Sinyali Üretme (STEP Pini - PB10)
    for(int i = 0; i < steps; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);

        // Pulse genişliği beklemesi (Mikrosaniye seviyesi)
        for(volatile int j=0; j<2000; j++);

        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);

        // İki adım arası bekleme (Hız kontrolü)
        for(volatile int j=0; j<2000; j++);

        // Adım sayacını (telemetri) güncelle
        if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_SET) agv_packet.step_pos++;
        else agv_packet.step_pos--;
    }
}
