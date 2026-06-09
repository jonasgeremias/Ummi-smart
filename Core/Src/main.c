/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Ummi Control (termo-higrometro) — orquestracao principal.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "defines.h"
#include "display.h"
#include "timer.h"
#include "config.h"
#include "sensors.h"
#include "rtc_app.h"
#include "datalog.h"
#include "control.h"
#include "ui.h"
#include "protocol.h"
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

USART_HandleTypeDef husart1;
USART_HandleTypeDef husart6;

/* USER CODE BEGIN PV */
static uint8_t leitura_analogica_div = 0; /* divisor 10 ms -> 20 ms */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_Init(void);
static void MX_USART6_Init(void);
/* USER CODE BEGIN PFP */
static void iwdg_start(void);
void iwdg_refresh(void); /* publico: chamado por config.c durante o erase da Flash */
static void saidas_estado_seguro(void);
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
  HAL_Init();

  /* USER CODE BEGIN Init */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  saidas_estado_seguro();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_USART1_Init();
  MX_USART6_Init();
  /* USER CODE BEGIN 2 */
  ui_splash_marca(); /* marca BE1 1.0 ja no boot, antes das inits longas */
  sensors_init();
  config_init();
  rtc_app_init();
  datalog_init();
  control_init();
  protocol_init();
  ui_init();

  ui_splash_inicial(); /* sequencia bloqueante curta (antes do IWDG) */

  iwdg_start(); /* watchdog ligado apos o splash */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    iwdg_refresh();

    if (timer_flag_10ms) {
      timer_flag_10ms = 0;
      if (++leitura_analogica_div >= 2U) {
        leitura_analogica_div = 0;
        sensors_update();
      }
    }

    if (timer_flag_50ms) {
      timer_flag_50ms = 0;
      ui_tick_50ms();
      control_tick_50ms();
    }

    if (timer_flag_1000ms) {
      timer_flag_1000ms = 0;
      datalog_tick_1s();
    }

    protocol_process();
    ui_tick();
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

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

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
  */
static void MX_I2C1_Init(void)
{
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
}

/**
  * @brief RTC Initialization Function
  */
static void MX_RTC_Init(void)
{
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
}

/**
  * @brief USART1 Initialization Function
  */
