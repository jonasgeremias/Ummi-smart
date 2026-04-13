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
 *
 * PROJETO CONTADOR DE PEÇAS COM DISPLAY 7 SEGMENTOS
 * GRAVAÇÃO DE VALORES NA EEPROM
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <defines.h>
#include <display.h>
#include <eeprom.h>
#include <timer.h>

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
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */
int32_t contador_pecas = 0;
int32_t contador_pecas_anterior = 0;
int32_t setpoint = 0;
int32_t setpoint_boost = 0;
int32_t setpoint_obrigatorio = 0;
int32_t setpoint_obrigatorio_boost = 0;
// uint32_t setpoint = 100;

// Variáveis para controle do modo
uint8_t modo = 0;
uint32_t setpoint_obrigatorio_contador = 0;

// Variaveis display
// volatile uint16_t splash_timeout = 0;

// variaveis auxiliares
//uint8_t saida_01 = 0;

// variaveis eeprom
uint32_t eeprom_timer = 0;
uint32_t setpoint_obrigatorio_guardado = 0;

// flags auxiliares
uint8_t flagaux_setpoint = 0;
uint8_t eeprom_flagaux_salvar = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  splash_screen(200, 11, 14, 1, 0, 0, 1);
  eeprom_init();
  // dados = eeprom_read();
  carrega_config();
  // if (dados.setpoint_obrigatorio_01 == 0xFFFF){
  //   dados.setpoint_01 = 100;
  //   dados.setpoint_obrigatorio_01 = 200;

  //   eeprom_write(&dados);
  // }
  HAL_Delay(200);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    le_botoes();
    modo_operacao();
    if (eeprom_flagaux_salvar == 1){
      eeprom_timer = HAL_GetTick();
      eeprom_flagaux_salvar = 2;
    }
    if (eeprom_flagaux_salvar == 2) {
      if (HAL_GetTick() - eeprom_timer > 1000){
        eeprom_write(&dados);
        eeprom_flagaux_salvar = 0;
      }
    }
    eeprom_process();
    // contagem();
  }
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, PA5_PIN_DISPLAY_A_Pin|PA6_PIN_DISPLAY_B_Pin|PA7_PIN_DISPLAY_C_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, PC4_PIN_DISPLAY_D_Pin|PC5_PIN_DISPLAY_E_Pin|PC6_DISPLAY_UNIDADE_Pin|PC7_PIN_SAIDA_DIGITAL1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, PB0_PIN_DISPLAY_F_Pin|PB1_PIN_DISPLAY_G_Pin|PB2_PIN_DISPLAY_PD_Pin|PB10_DISPLAY_CENTENA_MILHAR_Pin
                          |PB12_DISPLAY_DEZENA_MILHAR_Pin|PB13_DISPLAY_UNIDADE_MILHAR_Pin|PB14_DISPLAY_CENTENA_Pin|PB15_DISPLAY_DEZENA_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC1_PIN_INPUT_MEMBRANA_01_Pin PC2_PIN_INPUT_MEMBRANA_02_Pin PC3_PIN_INPUT_MEMBRANA_03_Pin */
  GPIO_InitStruct.Pin = PC1_PIN_INPUT_MEMBRANA_01_Pin|PC2_PIN_INPUT_MEMBRANA_02_Pin|PC3_PIN_INPUT_MEMBRANA_03_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5_PIN_DISPLAY_A_Pin PA6_PIN_DISPLAY_B_Pin PA7_PIN_DISPLAY_C_Pin */
  GPIO_InitStruct.Pin = PA5_PIN_DISPLAY_A_Pin|PA6_PIN_DISPLAY_B_Pin|PA7_PIN_DISPLAY_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PC4_PIN_DISPLAY_D_Pin PC5_PIN_DISPLAY_E_Pin PC6_DISPLAY_UNIDADE_Pin PC7_PIN_SAIDA_DIGITAL1_Pin */
  GPIO_InitStruct.Pin = PC4_PIN_DISPLAY_D_Pin|PC5_PIN_DISPLAY_E_Pin|PC6_DISPLAY_UNIDADE_Pin|PC7_PIN_SAIDA_DIGITAL1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0_PIN_DISPLAY_F_Pin PB1_PIN_DISPLAY_G_Pin PB2_PIN_DISPLAY_PD_Pin PB10_DISPLAY_CENTENA_MILHAR_Pin
                           PB12_DISPLAY_DEZENA_MILHAR_Pin PB13_DISPLAY_UNIDADE_MILHAR_Pin PB14_DISPLAY_CENTENA_Pin PB15_DISPLAY_DEZENA_Pin */
  GPIO_InitStruct.Pin = PB0_PIN_DISPLAY_F_Pin|PB1_PIN_DISPLAY_G_Pin|PB2_PIN_DISPLAY_PD_Pin|PB10_DISPLAY_CENTENA_MILHAR_Pin
                          |PB12_DISPLAY_DEZENA_MILHAR_Pin|PB13_DISPLAY_UNIDADE_MILHAR_Pin|PB14_DISPLAY_CENTENA_Pin|PB15_DISPLAY_DEZENA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB8_PIN_ENTRADA_01_Pin */
  GPIO_InitStruct.Pin = PB8_PIN_ENTRADA_01_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(PB8_PIN_ENTRADA_01_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void splash_screen(uint16_t tempo_ms, uint8_t d5, uint8_t d4, uint8_t d3,
                   uint8_t d2, uint8_t d1, uint8_t d0) {
  splash_timeout = tempo_ms;
  splash_digits[0] = d0;
  splash_digits[1] = d1;
  splash_digits[2] = d2;
  splash_digits[3] = d3;
  splash_digits[4] = d4;
  splash_digits[5] = d5;
}

void modo_operacao() {

  switch (btn_relogio_evento) {
  case 1:
    if (modo == 0) {
      modo = 1;
      flagaux_setpoint = 1;
    } else if (modo == 1) {
      modo = 2;
      flagaux_setpoint = 2;
    } else {
      modo = 0;
      flagaux_setpoint = 0;
    }
    break;

  case 2:
    modo = 0;
    flagaux_setpoint = 0;
    break;

  case 3:
    break;
  }

  btn_relogio_evento = 0;
  switch (modo) {
  case 0:
    contagem();
    break;
  case 1:
    modo_setpoint();
    break;
  case 2:
    modo_setpoint_obrigatorio();
    break;
  }
}

void contagem() {
  static uint8_t trava = 0;

  if (contador_pecas > 999999) {
    contador_pecas = 0;
  } else if (contador_pecas < 0) {
    contador_pecas = 999999;
  }

  display_atualiza(contador_pecas);
  if ((entrada_digital_status) && (!trava)) {
    trava = 1;
    contador_pecas++;
    contador_pecas_anterior = contador_pecas;
  }

  if (contador_pecas > setpoint) {
    SAIDA_01 = 1;
  } else {
    SAIDA_01 = 0;
  }

  if (!entrada_digital_status) {
    trava = 0;
  }

  if (contador_pecas_anterior != contador_pecas) {
    salva_configs();
  }
}

void modo_setpoint() {
  static uint32_t setpoint_guardado = 0;

  setpoint = setpoint_boost;

  if (setpoint != setpoint_guardado) {
    setpoint_guardado = setpoint;
    dados.setpoint_01 = setpoint_guardado;
    // eeprom_write(&dados);
    eeprom_flagaux_salvar = 1;
  }

  if (setpoint > 999999) {
    setpoint = 0;
  } else if (setpoint < 0) {
    setpoint = 999999;
  }

  display_atualiza(setpoint);
  // salva_configs();
  //  Implementar lógica para modo de configuração do setpoint
}

// void modo_setpoint_obrigatorio() {

//   setpoint_obrigatorio = setpoint_obrigatorio_boost;

//   // if (setpoint_obrigatorio != setpoint_obrigatorio_guardado) {
//   //   setpoint_obrigatorio_guardado = setpoint_obrigatorio;
//   //   dados.setpoint_obrigatorio_01 = setpoint_obrigatorio_guardado;
//   //   // eeprom_write(&dados);
//   //   eeprom_flagaux_salvar = 1;
//   // }

//   if (dados.setpoint_obrigatorio_01 != setpoint_obrigatorio) {
//     dados.setpoint_obrigatorio_01 = setpoint_obrigatorio;
//     // eeprom_write(&dados);
//     eeprom_flagaux_salvar = 1;
//   }

//   display_atualiza(setpoint_obrigatorio);
//   // salva_configs();
// }

void modo_setpoint_obrigatorio() {
  static uint32_t ultimo_valor = 0;
  static uint32_t tempo_ultima_mudanca = 0;

  setpoint_obrigatorio = setpoint_obrigatorio_boost;

  if (setpoint_obrigatorio != ultimo_valor) {
    ultimo_valor = setpoint_obrigatorio;
    tempo_ultima_mudanca = HAL_GetTick();
  }

  // Só salva se ficou 1s sem mudar
  if ((HAL_GetTick() - tempo_ultima_mudanca) > 1000) {
    if (dados.setpoint_obrigatorio_01 != setpoint_obrigatorio) {
      dados.setpoint_obrigatorio_01 = setpoint_obrigatorio;
      eeprom_flagaux_salvar = 1;
    }
  }

  display_atualiza(setpoint_obrigatorio);
}

void btn_relogio_processado(void) {

  if (setpoint_obrigatorio > 999999) {
    setpoint_obrigatorio = 0;
  }

  // borda de subida
  if (btn_relogio_status && !btn_relogio_borda_anterior) {
    btn_relogio_contador = 0;
    btn_relogio_hold = 0;
  }

  // botão pressionado
  if (btn_relogio_status) {

    if (btn_relogio_contador < 1000)
      btn_relogio_contador++;

    // LONG PRESS (1x só)
    if (btn_relogio_contador >= TEMPO_LONG_PRESS) {
      btn_relogio_evento = 2;
    }
  }

  // borda de descida → CLICK
  if (!btn_relogio_status && btn_relogio_borda_anterior) {
    if (btn_relogio_contador < TEMPO_LONG_PRESS) {
      btn_relogio_evento = 1;
    }
  }

  btn_relogio_borda_anterior = btn_relogio_status;
}

void le_botoes() {
  static uint8_t max_press = 0;
  static uint8_t min_press = 0;

  if (flagaux_setpoint == 1) {
    if (setpoint_boost > 999999) {
      setpoint_boost = 0;
    } else if (setpoint_boost < 0) {
      setpoint_boost = 999999;
    }
  }
  if (flagaux_setpoint == 2) {
    if (setpoint_obrigatorio_boost > 999999) {
      setpoint_obrigatorio_boost = 0;
    } else if (setpoint_obrigatorio_boost < 0) {
      setpoint_obrigatorio_boost = 999999;
    }
  }

  if (btn_max_status)
    max_press = 1;

  if (btn_min_status)
    min_press = 1;

  if (max_press && min_press) {

    if (flagaux_setpoint == 1) {
      setpoint_boost = 0;
    } else if (flagaux_setpoint == 2) {
      setpoint_obrigatorio_boost = 0;
    }

    max_press = 0;
    min_press = 0;
  }

  // reset quando solta ambos
  if (!btn_max_status && !btn_min_status) {
    max_press = 0;
    min_press = 0;
  }

  if (btn_max_status) {
    if (btn_max_timeout == 0) {
      if (btn_max_turbo >= 25) {
        if (flagaux_setpoint == 0) {
          contador_pecas += 100000;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost += 100000;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost += 100000;
        }
      } else if (btn_max_turbo >= 20) {
        if (flagaux_setpoint == 0) {
          contador_pecas += 10000;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost += 10000;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost += 10000;
        }
      } else if (btn_max_turbo >= 15) {
        if (flagaux_setpoint == 0) {
          contador_pecas += 1000;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost += 1000;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost += 1000;
        }
      } else if (btn_max_turbo >= 10) {
        if (flagaux_setpoint == 0) {
          contador_pecas += 100;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost += 100;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost += 100;
        }
      } else if (btn_max_turbo >= 8) {
        if (flagaux_setpoint == 0) {
          contador_pecas += 50;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost += 50;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost += 50;
        }
      } else if (btn_max_turbo >= 5) {
        if (flagaux_setpoint == 0) {
          contador_pecas += 10;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost += 10;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost += 10;
        }
      } else {
        if (flagaux_setpoint == 0) {
          contador_pecas++;
        }
        if (flagaux_setpoint == 1) {
          setpoint_boost++;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost++;
        }
      }
      if (btn_max_turbo < 25) {
        btn_max_turbo++;
      }
      if (btn_max_turbo >= 5) {
        btn_max_timeout = TURBO_LEVEL2;
      } else {
        btn_max_timeout = TURBO_LEVEL1;
      }
    }
  } else {
    btn_max_turbo = 0;
  }

  if (btn_min_status) {
    if (btn_min_timeout == 0) {
      if (btn_min_turbo >= 25) {
        if (flagaux_setpoint == 0) {
          contador_pecas -= 100000;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost -= 100000;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost -= 100000;
        }
      } else if (btn_min_turbo >= 20) {
        if (flagaux_setpoint == 0) {
          contador_pecas -= 10000;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost -= 10000;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost -= 10000;
        }
      } else if (btn_min_turbo >= 15) {
        if (flagaux_setpoint == 0) {
          contador_pecas -= 1000;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost -= 1000;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost -= 1000;
        }
      } else if (btn_min_turbo >= 10) {
        if (flagaux_setpoint == 0) {
          contador_pecas -= 100;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost -= 100;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost -= 100;
        }
      } else if (btn_min_turbo >= 8) {
        if (flagaux_setpoint == 0) {
          contador_pecas -= 50;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost -= 50;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost -= 50;
        }
      } else if (btn_min_turbo >= 5) {
        if (flagaux_setpoint == 0) {
          contador_pecas -= 10;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost -= 10;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost -= 10;
        }
      } else {
        if (flagaux_setpoint == 0) {
          contador_pecas--;
        } else if (flagaux_setpoint == 1) {
          setpoint_boost--;
        } else if (flagaux_setpoint == 2) {
          setpoint_obrigatorio_boost--;
        }
      }
      if (btn_min_turbo < 25) {
        btn_min_turbo++;
      }
      if (btn_min_turbo >= 5) {
        btn_min_timeout = TURBO_LEVEL2;
      } else {
        btn_min_timeout = TURBO_LEVEL1;
      }
    }
  } else {
    btn_min_turbo = 0;
  }

  btn_max_borda_anterior = btn_max_status;
  btn_min_borda_anterior = btn_min_status;
  btn_relogio_borda_anterior = btn_relogio_status;
}

void salva_configs() {
  static int32_t ultimo_salvo = -1;

  if ((contador_pecas % 10 == 0) && (contador_pecas != ultimo_salvo)) {
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, contador_pecas);
    ultimo_salvo = contador_pecas;
  }
  if (flagaux_setpoint == 1) {
    // HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, setpoint);
  } else if (flagaux_setpoint == 2) {
    // HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, setpoint_obrigatorio);
  }
}

void carrega_config() {
  contador_pecas = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);
  // setpoint = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0);
  // setpoint_obrigatorio = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1);

  // setpoint_boost = setpoint;
  // setpoint_obrigatorio_boost = setpoint_obrigatorio;

  // eeprom_read(&dados);
  dados = eeprom_read();

  //protecao para lixo
  if (dados.setpoint_01 > 999999) {
    dados.setpoint_01 = 0;
  }
  if (dados.setpoint_obrigatorio_01 > 999999) {
    dados.setpoint_obrigatorio_01 = 0;
  }

  //joga para variaveis do sistema
  setpoint = dados.setpoint_01;
  setpoint_boost = setpoint;

  setpoint_obrigatorio = dados.setpoint_obrigatorio_01;
  setpoint_obrigatorio_boost = setpoint_obrigatorio;
}

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
  while (1) {
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
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
