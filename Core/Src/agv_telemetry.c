/*
 * agv_telemetry.c
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#include "agv_telemetry.h"
#include "usart.h" // huart1 için gerekli

AGV_Telemetry_Packet_t agv_packet;
void Send_Telemetry_Binary(void)
{
    // Checksum hesaplama...
    uint8_t calc_checksum = 0;
    uint8_t *ptr = (uint8_t*)&agv_packet;

    for(int i = 2; i < sizeof(AGV_Telemetry_Packet_t) - 1; i++) {
        calc_checksum ^= ptr[i];
    }
    agv_packet.checksum = calc_checksum;

    // Struct'ın bellekteki ham halini doğrudan UART'a bas
    HAL_UART_Transmit(&huart1, (uint8_t*)&agv_packet, sizeof(AGV_Telemetry_Packet_t), 100);

    // --- YENİ EKLENEN KISIM: Veri Akış Monitörü ---
    // Her paket gittiğinde LED durum değiştirir. (Titreşim efekti)
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}