static void MX_USART1_Init(void)
{
  husart1.Instance = USART1;
  husart1.Init.BaudRate = 9600;
  husart1.Init.WordLength = USART_WORDLENGTH_8B;
  husart1.Init.StopBits = USART_STOPBITS_1;
  husart1.Init.Parity = USART_PARITY_NONE;
  husart1.Init.Mode = USART_MODE_TX_RX;
  husart1.Init.CLKPolarity = USART_POLARITY_LOW;
  husart1.Init.CLKPhase = USART_PHASE_1EDGE;
  husart1.Init.CLKLastBit = USART_LASTBIT_DISABLE;
  if (HAL_USART_Init(&husart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USART1 e reconfigurado como assincrono 8N1 em protocol_init(). */
}

/**
  * @brief USART6 Initialization Function
  */
static void MX_USART6_Init(void)
{
  husart6.Instance = USART6;
  husart6.Init.BaudRate = 115200;
  husart6.Init.WordLength = USART_WORDLENGTH_8B;
  husart6.Init.StopBits = USART_STOPBITS_1;
  husart6.Init.Parity = USART_PARITY_NONE;
  husart6.Init.Mode = USART_MODE_TX_RX;
  husart6.Init.CLKPolarity = USART_POLARITY_LOW;
  husart6.Init.CLKPhase = USART_PHASE_1EDGE;
  husart6.Init.CLKLastBit = USART_LASTBIT_DISABLE;
  if (HAL_USART_Init(&husart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
/**
  * @brief Watchdog independente por registradores (~2,5 s @LSI ~32 kHz).
  *        LSI/64 ~= 500 Hz; reload 1250 -> ~2,5 s. O driver HAL do IWDG nao
  *        esta incluido neste projeto, por isso o acesso direto fica isolado
  *        aqui (main.c e o "dono" do watchdog). Outros modulos chamam
  *        iwdg_refresh() — sem tocar em registradores.
  */
static void iwdg_start(void)
{
  IWDG->KR = 0x5555U;   /* habilita escrita em PR/RLR */
  IWDG->PR = 0x04U;     /* prescaler /64 */
  IWDG->RLR = 1250U;    /* reload */
  IWDG->KR = 0xAAAAU;   /* refresh inicial */
  IWDG->KR = 0xCCCCU;   /* start */
}

void iwdg_refresh(void)
{
  IWDG->KR = 0xAAAAU;
}

/**
  * @brief Coloca todas as saidas em estado seguro (reles e buzzer desligados).
  */
static void saidas_estado_seguro(void)
{
  HAL_GPIO_WritePin(SAIDA_01_PORT, SAIDA_01, LOW);
  HAL_GPIO_WritePin(SAIDA_02_PORT, SAIDA_02, LOW);
  HAL_GPIO_WritePin(SD_BUZZER_GPIO_Port, SD_BUZZER_Pin, LOW);
}
/* USER CODE END 4 */

/**
  * @brief GPIO Initialization Function
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, LED_STATUS4_Pin|PC4_PIN_DISPLAY_D_Pin|PC5_PIN_DISPLAY_E_Pin|PC6_DISPLAY_UNIDADE_Pin
                          |PC7_PIN_SAIDA_DIGITAL1_Pin|PC9_PIN_SAIDA_DIGITAL2_Pin|LED_STATUS1_Pin|LED_STATUS2_Pin
                          |LED_STATUS3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, PA5_PIN_DISPLAY_A_Pin|PA6_PIN_DISPLAY_B_Pin|PA7_PIN_DISPLAY_C_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, PB0_PIN_DISPLAY_F_Pin|PB1_PIN_DISPLAY_G_Pin|PB2_PIN_DISPLAY_PD_Pin|PB10_DISPLAY_CENTENA_MILHAR_Pin
                          |PB12_DISPLAY_DEZENA_MILHAR_Pin|PB13_DISPLAY_UNIDADE_MILHAR_Pin|PB14_DISPLAY_CENTENA_Pin|PB15_DISPLAY_DEZENA_Pin
                          |SD_BUZZER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED/DISPLAY/SAIDAS em GPIOC */
  GPIO_InitStruct.Pin = LED_STATUS4_Pin|PC4_PIN_DISPLAY_D_Pin|PC5_PIN_DISPLAY_E_Pin|PC6_DISPLAY_UNIDADE_Pin
                          |PC7_PIN_SAIDA_DIGITAL1_Pin|PC9_PIN_SAIDA_DIGITAL2_Pin|LED_STATUS1_Pin|LED_STATUS2_Pin
                          |LED_STATUS3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : EA_12V_Pin */
  GPIO_InitStruct.Pin = EA_12V_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(EA_12V_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : botoes da membrana */
  GPIO_InitStruct.Pin = PC1_PIN_INPUT_MEMBRANA_01_Pin|PC2_PIN_INPUT_MEMBRANA_02_Pin|PC3_PIN_INPUT_MEMBRANA_03_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : entradas analogicas em GPIOA */
  GPIO_InitStruct.Pin = EA_UMIDADE_Pin|EA_TEMPERATURA_Pin|EA_TEMP_REF_GND_Pin|EA_5V_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : segmentos em GPIOA */
  GPIO_InitStruct.Pin = PA5_PIN_DISPLAY_A_Pin|PA6_PIN_DISPLAY_B_Pin|PA7_PIN_DISPLAY_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : segmentos/mux/buzzer em GPIOB */
  GPIO_InitStruct.Pin = PB0_PIN_DISPLAY_F_Pin|PB1_PIN_DISPLAY_G_Pin|PB2_PIN_DISPLAY_PD_Pin|PB10_DISPLAY_CENTENA_MILHAR_Pin
                          |PB12_DISPLAY_DEZENA_MILHAR_Pin|PB13_DISPLAY_UNIDADE_MILHAR_Pin|PB14_DISPLAY_CENTENA_Pin|PB15_DISPLAY_DEZENA_Pin
                          |SD_BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : entradas digitais */
  GPIO_InitStruct.Pin = PB8_PIN_ENTRADA_01_Pin|PB9_PIN_ENTRADA_02_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  saidas_estado_seguro();
  __disable_irq();
  while (1) {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
