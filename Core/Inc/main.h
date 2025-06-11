/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_Pin GPIO_PIN_13
#define LED_GPIO_Port GPIOC
#define IR4_Pin GPIO_PIN_0
#define IR4_GPIO_Port GPIOA
#define IR1_Pin GPIO_PIN_1
#define IR1_GPIO_Port GPIOA
#define IR2_Pin GPIO_PIN_2
#define IR2_GPIO_Port GPIOA
#define IR3_Pin GPIO_PIN_3
#define IR3_GPIO_Port GPIOA
#define IR5_Pin GPIO_PIN_4
#define IR5_GPIO_Port GPIOA
#define IR6_Pin GPIO_PIN_5
#define IR6_GPIO_Port GPIOA
#define IR7_Pin GPIO_PIN_6
#define IR7_GPIO_Port GPIOA
#define IR8_Pin GPIO_PIN_7
#define IR8_GPIO_Port GPIOA
#define EncoderSW_Pin GPIO_PIN_0
#define EncoderSW_GPIO_Port GPIOB
#define User_BTN_Pin GPIO_PIN_1
#define User_BTN_GPIO_Port GPIOB
#define MOTORR_F_Pin GPIO_PIN_10
#define MOTORR_F_GPIO_Port GPIOB
#define MOTORR_B_Pin GPIO_PIN_11
#define MOTORR_B_GPIO_Port GPIOB
#define nRF24_CSN_Pin GPIO_PIN_12
#define nRF24_CSN_GPIO_Port GPIOB
#define ESP_ENABLE_Pin GPIO_PIN_8
#define ESP_ENABLE_GPIO_Port GPIOA
#define MOTORL_F_Pin GPIO_PIN_15
#define MOTORL_F_GPIO_Port GPIOA
#define MOTORL_B_Pin GPIO_PIN_3
#define MOTORL_B_GPIO_Port GPIOB
#define EncoderA_Pin GPIO_PIN_6
#define EncoderA_GPIO_Port GPIOB
#define EncoderB_Pin GPIO_PIN_7
#define EncoderB_GPIO_Port GPIOB
#define I2C_SCL_Pin GPIO_PIN_8
#define I2C_SCL_GPIO_Port GPIOB
#define I2C_SDA_Pin GPIO_PIN_9
#define I2C_SDA_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
