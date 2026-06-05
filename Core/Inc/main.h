/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void ativa_alarme(void);
void btn_relogio_processado(void);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PC1_PIN_INPUT_MEMBRANA_01_Pin GPIO_PIN_1
#define PC1_PIN_INPUT_MEMBRANA_01_GPIO_Port GPIOC
#define PC2_PIN_INPUT_MEMBRANA_02_Pin GPIO_PIN_2
#define PC2_PIN_INPUT_MEMBRANA_02_GPIO_Port GPIOC
#define PC3_PIN_INPUT_MEMBRANA_03_Pin GPIO_PIN_3
#define PC3_PIN_INPUT_MEMBRANA_03_GPIO_Port GPIOC
#define PA5_PIN_DISPLAY_A_Pin GPIO_PIN_5
#define PA5_PIN_DISPLAY_A_GPIO_Port GPIOA
#define PA6_PIN_DISPLAY_B_Pin GPIO_PIN_6
#define PA6_PIN_DISPLAY_B_GPIO_Port GPIOA
#define PA7_PIN_DISPLAY_C_Pin GPIO_PIN_7
#define PA7_PIN_DISPLAY_C_GPIO_Port GPIOA
#define PC4_PIN_DISPLAY_D_Pin GPIO_PIN_4
#define PC4_PIN_DISPLAY_D_GPIO_Port GPIOC
#define PC5_PIN_DISPLAY_E_Pin GPIO_PIN_5
#define PC5_PIN_DISPLAY_E_GPIO_Port GPIOC
#define PB0_PIN_DISPLAY_F_Pin GPIO_PIN_0
#define PB0_PIN_DISPLAY_F_GPIO_Port GPIOB
#define PB1_PIN_DISPLAY_G_Pin GPIO_PIN_1
#define PB1_PIN_DISPLAY_G_GPIO_Port GPIOB
#define PB2_PIN_DISPLAY_PD_Pin GPIO_PIN_2
#define PB2_PIN_DISPLAY_PD_GPIO_Port GPIOB
#define PB10_DISPLAY_CENTENA_MILHAR_Pin GPIO_PIN_10
#define PB10_DISPLAY_CENTENA_MILHAR_GPIO_Port GPIOB
#define PB12_DISPLAY_DEZENA_MILHAR_Pin GPIO_PIN_12
#define PB12_DISPLAY_DEZENA_MILHAR_GPIO_Port GPIOB
#define PB13_DISPLAY_UNIDADE_MILHAR_Pin GPIO_PIN_13
#define PB13_DISPLAY_UNIDADE_MILHAR_GPIO_Port GPIOB
#define PB14_DISPLAY_CENTENA_Pin GPIO_PIN_14
#define PB14_DISPLAY_CENTENA_GPIO_Port GPIOB
#define PB15_DISPLAY_DEZENA_Pin GPIO_PIN_15
#define PB15_DISPLAY_DEZENA_GPIO_Port GPIOB
#define PC6_DISPLAY_UNIDADE_Pin GPIO_PIN_6
#define PC6_DISPLAY_UNIDADE_GPIO_Port GPIOC
#define PC7_PIN_SAIDA_DIGITAL1_Pin GPIO_PIN_7
#define PC7_PIN_SAIDA_DIGITAL1_GPIO_Port GPIOC
#define PC9_PIN_SAIDA_DIGITAL2_Pin GPIO_PIN_9
#define PC9_PIN_SAIDA_DIGITAL2_GPIO_Port GPIOC
#define PB6_PIN_I2C1_SCL_Pin GPIO_PIN_6
#define PB6_PIN_I2C1_SCL_GPIO_Port GPIOB
#define PB7_PIN_I2C1_SDA_Pin GPIO_PIN_7
#define PB7_PIN_I2C1_SDA_GPIO_Port GPIOB
#define PB8_PIN_ENTRADA_01_Pin GPIO_PIN_8
#define PB8_PIN_ENTRADA_01_GPIO_Port GPIOB
#define PB9_PIN_ENTRADA_02_Pin GPIO_PIN_9
#define PB9_PIN_ENTRADA_02_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
