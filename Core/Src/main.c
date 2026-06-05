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
static const uint32_t VALOR_MAXIMO = 999999U;
static const uint32_t SETPOINT_DEFAULT = 100U;
static const uint32_t SETPOINT_OBRIGA_DEFAULT = 200U;
static const uint32_t EEPROM_ASSINATURA_APP = 0x554D4D32U;

static uint32_t contador = 0;
static uint32_t ultima_contagem = 0;
static uint32_t ultima_conferencia = 0;
static int32_t setpoint_inspecao = 0;
static int32_t setpoint_obriga_inspecao = 0;
static int32_t setpoint_temp = 0;

static uint8_t modo = 0;
static uint8_t atualiza_display_timeout = 0;
static uint16_t buzzer_periodo_timeout = 0;
static uint8_t buzzer_ligado_timeout = 0;
static uint16_t reset_contador_timeout = 0;
static uint16_t tac_relogio_timeout = 0;

static uint32_t eeprom_timer = 0;
uint8_t eeprom_flagaux_salvar = 0;

static volatile uint8_t serial_rx_buffer[64];
static volatile uint8_t serial_rx_head = 0;
static volatile uint8_t serial_rx_tail = 0;
// uint32_t setpoint = 100;

// Variáveis para controle do modo
// Variaveis display
// volatile uint16_t splash_timeout = 0;

// variaveis auxiliares
// uint8_t saida_01 = 0;

// variaveis eeprom

// flags auxiliares

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void splash_screen(uint16_t tempo_ms, uint8_t d5, uint8_t d4,
                          uint8_t d3, uint8_t d2, uint8_t d1, uint8_t d0);
