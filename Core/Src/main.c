/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "agv_motor.h"
#include "agv_sensor.h"
#include "agv_telemetry.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void RGB_Sensor_Init(void);
void RGB_Sensor_Read(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include "usbd_cdc_if.h"

// USB CDC buffer'larını kullan (usbd_cdc_if.c'den)
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

uint32_t last_blink_time = 0;
uint32_t last_telemetry_time = 0;
/* USER CODE END 0 */


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
  agv_packet.header[0] = 0xAA;
  agv_packet.header[1] = 0x55;
  agv_packet.msg_id = 0x02;
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);

  RGB_Sensor_Init();
  HAL_Delay(200);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
      // --- 1. RUN LED (PC13) ---
      if (HAL_GetTick() - last_blink_time >= 500) {
          HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
          last_blink_time = HAL_GetTick();
      }

      // --- 2. USB CDC KOMUT DİNLEYİCİSİ ---
      // UserRxBufferFS'deki komutları kontrol et
      
      // Heartbeat Sorgusu (FF 01)
      if (UserRxBufferFS[0] == 0xFF && UserRxBufferFS[1] == 0x01) {
          uint8_t hb_response[4] = {0xAA, 0x55, 0x01, 0x01};
          CDC_Transmit_FS(hb_response, 4);
          memset(UserRxBufferFS, 0, 2);  // Buffer'ı temizle
      }
      // Motor Komutları Paketi (AA 55)
      else if (UserRxBufferFS[0] == 0xAA && UserRxBufferFS[1] == 0x55) {
          // Yükü oku: [AA 55 ID L_Byte H_Byte Checksum]
          uint8_t msg_id = UserRxBufferFS[2];
          int16_t value = (UserRxBufferFS[4] << 8) | UserRxBufferFS[3];

          if (msg_id == 0x03) {  // DC Motor Hız
              agv_packet.dc_speed = value;
              if(value > 0) AGV_Forward(value);
              else if(value < 0) AGV_Backward(-value);
              else AGV_Stop();
          }
          else if (msg_id == 0x05) {  // DC Motor Tank Dönüşü
              if(value > 0) AGV_TurnRight(value);
              else if(value < 0) AGV_TurnLeft(-value);
              else AGV_Stop();
          }
          else if (msg_id == 0x04) {  // Step Motor
              StepMotor_Drive(value);
          }
          
          memset(UserRxBufferFS, 0, 6);  // Buffer'ı temizle
      }

      // --- 3. SENSÖR OKUMALARI ---

      // A. ADC Okumaları (Çizgi İzleyen Sensörler)
      ADC_ChannelConfTypeDef sConfig = {0};

      // IR Sol
      sConfig.Channel = ADC_CHANNEL_4;
      sConfig.Rank = 1;
      sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
      HAL_ADC_ConfigChannel(&hadc1, &sConfig);
      HAL_ADC_Start(&hadc1);
      if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) agv_packet.ir_left = HAL_ADC_GetValue(&hadc1);
      HAL_ADC_Stop(&hadc1);

      // IR Sağ
      sConfig.Channel = ADC_CHANNEL_5;
      sConfig.Rank = 1;
      sConfig.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;
      HAL_ADC_ConfigChannel(&hadc1, &sConfig);
      HAL_ADC_Start(&hadc1);
      if(HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) agv_packet.ir_right = HAL_ADC_GetValue(&hadc1);
      HAL_ADC_Stop(&hadc1);

      // --- B. I2C Okuma ---
      uint8_t current_status = 0;

      // RGB Sensörü (I2C1)
      if (HAL_I2C_IsDeviceReady(&hi2c1, TCS34725_ADDR, 1, 2) == HAL_OK) {
          current_status |= 0x01;
          RGB_Sensor_Read(&agv_packet.color_r, &agv_packet.color_g, &agv_packet.color_b, &agv_packet.color_c);
      } else {
          agv_packet.color_r = 0; agv_packet.color_g = 0; agv_packet.color_b = 0; agv_packet.color_c = 0;
          RGB_Sensor_Init();
      }

      // Manyetik Açı Sensörü (I2C2)
      if (HAL_I2C_IsDeviceReady(&hi2c2, AS5600_ADDR, 1, 2) == HAL_OK) {
          current_status |= 0x02;
          AS5600_Read_Angle(&agv_packet.mag_angle);
      } else {
          agv_packet.mag_angle = -1.0f;
      }

      agv_packet.i2c_status = current_status;

      // --- 4. TELEMETRİ GÖNDERİMİ (USB CDC üzerinden) ---
      if (HAL_GetTick() - last_telemetry_time >= 100) {
          Send_Telemetry_Binary();
          last_telemetry_time = HAL_GetTick();
      }
  }
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC|RCC_PERIPHCLK_USB;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */



/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
