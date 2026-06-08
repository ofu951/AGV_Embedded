/*
 * agv_telemetry.c
 *
 *  Created on: Jun 5, 2026
 *      Author: ofu95
 */

#include "agv_telemetry.h"
#include "usbd_cdc_if.h" // USB CDC gönderme için

AGV_Telemetry_Packet_t agv_packet;
void Send_Telemetry_Binary(void)
{
    // Checksum hesaplama (aynı kalacak)
    uint8_t calc_checksum = 0;
    uint8_t *ptr = (uint8_t*)&agv_packet;

    for(int i = 2; i < sizeof(AGV_Telemetry_Packet_t) - 1; i++) {
        calc_checksum ^= ptr[i];
    }
    agv_packet.checksum = calc_checksum;

    // --- DEĞİŞİKLİK: Artık USB üzerinden gönder ---
    // Eski UART gönderme: HAL_UART_Transmit(&huart1, ...)
    // Yeni USB gönderme:
    CDC_Transmit_FS((uint8_t*)&agv_packet, sizeof(AGV_Telemetry_Packet_t));

    // LED toggle (opsiyonel, kalabilir)
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
}