static void processa_tick_50ms(void);
static void trata_tac_relogio_press(void);
static void trata_reset_contador(void);
static void atualiza_display_int32(uint32_t valor);
static void modo_contagem(void);
static void modo_setpoint_inspecao(void);
static void prepara_dados_eeprom(void);
static void salva_dados_agendado(void);
static void salva_dados_imediato(void);
static void salva_presets_flash(void);
static void carrega_config(void);
static void serial_init_rx(void);
static void serial_send_text(const char *text);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick.
   */
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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */

  eeprom_init();
  carrega_config();
  serial_init_rx();
  serial_send_text("UMMI CONTADOR STM\r\n");
  splash_screen(40, DSP_B, DSP_E, DSP_1, DSP_0, DSP_0, 2);
  modo = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if (timer_flag_50ms) {
      timer_flag_50ms = 0;
      processa_tick_50ms();
    }

    if (modo == 0) {
      modo_contagem();
    } else if (modo == 1 || modo == 2) {
      modo_setpoint_inspecao();
    } else {
      modo = 0;
    }

    if (eeprom_flagaux_salvar == 1) {
      eeprom_timer = HAL_GetTick();
      eeprom_flagaux_salvar = 2;
    }
    if (eeprom_flagaux_salvar == 2) {
      if (HAL_GetTick() - eeprom_timer > 1000) {
        eeprom_write(&dados);
        eeprom_flagaux_salvar = 0;
      }
    }
    eeprom_process();
  }
  /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
   * in the RCC_OscInitTypeDef structure.
   */
  RCC_OscInitStruct.OscillatorType =
      RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
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
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
   */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
    Error_Handler();
  }
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {

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
  if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
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
static void MX_RTC_Init(void) {

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
  if (HAL_RTC_Init(&hrtc) != HAL_OK) {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  USART1->CR1 = 0;
  USART1->CR2 = 0;
  USART1->CR3 = 0;
  USART1->BRR = (uint16_t)((HAL_RCC_GetPCLK2Freq() + 57600U) / 115200U);
  USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_RXNEIE |
                USART_CR1_UE;

  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA,
                    PA5_PIN_DISPLAY_A_Pin | PA6_PIN_DISPLAY_B_Pin |
                        PA7_PIN_DISPLAY_C_Pin,
                    GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC,
                    PC4_PIN_DISPLAY_D_Pin | PC5_PIN_DISPLAY_E_Pin |
                        PC6_DISPLAY_UNIDADE_Pin | PC7_PIN_SAIDA_DIGITAL1_Pin |
                        PC9_PIN_SAIDA_DIGITAL2_Pin,
                    GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(
      GPIOB,
      PB0_PIN_DISPLAY_F_Pin | PB1_PIN_DISPLAY_G_Pin | PB2_PIN_DISPLAY_PD_Pin |
          PB10_DISPLAY_CENTENA_MILHAR_Pin | PB12_DISPLAY_DEZENA_MILHAR_Pin |
          PB13_DISPLAY_UNIDADE_MILHAR_Pin | PB14_DISPLAY_CENTENA_Pin |
          PB15_DISPLAY_DEZENA_Pin,
      GPIO_PIN_RESET);

  /*Configure GPIO pins : PC1_PIN_INPUT_MEMBRANA_01_Pin
   * PC2_PIN_INPUT_MEMBRANA_02_Pin PC3_PIN_INPUT_MEMBRANA_03_Pin */
  GPIO_InitStruct.Pin = PC1_PIN_INPUT_MEMBRANA_01_Pin |
                        PC2_PIN_INPUT_MEMBRANA_02_Pin |
                        PC3_PIN_INPUT_MEMBRANA_03_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5_PIN_DISPLAY_A_Pin PA6_PIN_DISPLAY_B_Pin
   * PA7_PIN_DISPLAY_C_Pin */
  GPIO_InitStruct.Pin =
      PA5_PIN_DISPLAY_A_Pin | PA6_PIN_DISPLAY_B_Pin | PA7_PIN_DISPLAY_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PC4_PIN_DISPLAY_D_Pin PC5_PIN_DISPLAY_E_Pin
     PC6_DISPLAY_UNIDADE_Pin PC7_PIN_SAIDA_DIGITAL1_Pin
                           PC9_PIN_SAIDA_DIGITAL2_Pin */
  GPIO_InitStruct.Pin = PC4_PIN_DISPLAY_D_Pin | PC5_PIN_DISPLAY_E_Pin |
                        PC6_DISPLAY_UNIDADE_Pin | PC7_PIN_SAIDA_DIGITAL1_Pin |
                        PC9_PIN_SAIDA_DIGITAL2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0_PIN_DISPLAY_F_Pin PB1_PIN_DISPLAY_G_Pin
     PB2_PIN_DISPLAY_PD_Pin PB10_DISPLAY_CENTENA_MILHAR_Pin
                           PB12_DISPLAY_DEZENA_MILHAR_Pin
     PB13_DISPLAY_UNIDADE_MILHAR_Pin PB14_DISPLAY_CENTENA_Pin
     PB15_DISPLAY_DEZENA_Pin */
  GPIO_InitStruct.Pin =
      PB0_PIN_DISPLAY_F_Pin | PB1_PIN_DISPLAY_G_Pin | PB2_PIN_DISPLAY_PD_Pin |
      PB10_DISPLAY_CENTENA_MILHAR_Pin | PB12_DISPLAY_DEZENA_MILHAR_Pin |
      PB13_DISPLAY_UNIDADE_MILHAR_Pin | PB14_DISPLAY_CENTENA_Pin |
      PB15_DISPLAY_DEZENA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB8_PIN_ENTRADA_01_Pin */
  GPIO_InitStruct.Pin = PB8_PIN_ENTRADA_01_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PB9_PIN_ENTRADA_02_Pin */
  GPIO_InitStruct.Pin = PB9_PIN_ENTRADA_02_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

static void splash_screen(uint16_t timeout_50ms, uint8_t d5, uint8_t d4,
                          uint8_t d3, uint8_t d2, uint8_t d1, uint8_t d0) {
  splash_timeout = timeout_50ms;
  splash_digits[0] = d0;
  splash_digits[1] = d1;
  splash_digits[2] = d2;
  splash_digits[3] = d3;
  splash_digits[4] = d4;
  splash_digits[5] = d5;
}

static void processa_tick_50ms(void) {
  if (btn_relogio_status && tac_relogio_timeout < 65535U) {
    tac_relogio_timeout++;
  } else if (!btn_relogio_status) {
    tac_relogio_timeout = 0;
  }

  if (atualiza_display_timeout > 0) {
    atualiza_display_timeout--;
  }
  if (buzzer_periodo_timeout > 0) {
    buzzer_periodo_timeout--;
  }
  if (buzzer_ligado_timeout > 0) {
    buzzer_ligado_timeout--;
  }

  if (btn_min_status && btn_max_status && reset_contador_timeout < 65535U) {
    reset_contador_timeout++;
  } else {
    reset_contador_timeout = 0;
  }
}

static void trata_tac_relogio_press(void) {
  static uint8_t trava = 0;
  uint16_t tempo_minimo = (modo == 0) ? 60U : 5U;

  if (trava || !btn_relogio_status || tac_relogio_timeout < tempo_minimo) {
    if (!btn_relogio_status) {
      trava = 0;
    }
    return;
  }

  trava = 1;

  if (modo == 0) {
    setpoint_temp = setpoint_inspecao;
    modo = 1;
    splash_screen(20, DSP_S, DSP_E, DSP_T, DSP_1, DSP_N, DSP_S);
  } else if (modo == 1) {
    setpoint_inspecao = setpoint_temp;
    salva_presets_flash();
    setpoint_temp = setpoint_obriga_inspecao;
    modo = 2;
    splash_screen(20, DSP_S, DSP_E, DSP_T, DSP_O, DSP_B, DSP_R);
  } else if (modo == 2) {
    setpoint_obriga_inspecao = setpoint_temp;
    salva_presets_flash();
    modo = 0;
    splash_screen(30, DSP_MINUS, DSP_0, DSP_K, DSP_U, DSP_E, DSP_MINUS);
  }
}

static void trata_reset_contador(void) {
  static uint8_t trava = 0;

  if (btn_min_status && btn_max_status && reset_contador_timeout >= 200U) {
    if (!trava) {
      trava = 1;
      contador = 0;
      ultima_conferencia = 0;
      ultima_contagem = 0;
      splash_screen(20, DSP_MINUS, DSP_MINUS, DSP_MINUS, DSP_R, DSP_S, DSP_T);
      HAL_GPIO_WritePin(SAIDA_01_PORT, SAIDA_01, LOW);
      HAL_GPIO_WritePin(SAIDA_02_PORT, SAIDA_02, LOW);
      buzzer_periodo_timeout = 0;
      buzzer_ligado_timeout = 0;
      atualiza_display_timeout = 0;
      salva_dados_imediato();
    }
  } else {
    trava = 0;
  }
}

static void atualiza_display_int32(uint32_t valor) {
  if (atualiza_display_timeout == 0) {
    atualiza_display_timeout = 2;
    display_atualiza(valor);
  }
}

static void modo_contagem(void) {
  uint16_t pulsos_pendentes;
  uint8_t sensor_gabarito_evento;

  if (btn_max_status && !btn_min_status) {
    atualiza_display_int32(ultima_conferencia);
  } else {
    atualiza_display_int32(contador);
  }

  __disable_irq();
  pulsos_pendentes = entrada_digital_pulsos_pendentes;
  entrada_digital_pulsos_pendentes = 0;
  sensor_gabarito_evento = entrada_digital_02_evento_pendente;
  entrada_digital_02_evento_pendente = 0;
  __enable_irq();

  if (pulsos_pendentes > 0U) {
    contador = (contador + pulsos_pendentes) % (VALOR_MAXIMO + 1U);
    atualiza_display_timeout = 0;
  }

  if (sensor_gabarito_evento) {
    ultima_conferencia = contador;
    splash_screen(20, DSP_1, DSP_N, DSP_S, DSP_P, DSP_E, DSP_C);
    salva_dados_imediato();
  }

  uint32_t diferenca;
  if (contador >= ultima_conferencia) {
    diferenca = contador - ultima_conferencia;
  } else {
    diferenca = (VALOR_MAXIMO - ultima_conferencia) + contador + 1U;
  }

  HAL_GPIO_WritePin(SAIDA_01_PORT, SAIDA_01,
                    (diferenca >= (uint32_t)setpoint_inspecao) ? HIGH : LOW);

  if (diferenca >= (uint32_t)setpoint_obriga_inspecao) {
    HAL_GPIO_WritePin(SAIDA_02_PORT, SAIDA_02, HIGH);
    if (buzzer_periodo_timeout == 0 && buzzer_ligado_timeout == 0) {
      buzzer_ligado_timeout = 20;
      buzzer_periodo_timeout = 200;
    }
    if (buzzer_ligado_timeout == 0 && buzzer_periodo_timeout == 0) {
      buzzer_ligado_timeout = 20;
      buzzer_periodo_timeout = 200;
    }
  } else {
    HAL_GPIO_WritePin(SAIDA_02_PORT, SAIDA_02, LOW);
    buzzer_periodo_timeout = 0;
    buzzer_ligado_timeout = 0;
  }

  trata_tac_relogio_press();
  trata_reset_contador();

  if (contador != ultima_contagem) {
    display_set_decimal_points(4, 0x01);
    salva_dados_agendado();
    ultima_contagem = contador;
  }
}

static void modo_setpoint_inspecao(void) {
  if (btn_max_status && btn_max_timeout == 0) {
    if (btn_max_turbo >= 30)
      setpoint_temp += 100;
    else if (btn_max_turbo >= 20)
      setpoint_temp += 20;
    else if (btn_max_turbo >= 10)
      setpoint_temp += 10;
    else
      setpoint_temp += 1;

    if (setpoint_temp > (int32_t)VALOR_MAXIMO)
      setpoint_temp = VALOR_MAXIMO;
    if (modo == 1 && setpoint_temp > setpoint_obriga_inspecao)
      setpoint_temp = setpoint_obriga_inspecao;
    if (modo == 2 && setpoint_temp < setpoint_inspecao)
      setpoint_temp = setpoint_inspecao;

    if (btn_max_turbo < 30)
      btn_max_turbo++;
    btn_max_timeout = (btn_max_turbo >= 5) ? 4 : 7;
    atualiza_display_timeout = 0;
  } else if (!btn_max_status) {
    btn_max_timeout = 2;
    btn_max_turbo = 0;
  }

  if (btn_min_status && btn_min_timeout == 0) {
    if (btn_min_turbo >= 30)
      setpoint_temp -= 100;
    else if (btn_min_turbo >= 20)
      setpoint_temp -= 20;
    else if (btn_min_turbo >= 10)
      setpoint_temp -= 10;
    else
      setpoint_temp -= 1;

    if (setpoint_temp < 0)
      setpoint_temp = 0;
    if (modo == 1 && setpoint_temp > setpoint_obriga_inspecao)
      setpoint_temp = setpoint_obriga_inspecao;
    if (modo == 2 && setpoint_temp < setpoint_inspecao)
      setpoint_temp = setpoint_inspecao;

    if (btn_min_turbo < 30)
      btn_min_turbo++;
    btn_min_timeout = (btn_min_turbo >= 5) ? 4 : 7;
    atualiza_display_timeout = 0;
  } else if (!btn_min_status) {
    btn_min_timeout = 2;
    btn_min_turbo = 0;
  }

  atualiza_display_int32((uint32_t)setpoint_temp);
  trata_tac_relogio_press();
}

void ativa_alarme(void) {}

void btn_relogio_processado(void) {}

static void prepara_dados_eeprom(void) {
  dados.assinatura = EEPROM_ASSINATURA_APP;
  dados.contador = contador;
  dados.ultima_conferencia = ultima_conferencia;
  dados.setpoint_01 = (uint32_t)setpoint_inspecao;
  dados.setpoint_obrigatorio_01 = (uint32_t)setpoint_obriga_inspecao;
}

static void salva_dados_agendado(void) {
  prepara_dados_eeprom();
  eeprom_flagaux_salvar = 1;
}

static void salva_dados_imediato(void) {
  prepara_dados_eeprom();
  eeprom_write(&dados);
  eeprom_process();
  eeprom_flagaux_salvar = 0;
}

static void salva_presets_flash(void) {
  prepara_dados_eeprom();
  eeprom_write_presets((uint32_t)setpoint_inspecao,
                       (uint32_t)setpoint_obriga_inspecao);
}

static void carrega_config(void) {
  dados = eeprom_read();

  if (dados.assinatura != EEPROM_ASSINATURA_APP) {
    dados.assinatura = EEPROM_ASSINATURA_APP;
    dados.contador = 0;
    dados.ultima_conferencia = 0;
    dados.setpoint_01 = SETPOINT_DEFAULT;
    dados.setpoint_obrigatorio_01 = SETPOINT_OBRIGA_DEFAULT;
    eeprom_write_presets(dados.setpoint_01, dados.setpoint_obrigatorio_01);
  }

  if (dados.setpoint_01 == 0U && dados.setpoint_obrigatorio_01 == 0U) {
    dados.setpoint_01 = SETPOINT_DEFAULT;
    dados.setpoint_obrigatorio_01 = SETPOINT_OBRIGA_DEFAULT;
    eeprom_write_presets(dados.setpoint_01, dados.setpoint_obrigatorio_01);
  }

  setpoint_inspecao = dados.setpoint_01;
  if (setpoint_inspecao > (int32_t)VALOR_MAXIMO)
    setpoint_inspecao = SETPOINT_DEFAULT;

  setpoint_obriga_inspecao = dados.setpoint_obrigatorio_01;
  if (setpoint_obriga_inspecao > (int32_t)VALOR_MAXIMO)
    setpoint_obriga_inspecao = SETPOINT_OBRIGA_DEFAULT;

  if (setpoint_inspecao > setpoint_obriga_inspecao) {
    setpoint_inspecao = SETPOINT_DEFAULT;
    setpoint_obriga_inspecao = SETPOINT_OBRIGA_DEFAULT;
  }

  contador = dados.contador;
  if (contador > VALOR_MAXIMO)
    contador = 0;

  ultima_conferencia = dados.ultima_conferencia;
  if (ultima_conferencia > VALOR_MAXIMO)
    ultima_conferencia = 0;

  ultima_contagem = contador;
  atualiza_display_timeout = 0;
}

static void serial_init_rx(void) {
  (void)USART1->DR;
  USART1->CR1 |= USART_CR1_RXNEIE;
}

static void serial_send_text(const char *text) {
  while (*text != '\0') {
    uint32_t start = HAL_GetTick();
    while ((USART1->SR & USART_SR_TXE) == 0) {
      if ((HAL_GetTick() - start) > 20U) {
        return;
      }
    }
    USART1->DR = (uint8_t)*text++;
  }
}

void serial_usart1_irq_handler(void) {
  if ((USART1->SR & USART_SR_RXNE) != 0) {
    uint8_t byte = (uint8_t)USART1->DR;
    uint8_t next = (uint8_t)((serial_rx_head + 1U) % sizeof(serial_rx_buffer));
    if (next != serial_rx_tail) {
      serial_rx_buffer[serial_rx_head] = byte;
      serial_rx_head = next;
    }
  }
}
/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  HAL_GPIO_WritePin(SAIDA_01_PORT, SAIDA_01, LOW);
  HAL_GPIO_WritePin(SAIDA_02_PORT, SAIDA_02, LOW);
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line) {
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
