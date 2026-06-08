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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint32_t assinatura;
  uint16_t versao;
  uint16_t record_size;
  uint16_t capacidade;
  uint16_t proximo_indice;
  uint16_t quantidade_valida;
  uint16_t reservado;
  uint32_t seq_global;
  uint32_t crc;
} datalog_header_t;

typedef struct {
  uint32_t timestamp;
  int16_t temperatura_dC;
  uint16_t umidade_dUR;
  uint16_t flags;
  uint32_t crc;
} datalog_record_t;

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
static const uint32_t EEPROM_ASSINATURA_APP = 0x554D4D32U;
static const uint32_t ADC_VREF_MV = 3300U;
static const uint32_t ADC_FULL_SCALE = 4095U;
static const uint8_t ADC_MEDIA_QTD_AMOSTRAS = 8U;
static const uint8_t ADC_MEDIA_SHIFT = 3U;
static const uint8_t ADC_MIN_AMOSTRAS_STATUS = 6U;
static const uint16_t ADC_UMIDADE_DESCONECTADO_mV = 300U;
static const uint16_t ADC_TEMPERATURA_DESCONECTADO_AD = 5U;
static const uint16_t UMIDADE_ZERO_MV = 758U;
static const uint16_t UMIDADE_FUNDO_MV = 3938U;
static const int32_t HISTERESE_TEMPERATURA_DEFAULT_dC = 10;
static const int32_t LIMITE_TEMPERATURA_ALTA_DEFAULT_dC = 700;
static const int32_t LIMITE_TEMPERATURA_BAIXA_DEFAULT_dC = 250;
static const uint16_t TEMPO_ESTABILIZACAO_BAIXA_50MS = 6000U;
static const uint16_t TEMPO_MINIMO_COMUTACAO_RELE_50MS = 100U;
static const uint16_t TEMPO_BUZZER_ON_50MS = 10U;
static const uint16_t TEMPO_BUZZER_OFF_50MS = 40U;
static const uint16_t TENSAO_5V_MIN_mV = 4500U;
static const uint16_t TENSAO_5V_MAX_mV = 5500U;
static const uint16_t TENSAO_12V_MIN_mV = 10000U;
static const uint16_t TENSAO_12V_MAX_mV = 15000U;
static const uint16_t DATALOG_PERIODO_DEFAULT_s = 60U;
static const uint16_t DATALOG_BASE_ADDR = 0x0200U;
static const uint16_t DATALOG_RECORDS_ADDR = 0x0240U;
static const uint32_t EXT_EEPROM_SIZE_BYTES = 32768U;
static const uint32_t DATALOG_ASSINATURA = 0x554D4C47U;
static const uint16_t DATALOG_VERSAO = 1U;
static const uint8_t EXT_EEPROM_ADDR = (0x50U << 1);
static const uint16_t EXT_EEPROM_PAGE_SIZE = 64U;
static const uint16_t EXT_EEPROM_TIMEOUT_MS = 20U;
#define SERIAL_RX_BUFFER_SIZE 256U
#define SERIAL_RX_TIMEOUT_MS 10U

static int32_t temperatura_dC = 0;
static uint16_t umidade_dUR = 0;
static uint16_t tensao_5v_mV = 0;
static uint16_t tensao_12v_mV = 0;
static int32_t setpoint_temperatura_dC = 450;
static int32_t histerese_temperatura_dC = HISTERESE_TEMPERATURA_DEFAULT_dC;
static int32_t limite_temperatura_alta_dC = LIMITE_TEMPERATURA_ALTA_DEFAULT_dC;
static int32_t limite_temperatura_baixa_dC = LIMITE_TEMPERATURA_BAIXA_DEFAULT_dC;
static uint8_t soprador_ligado = 0;
static uint8_t alarme_temperatura_alta = 0;
static uint8_t alarme_temperatura_baixa = 0;
static uint8_t alarme_sensor_temperatura = 0;
static uint8_t alarme_sensor_umidade = 0;
static uint8_t alarme_alimentacao = 0;
static uint8_t umidade_fora_faixa = 0;
static uint8_t adc_erro = 0;
static uint32_t zur_umidade = 6950U;
static uint32_t gur_umidade = 314U;
static uint16_t datalog_periodo_s = DATALOG_PERIODO_DEFAULT_s;
static uint16_t datalog_timer_s = DATALOG_PERIODO_DEFAULT_s;
static uint8_t datalog_ok = 0;
static uint8_t datalog_reset_confirm = 0;
static uint32_t rtc_data_ddmmaa = 10126U;
static uint32_t rtc_hora_hhmmss = 0U;
static uint8_t temperatura_desconectada = 1;
static uint8_t umidade_desconectada = 1;

static uint8_t modo = 0;
static uint16_t buzzer_periodo_timeout = 0;
static uint8_t buzzer_ligado_timeout = 0;
static uint16_t tac_relogio_timeout = 0;
static uint8_t leitura_analogica_20ms = 0;
static uint8_t analog_amostras_validas = 0;
static uint8_t analog_media_index = 0;
static uint16_t diag_timeout_50ms = 0;
static uint8_t btn_max_clicks_diag = 0;
static uint8_t btn_max_solto_diag = 1;
static uint16_t tempo_baixa_timeout = TEMPO_ESTABILIZACAO_BAIXA_50MS;
static uint16_t rele_comutacao_timeout = 0;
static uint8_t menu_config_item = 0;
static uint8_t menu_editando = 0;
static uint8_t menu_bloqueia_primeira_soltura = 0;
static uint8_t menu_rtc_campo = 0;
static uint8_t menu_rtc_visualiza_timeout = 0;
static uint16_t analog_buffer_umidade[8] = {0};
static uint16_t analog_buffer_temp[8] = {0};
static uint16_t analog_buffer_temp_ref[8] = {0};
static uint16_t analog_buffer_5v[8] = {0};
static uint16_t analog_buffer_12v[8] = {0};
static uint32_t analog_soma_umidade = 0;
static uint32_t analog_soma_temp = 0;
static uint32_t analog_soma_temp_ref = 0;
static uint32_t analog_soma_5v = 0;
static uint32_t analog_soma_12v = 0;
static datalog_header_t datalog_header = {0};

static volatile uint8_t serial_rx_buffer[SERIAL_RX_BUFFER_SIZE];
static volatile uint16_t serial_rx_posicao_fifo = 0;
static volatile uint16_t serial_rx_posicao_leitura = 0;
static volatile uint8_t serial_rx_ultimo_byte = 0;
static volatile uint8_t serial_rx_penultimo_byte = 0;
static volatile uint8_t serial_rx_overflow = 0;
static volatile uint8_t serial_rx_timeout_ms = 0;
static volatile uint8_t serial_rx_error_isr = 0;
static char serial_cmd_buffer[128];
static uint8_t serial_log_enviando = 0;
static uint16_t serial_log_indice = 0;
static uint16_t serial_log_restante = 0;
static uint8_t serial_log_pausa_50ms = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_USART1_Init(void);
static void MX_USART6_Init(void);
/* USER CODE BEGIN PFP */
static void splash_screen(uint16_t tempo_ms, uint8_t d5, uint8_t d4,
                          uint8_t d3, uint8_t d2, uint8_t d1, uint8_t d0);
static void startup_splash_sequence(void);
static void processa_tick_50ms(void);
static void adc1_init(void);
static uint16_t adc1_read_channel(uint32_t channel);
static void atualiza_medicoes_analogicas(void);
static void atualiza_display_tensoes_diagnostico(void);
static void trata_clicks_diagnostico(void);
static void trata_menu_secagem_press(void);
static void controle_buzzer(uint8_t alarme_ativo);
static void controle_soprador_e_alarmes(void);
static void salva_config_secagem_flash(void);
static int32_t menu_get_valor_atual(void);
static void menu_set_valor_atual(int32_t valor);
static uint8_t menu_item_rtc(void);
static uint8_t menu_rtc_qtd_campos(void);
static void menu_rtc_ajusta_campo(int8_t delta);
static void menu_display_lista(void);
static void menu_display_edicao(void);
static void menu_display_rtc_edicao(uint32_t valor);
static uint32_t checksum_bytes_local(const void *data, uint32_t size);
static uint32_t datalog_header_crc(const datalog_header_t *header);
static uint32_t datalog_record_crc(const datalog_record_t *record);
static uint8_t ext_eeprom_wait_ready(uint32_t timeout_ms);
static uint8_t ext_eeprom_read(uint16_t addr, void *data, uint16_t size);
static uint8_t ext_eeprom_write(uint16_t addr, const void *data, uint16_t size);
static uint16_t datalog_record_addr(uint16_t index);
static uint16_t datalog_expected_capacity(void);
static void datalog_format_header(void);
static void datalog_init(void);
static void datalog_reset(void);
static void datalog_tick_1s(void);
static void datalog_grava_amostra(void);
static uint32_t datalog_get_timestamp(void);
static void rtc_carrega_compacto(void);
static uint8_t rtc_aplica_compacto(uint32_t data_ddmmaa, uint32_t hora_hhmmss);
static void modo_secagem(void);
static void modo_menu_checkpoint(void);
static void modo_diagnostico(void);
static void prepara_dados_eeprom(void);
static void carrega_config(void);
static void serial_init_rx(void);
static void serial_send_text(const char *text);
static void serial_process(void);
static void serial_handle_command(const char *cmd);
static void serial_send_packet(uint8_t cmd, const char *payload);
static uint8_t serial_apply_config_list(const char *payload);
static uint8_t serial_apply_config_command(const char *cmd);
static uint8_t serial_parse_decimal_field(const char **text, uint32_t *value);
static uint8_t serial_parse_log_command(const char *cmd, uint32_t *indice,
                                        uint32_t *quantidade);
static void serial_log_start(uint16_t indice, uint16_t quantidade);
static void serial_log_process(void);
static uint8_t serial_log_send_record(uint16_t indice);
static uint16_t crc16_ibm(const uint8_t *data, uint16_t len);
static int8_t serial_hex_nibble(char c);
static uint8_t serial_parse_hex_u8(const char *text, uint8_t *value);
static uint8_t serial_parse_hex_u16(const char *text, uint16_t *value);
static uint8_t serial_parse_u32_strict(const char *text, uint32_t *value);
static uint8_t serial_text_is_digits(const char *text, uint8_t expected_len);
static uint8_t rtc_days_in_month(uint32_t month, uint32_t year);
static uint16_t serial_get_datalog_total(void);

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
  adc1_init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_USART1_Init();
  MX_USART6_Init();
  /* USER CODE BEGIN 2 */

  eeprom_init();
  carrega_config();
  datalog_init();
  rtc_carrega_compacto();
  serial_init_rx();
  serial_send_text("UMMI SECAGEM FUMO\r\n");
  startup_splash_sequence();
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
    if (timer_flag_1ms) {
      timer_flag_1ms = 0;
    }
    if (timer_flag_10ms) {
      timer_flag_10ms = 0;
      if (++leitura_analogica_20ms >= 2U) {
        leitura_analogica_20ms = 0;
        atualiza_medicoes_analogicas();
      }
    }
    if (timer_flag_1000ms) {
      timer_flag_1000ms = 0;
      datalog_tick_1s();
    }
    serial_process();
    serial_log_process();

    if (modo == 0) {
      modo_secagem();
    } else if (modo == 1) {
      modo_menu_checkpoint();
    } else if (modo == 3) {
      modo_diagnostico();
    } else {
      modo = 0;
    }

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
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  husart1.Instance = USART1;
  husart1.Init.BaudRate = 115200;
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
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
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
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

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

  /*Configure GPIO pins : LED_STATUS4_Pin PC4_PIN_DISPLAY_D_Pin PC5_PIN_DISPLAY_E_Pin PC6_DISPLAY_UNIDADE_Pin
                           PC7_PIN_SAIDA_DIGITAL1_Pin PC9_PIN_SAIDA_DIGITAL2_Pin LED_STATUS1_Pin LED_STATUS2_Pin
                           LED_STATUS3_Pin */
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

  /*Configure GPIO pins : PC1_PIN_INPUT_MEMBRANA_01_Pin PC2_PIN_INPUT_MEMBRANA_02_Pin PC3_PIN_INPUT_MEMBRANA_03_Pin */
  GPIO_InitStruct.Pin = PC1_PIN_INPUT_MEMBRANA_01_Pin|PC2_PIN_INPUT_MEMBRANA_02_Pin|PC3_PIN_INPUT_MEMBRANA_03_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : EA_UMIDADE_Pin EA_TEMPERATURA_Pin EA_TEMP_REF_GND_Pin EA_5V_Pin */
  GPIO_InitStruct.Pin = EA_UMIDADE_Pin|EA_TEMPERATURA_Pin|EA_TEMP_REF_GND_Pin|EA_5V_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA5_PIN_DISPLAY_A_Pin PA6_PIN_DISPLAY_B_Pin PA7_PIN_DISPLAY_C_Pin */
  GPIO_InitStruct.Pin = PA5_PIN_DISPLAY_A_Pin|PA6_PIN_DISPLAY_B_Pin|PA7_PIN_DISPLAY_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0_PIN_DISPLAY_F_Pin PB1_PIN_DISPLAY_G_Pin PB2_PIN_DISPLAY_PD_Pin PB10_DISPLAY_CENTENA_MILHAR_Pin
                           PB12_DISPLAY_DEZENA_MILHAR_Pin PB13_DISPLAY_UNIDADE_MILHAR_Pin PB14_DISPLAY_CENTENA_Pin PB15_DISPLAY_DEZENA_Pin
                           SD_BUZZER_Pin */
  GPIO_InitStruct.Pin = PB0_PIN_DISPLAY_F_Pin|PB1_PIN_DISPLAY_G_Pin|PB2_PIN_DISPLAY_PD_Pin|PB10_DISPLAY_CENTENA_MILHAR_Pin
                          |PB12_DISPLAY_DEZENA_MILHAR_Pin|PB13_DISPLAY_UNIDADE_MILHAR_Pin|PB14_DISPLAY_CENTENA_Pin|PB15_DISPLAY_DEZENA_Pin
                          |SD_BUZZER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB8_PIN_ENTRADA_01_Pin PB9_PIN_ENTRADA_02_Pin */
  GPIO_InitStruct.Pin = PB8_PIN_ENTRADA_01_Pin|PB9_PIN_ENTRADA_02_Pin;
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

static void startup_splash_sequence(void) {
  const uint16_t etapa_50ms = 24U;
  const uint32_t etapa_ms = 1200U;

  splash_screen(etapa_50ms, DSP_B, DSP_E, DSP_1, DSP_OFF, DSP_1, DSP_0);
  display_set_decimal_points((uint8_t)etapa_50ms, 0x02);
  HAL_Delay(etapa_ms);

  display_set_decimal_points(2, 0);
  display_atualiza(rtc_data_ddmmaa);
  splash_timeout = 0;
  HAL_Delay(etapa_ms);

  display_atualiza(rtc_hora_hhmmss);
  HAL_Delay(etapa_ms);
}

static void adc1_init(void) {
  __HAL_RCC_ADC1_CLK_ENABLE();

  ADC->CCR = 0;
  ADC1->CR1 = 0;
  ADC1->CR2 = 0;
  ADC1->SMPR1 = ADC_SMPR1_SMP10_2 | ADC_SMPR1_SMP10_1 | ADC_SMPR1_SMP10_0;
  ADC1->SMPR2 = ADC_SMPR2_SMP1_2 | ADC_SMPR2_SMP1_1 | ADC_SMPR2_SMP1_0 |
                 ADC_SMPR2_SMP2_2 | ADC_SMPR2_SMP2_1 | ADC_SMPR2_SMP2_0 |
                 ADC_SMPR2_SMP3_2 | ADC_SMPR2_SMP3_1 | ADC_SMPR2_SMP3_0 |
                 ADC_SMPR2_SMP4_2 | ADC_SMPR2_SMP4_1 | ADC_SMPR2_SMP4_0;
  ADC1->SQR1 = 0;
  ADC1->CR2 = ADC_CR2_ADON;
}

static uint16_t adc1_read_channel(uint32_t channel) {
  uint32_t timeout = 10000U;

  ADC1->SQR3 = channel & ADC_SQR3_SQ1;
  ADC1->SR = 0;
  ADC1->CR2 |= ADC_CR2_SWSTART;

  while (((ADC1->SR & ADC_SR_EOC) == 0U) && timeout > 0U) {
    timeout--;
  }

  if (timeout == 0U) {
    adc_erro = 1;
    return 0;
  }
  return (uint16_t)ADC1->DR;
}

static void atualiza_medicoes_analogicas(void) {
  adc_erro = 0;
  uint16_t adc_umidade = adc1_read_channel(1U);
  uint16_t adc_temp = adc1_read_channel(2U);
  uint16_t adc_temp_ref = adc1_read_channel(3U);
  uint16_t adc_5v = adc1_read_channel(4U);
  uint16_t adc_12v = adc1_read_channel(10U);
  uint16_t media_umidade = 0;
  uint16_t media_temp = 0;
  uint16_t media_temp_ref = 0;
  uint16_t media_5v = 0;
  uint16_t media_12v = 0;
  uint32_t adc_temp_dif = 0;

  analog_soma_umidade -= analog_buffer_umidade[analog_media_index];
  analog_soma_temp -= analog_buffer_temp[analog_media_index];
  analog_soma_temp_ref -= analog_buffer_temp_ref[analog_media_index];
  analog_soma_5v -= analog_buffer_5v[analog_media_index];
  analog_soma_12v -= analog_buffer_12v[analog_media_index];

  analog_buffer_umidade[analog_media_index] = adc_umidade;
  analog_buffer_temp[analog_media_index] = adc_temp;
  analog_buffer_temp_ref[analog_media_index] = adc_temp_ref;
  analog_buffer_5v[analog_media_index] = adc_5v;
  analog_buffer_12v[analog_media_index] = adc_12v;

  analog_soma_umidade += adc_umidade;
  analog_soma_temp += adc_temp;
  analog_soma_temp_ref += adc_temp_ref;
  analog_soma_5v += adc_5v;
  analog_soma_12v += adc_12v;

  if (++analog_media_index >= ADC_MEDIA_QTD_AMOSTRAS) {
    analog_media_index = 0;
  }
  if (analog_amostras_validas < ADC_MEDIA_QTD_AMOSTRAS) {
    analog_amostras_validas++;
  }

  media_umidade = (uint16_t)(analog_soma_umidade >> ADC_MEDIA_SHIFT);
  media_temp = (uint16_t)(analog_soma_temp >> ADC_MEDIA_SHIFT);
  media_temp_ref = (uint16_t)(analog_soma_temp_ref >> ADC_MEDIA_SHIFT);
  media_5v = (uint16_t)(analog_soma_5v >> ADC_MEDIA_SHIFT);
  media_12v = (uint16_t)(analog_soma_12v >> ADC_MEDIA_SHIFT);

  uint32_t umidade_mV = ((uint32_t)media_umidade * ADC_VREF_MV) / ADC_FULL_SCALE;
  uint32_t temp_mV = 0;
  umidade_fora_faixa = (umidade_mV < (UMIDADE_ZERO_MV - 100U)) ||
                       (umidade_mV > (UMIDADE_FUNDO_MV + 100U));

  if (media_temp > media_temp_ref) {
    adc_temp_dif = (uint32_t)media_temp - media_temp_ref;
  }
  temp_mV = (adc_temp_dif * ADC_VREF_MV) / ADC_FULL_SCALE;

  if (analog_amostras_validas >= ADC_MIN_AMOSTRAS_STATUS) {
    umidade_desconectada = (umidade_mV < ADC_UMIDADE_DESCONECTADO_mV);
    temperatura_desconectada = (adc_temp_dif < ADC_TEMPERATURA_DESCONECTADO_AD);
  }

  if (!temperatura_desconectada) {
    temperatura_dC = (int32_t)temp_mV;
  }

  if (umidade_mV <= UMIDADE_ZERO_MV) {
    umidade_dUR = 0;
  } else if (umidade_mV >= UMIDADE_FUNDO_MV) {
    umidade_dUR = 1000;
  } else {
    int32_t umidade_calc =
        (((int32_t)umidade_mV * (int32_t)gur_umidade) -
         (int32_t)zur_umidade);

    if (umidade_calc < 0) {
      umidade_calc = 0;
    }
    umidade_calc /= 100;
    if (umidade_calc > 1000) {
      umidade_calc = 1000;
    }
    umidade_dUR = (uint16_t)umidade_calc;
  }

  tensao_5v_mV = (uint16_t)((((uint32_t)media_5v * ADC_VREF_MV) /
                             ADC_FULL_SCALE) *
                            2800U / 1800U);
  tensao_12v_mV = (uint16_t)((((uint32_t)media_12v * ADC_VREF_MV) /
                              ADC_FULL_SCALE) *
                             13300U / 3300U);
}

static void controle_buzzer(uint8_t alarme_ativo) {
  if (!alarme_ativo) {
    buzzer_periodo_timeout = 0;
    buzzer_ligado_timeout = 0;
    HAL_GPIO_WritePin(SD_BUZZER_GPIO_Port, SD_BUZZER_Pin, LOW);
    return;
  }

  if (buzzer_periodo_timeout == 0U && buzzer_ligado_timeout == 0U) {
    buzzer_ligado_timeout = TEMPO_BUZZER_ON_50MS;
    buzzer_periodo_timeout = TEMPO_BUZZER_ON_50MS + TEMPO_BUZZER_OFF_50MS;
  }

  HAL_GPIO_WritePin(SD_BUZZER_GPIO_Port, SD_BUZZER_Pin,
                    (buzzer_ligado_timeout > 0U) ? HIGH : LOW);
}

static void controle_soprador_e_alarmes(void) {
  uint8_t falha_bloqueante = 0;
  uint8_t alarme_ativo = 0;

  alarme_sensor_temperatura = adc_erro || temperatura_desconectada ||
                              analog_amostras_validas < ADC_MIN_AMOSTRAS_STATUS ||
                              temperatura_dC < 0 ||
                              temperatura_dC > 1200;
  alarme_sensor_umidade = umidade_desconectada || umidade_fora_faixa;
  alarme_alimentacao = (tensao_5v_mV < TENSAO_5V_MIN_mV) ||
                       (tensao_5v_mV > TENSAO_5V_MAX_mV) ||
                       (tensao_12v_mV < TENSAO_12V_MIN_mV) ||
                       (tensao_12v_mV > TENSAO_12V_MAX_mV);
  alarme_temperatura_alta =
      (!alarme_sensor_temperatura &&
       temperatura_dC >= limite_temperatura_alta_dC);

  if (alarme_sensor_temperatura || alarme_alimentacao) {
    falha_bloqueante = 1;
  }

  if (alarme_sensor_temperatura || temperatura_dC > limite_temperatura_baixa_dC) {
    tempo_baixa_timeout = TEMPO_ESTABILIZACAO_BAIXA_50MS;
    alarme_temperatura_baixa = 0;
  } else if (tempo_baixa_timeout == 0U) {
    alarme_temperatura_baixa = 1;
  }

  if (falha_bloqueante) {
    soprador_ligado = 0;
  } else if (rele_comutacao_timeout == 0U) {
    if (soprador_ligado && temperatura_dC >= setpoint_temperatura_dC) {
      soprador_ligado = 0;
      rele_comutacao_timeout = TEMPO_MINIMO_COMUTACAO_RELE_50MS;
    } else if (!soprador_ligado &&
               temperatura_dC <
                   (setpoint_temperatura_dC - histerese_temperatura_dC)) {
      soprador_ligado = 1;
      rele_comutacao_timeout = TEMPO_MINIMO_COMUTACAO_RELE_50MS;
    }
  }

  HAL_GPIO_WritePin(SAIDA_01_PORT, SAIDA_01, soprador_ligado ? HIGH : LOW);
  HAL_GPIO_WritePin(SAIDA_02_PORT, SAIDA_02, LOW);

  alarme_ativo = alarme_temperatura_alta || alarme_temperatura_baixa ||
                 alarme_sensor_temperatura || alarme_sensor_umidade ||
                 alarme_alimentacao;
  controle_buzzer(alarme_ativo);
}

static void atualiza_display_principal(void) {
  uint32_t umidade = umidade_dUR;
  uint32_t temperatura = (temperatura_dC < 0) ? 0U : (uint32_t)temperatura_dC;

  if (analog_amostras_validas < ADC_MIN_AMOSTRAS_STATUS) {
    display_set_digits(DSP_MINUS, DSP_MINUS, DSP_MINUS, DSP_MINUS, DSP_MINUS,
                       DSP_MINUS);
    display_set_decimal_points(2, 0);
    return;
  }

  if (umidade_desconectada || umidade_fora_faixa ||
      temperatura_desconectada) {
    uint8_t umidade_invalida = umidade_desconectada || umidade_fora_faixa;

    display_set_digits(umidade_invalida ? DSP_MINUS : ((umidade / 100U) % 10U),
                       umidade_invalida ? DSP_MINUS : ((umidade / 10U) % 10U),
                       umidade_invalida ? DSP_MINUS : (umidade % 10U),
                       temperatura_desconectada ? DSP_MINUS
                                                 : ((temperatura / 100U) % 10U),
                       temperatura_desconectada ? DSP_MINUS
                                                 : ((temperatura / 10U) % 10U),
                       temperatura_desconectada ? DSP_MINUS
                                                 : (temperatura % 10U));
    display_set_decimal_points(2, 0);
    return;
  }

  if (umidade > 999U) {
    umidade = 999U;
  }
  if (temperatura > 999U) {
    temperatura = 999U;
  }

  display_set_digits((umidade / 100U) % 10U, (umidade / 10U) % 10U,
                     umidade % 10U, (temperatura / 100U) % 10U,
                     (temperatura / 10U) % 10U, temperatura % 10U);
  display_set_decimal_points(2, 0);
}

static void atualiza_display_tensoes_diagnostico(void) {
  uint32_t valor_12v_dV = ((uint32_t)tensao_12v_mV + 50U) / 100U;
  uint32_t valor_5v_dV = ((uint32_t)tensao_5v_mV + 50U) / 100U;

  if (valor_12v_dV > 999U) {
    valor_12v_dV = 999U;
  }
  if (valor_5v_dV > 999U) {
    valor_5v_dV = 999U;
  }

  display_set_digits((valor_12v_dV / 100U) % 10U,
                     (valor_12v_dV / 10U) % 10U, valor_12v_dV % 10U,
                     (valor_5v_dV / 100U) % 10U,
                     (valor_5v_dV / 10U) % 10U, valor_5v_dV % 10U);
  display_set_decimal_points(2, 0x12);
}

static void trata_clicks_diagnostico(void) {
  if (!btn_max_status) {
    btn_max_solto_diag = 1;
    return;
  }

  if (btn_max_solto_diag) {
    btn_max_solto_diag = 0;
    if (btn_max_clicks_diag < 5U) {
      btn_max_clicks_diag++;
    }
    if (btn_max_clicks_diag >= 5U) {
      btn_max_clicks_diag = 0;
      diag_timeout_50ms = 600;
      modo = 3;
      splash_screen(10, DSP_D, DSP_I, DSP_A, DSP_G, DSP_OFF, DSP_OFF);
    }
  }
}

static void trata_menu_secagem_press(void) {
  static uint8_t enter_pendente = 0;

  if (btn_relogio_status && tac_relogio_timeout >= 60U) {
    enter_pendente = 1;
    splash_screen(4, DSP_S, DSP_E, DSP_T, DSP_U, DSP_P, DSP_OFF);
    return;
  }

  if (btn_relogio_status || !enter_pendente) {
    return;
  }

  enter_pendente = 0;
  menu_config_item = 0;
  menu_editando = 0;
  menu_bloqueia_primeira_soltura = 1;
  modo = 1;
  splash_timeout = 0;
  menu_display_lista();
}

static void salva_config_secagem_flash(void) {
  dados.assinatura = EEPROM_ASSINATURA_APP;
  dados.setpoint_01 = (uint32_t)setpoint_temperatura_dC;
  dados.histerese_temperatura_dC = (uint32_t)histerese_temperatura_dC;
  dados.limite_temperatura_alta_dC = (uint32_t)limite_temperatura_alta_dC;
  dados.limite_temperatura_baixa_dC = (uint32_t)limite_temperatura_baixa_dC;
  dados.zur_umidade = zur_umidade;
  dados.gur_umidade = gur_umidade;
  dados.datalog_periodo_s = datalog_periodo_s;
  eeprom_write_config(&dados);
}

static uint32_t checksum_bytes_local(const void *data, uint32_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t hash = 2166136261UL;

  for (uint32_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= 16777619UL;
  }

  return hash;
}

static uint32_t datalog_header_crc(const datalog_header_t *header) {
  return checksum_bytes_local(header, sizeof(*header) - sizeof(header->crc));
}

static uint32_t datalog_record_crc(const datalog_record_t *record) {
  return checksum_bytes_local(record, sizeof(*record) - sizeof(record->crc));
}

static uint8_t ext_eeprom_wait_ready(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();

  do {
    if (HAL_I2C_IsDeviceReady(&hi2c1, EXT_EEPROM_ADDR, 1, 2) == HAL_OK) {
      return 1;
    }
  } while ((HAL_GetTick() - start) < timeout_ms);

  return 0;
}

static uint8_t ext_eeprom_read(uint16_t addr, void *data, uint16_t size) {
  if (!ext_eeprom_wait_ready(EXT_EEPROM_TIMEOUT_MS)) {
    return 0;
  }

  return HAL_I2C_Mem_Read(&hi2c1, EXT_EEPROM_ADDR, addr,
                          I2C_MEMADD_SIZE_16BIT, (uint8_t *)data, size,
                          EXT_EEPROM_TIMEOUT_MS) == HAL_OK;
}

static uint8_t ext_eeprom_write(uint16_t addr, const void *data, uint16_t size) {
  const uint8_t *bytes = (const uint8_t *)data;

  while (size > 0U) {
    uint16_t page_offset = addr % EXT_EEPROM_PAGE_SIZE;
    uint16_t page_space = EXT_EEPROM_PAGE_SIZE - page_offset;
    uint16_t chunk = (size < page_space) ? size : page_space;

    if (HAL_I2C_Mem_Write(&hi2c1, EXT_EEPROM_ADDR, addr, I2C_MEMADD_SIZE_16BIT,
                          (uint8_t *)bytes, chunk,
                          EXT_EEPROM_TIMEOUT_MS) != HAL_OK) {
      return 0;
    }
    if (!ext_eeprom_wait_ready(EXT_EEPROM_TIMEOUT_MS)) {
      return 0;
    }

    addr += chunk;
    bytes += chunk;
    size -= chunk;
  }

  return 1;
}

static uint16_t datalog_record_addr(uint16_t index) {
  return (uint16_t)(DATALOG_RECORDS_ADDR +
                    (index * sizeof(datalog_record_t)));
}

static uint16_t datalog_expected_capacity(void) {
  if (DATALOG_RECORDS_ADDR >= EXT_EEPROM_SIZE_BYTES) {
    return 0;
  }
  return (uint16_t)((EXT_EEPROM_SIZE_BYTES - DATALOG_RECORDS_ADDR) /
                    sizeof(datalog_record_t));
}

static void datalog_format_header(void) {
  uint16_t capacidade = datalog_expected_capacity();

  memset(&datalog_header, 0, sizeof(datalog_header));
  datalog_header.assinatura = DATALOG_ASSINATURA;
  datalog_header.versao = DATALOG_VERSAO;
  datalog_header.record_size = sizeof(datalog_record_t);
  datalog_header.capacidade = capacidade;
  datalog_header.proximo_indice = 0;
  datalog_header.quantidade_valida = 0;
  datalog_header.seq_global = 1;
  datalog_header.crc = datalog_header_crc(&datalog_header);
}

static void datalog_init(void) {
  datalog_ok = 0;

  if (!ext_eeprom_read(DATALOG_BASE_ADDR, &datalog_header,
                       sizeof(datalog_header))) {
    return;
  }

  if (datalog_header.assinatura != DATALOG_ASSINATURA ||
      datalog_header.versao != DATALOG_VERSAO ||
      datalog_header.record_size != sizeof(datalog_record_t) ||
      datalog_header.capacidade == 0U ||
      datalog_header.capacidade != datalog_expected_capacity() ||
      datalog_header.crc != datalog_header_crc(&datalog_header)) {
    datalog_reset();
    return;
  }

  if (datalog_header.proximo_indice >= datalog_header.capacidade) {
    datalog_header.proximo_indice = 0;
  }
  if (datalog_header.quantidade_valida > datalog_header.capacidade) {
    datalog_header.quantidade_valida = datalog_header.capacidade;
  }

  datalog_ok = 1;
}

static void datalog_reset(void) {
  datalog_format_header();
  datalog_ok = ext_eeprom_write(DATALOG_BASE_ADDR, &datalog_header,
                                sizeof(datalog_header));
  datalog_timer_s = datalog_periodo_s;
}

static uint32_t datalog_get_timestamp(void) {
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};

  if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK) {
    return HAL_GetTick() / 1000U;
  }
  if (HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
    return HAL_GetTick() / 1000U;
  }

  return ((uint32_t)date.Year << 26) | ((uint32_t)date.Month << 22) |
         ((uint32_t)date.Date << 17) | ((uint32_t)time.Hours << 12) |
         ((uint32_t)time.Minutes << 6) | time.Seconds;
}

static void rtc_carrega_compacto(void) {
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};

  if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK ||
      HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
    return;
  }

  rtc_data_ddmmaa = ((uint32_t)date.Date * 10000U) +
                    ((uint32_t)date.Month * 100U) + date.Year;
  rtc_hora_hhmmss = ((uint32_t)time.Hours * 10000U) +
                    ((uint32_t)time.Minutes * 100U) + time.Seconds;
}

static uint8_t rtc_days_in_month(uint32_t month, uint32_t year) {
  static const uint8_t days_by_month[12] = {
      31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};
  uint32_t full_year = 2000U + year;

  if (month < 1U || month > 12U) {
    return 0;
  }
  if (month == 2U &&
      ((full_year % 4U == 0U && full_year % 100U != 0U) ||
       (full_year % 400U == 0U))) {
    return 29U;
  }
  return days_by_month[month - 1U];
}

static uint8_t rtc_aplica_compacto(uint32_t data_ddmmaa, uint32_t hora_hhmmss) {
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};
  uint32_t dia = data_ddmmaa / 10000U;
  uint32_t mes = (data_ddmmaa / 100U) % 100U;
  uint32_t ano = data_ddmmaa % 100U;
  uint32_t hora = hora_hhmmss / 10000U;
  uint32_t minuto = (hora_hhmmss / 100U) % 100U;
  uint32_t segundo = hora_hhmmss % 100U;

  if (dia < 1U || mes < 1U || mes > 12U ||
      dia > rtc_days_in_month(mes, ano) || ano > 99U ||
      hora > 23U || minuto > 59U || segundo > 59U) {
    return 0;
  }

  time.Hours = (uint8_t)hora;
  time.Minutes = (uint8_t)minuto;
  time.Seconds = (uint8_t)segundo;
  date.Date = (uint8_t)dia;
  date.Month = (uint8_t)mes;
  date.Year = (uint8_t)ano;
  date.WeekDay = RTC_WEEKDAY_MONDAY;

  if (HAL_RTC_SetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK ||
      HAL_RTC_SetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
    return 0;
  }

  rtc_data_ddmmaa = data_ddmmaa;
  rtc_hora_hhmmss = hora_hhmmss;
  return 1;
}

static void datalog_grava_amostra(void) {
  datalog_record_t record = {0};

  if (!datalog_ok || analog_amostras_validas < ADC_MEDIA_QTD_AMOSTRAS) {
    return;
  }

  record.timestamp = datalog_get_timestamp();
  record.temperatura_dC = (int16_t)temperatura_dC;
  record.umidade_dUR = umidade_dUR;
  record.flags = (soprador_ligado ? 0x0001U : 0x0000U) |
                 (HAL_GPIO_ReadPin(SAIDA_02_PORT, SAIDA_02) ? 0x0002U
                                                            : 0x0000U) |
                 (temperatura_desconectada ? 0x0010U : 0x0000U) |
                 (umidade_desconectada ? 0x0020U : 0x0000U) |
                 (umidade_fora_faixa ? 0x0040U : 0x0000U) |
                 (alarme_temperatura_alta ? 0x0100U : 0x0000U) |
                 (alarme_temperatura_baixa ? 0x0200U : 0x0000U) |
                 (alarme_sensor_temperatura ? 0x0400U : 0x0000U) |
                 (alarme_sensor_umidade ? 0x0800U : 0x0000U) |
                 (alarme_alimentacao ? 0x1000U : 0x0000U);
  record.crc = datalog_record_crc(&record);

  if (!ext_eeprom_write(datalog_record_addr(datalog_header.proximo_indice),
                        &record, sizeof(record))) {
    datalog_ok = 0;
    return;
  }

  datalog_header.seq_global++;
  datalog_header.proximo_indice++;
  if (datalog_header.proximo_indice >= datalog_header.capacidade) {
    datalog_header.proximo_indice = 0;
  }
  if (datalog_header.quantidade_valida < datalog_header.capacidade) {
    datalog_header.quantidade_valida++;
  }
  datalog_header.crc = datalog_header_crc(&datalog_header);

  if (!ext_eeprom_write(DATALOG_BASE_ADDR, &datalog_header,
                        sizeof(datalog_header))) {
    datalog_ok = 0;
  }
}

static void datalog_tick_1s(void) {
  if (datalog_periodo_s == 0U) {
    return;
  }

  if (datalog_timer_s > 0U) {
    datalog_timer_s--;
  }
  if (datalog_timer_s == 0U) {
    datalog_grava_amostra();
    datalog_timer_s = datalog_periodo_s;
  }
}

static int32_t menu_get_valor_atual(void) {
  switch (menu_config_item) {
  case 0:
    return setpoint_temperatura_dC;
  case 1:
    return histerese_temperatura_dC;
  case 2:
    return limite_temperatura_alta_dC;
  case 3:
    return limite_temperatura_baixa_dC;
  case 4:
    return (int32_t)zur_umidade;
  case 5:
    return (int32_t)gur_umidade;
  case 6:
    return datalog_reset_confirm;
  case 7:
    return datalog_periodo_s;
  case 8:
    return (int32_t)rtc_data_ddmmaa;
  case 9:
    return (int32_t)rtc_hora_hhmmss;
  default:
    return 0;
  }
}

static void menu_set_valor_atual(int32_t valor) {
  if (valor < 0) {
    valor = 0;
  }
  if (menu_config_item < 4U && valor > 999) {
    valor = 999;
  } else if (menu_config_item >= 8U && valor > 999999) {
    valor = 999999;
  }

  switch (menu_config_item) {
  case 0:
    setpoint_temperatura_dC = valor;
    break;
  case 1:
    if (valor < 1) {
      valor = 1;
    }
    if (valor > 200) {
      valor = 200;
    }
    histerese_temperatura_dC = valor;
    break;
  case 2:
    if (valor < limite_temperatura_baixa_dC) {
      valor = limite_temperatura_baixa_dC;
    }
    limite_temperatura_alta_dC = valor;
    break;
  case 3:
    if (valor > limite_temperatura_alta_dC) {
      valor = limite_temperatura_alta_dC;
    }
    limite_temperatura_baixa_dC = valor;
    break;
  case 4:
    if (valor > 65535) {
      valor = 65535;
    }
    zur_umidade = (uint32_t)valor;
    break;
  case 5:
    if (valor < 1) {
      valor = 1;
    }
    if (valor > 65535) {
      valor = 65535;
    }
    gur_umidade = (uint32_t)valor;
    break;
  case 6:
    datalog_reset_confirm = (valor > 0) ? 1U : 0U;
    break;
  case 7:
    if (valor < 10) {
      valor = 10;
    }
    if (valor > 3600) {
      valor = 3600;
    }
    datalog_periodo_s = (uint16_t)valor;
    if (datalog_timer_s > datalog_periodo_s) {
      datalog_timer_s = datalog_periodo_s;
    }
    break;
  case 8:
    rtc_data_ddmmaa = (uint32_t)valor;
    break;
  case 9:
    valor = (valor / 100) * 100;
    rtc_hora_hhmmss = (uint32_t)valor;
    break;
  default:
    break;
  }
}

static uint8_t menu_item_rtc(void) {
  return (menu_config_item == 8U || menu_config_item == 9U);
}

static uint8_t menu_rtc_qtd_campos(void) {
  return (menu_config_item == 8U) ? 3U : 2U;
}

static uint32_t menu_ajusta_circular(uint32_t valor, uint32_t min,
                                     uint32_t max, int8_t delta) {
  if (valor < min || valor > max) {
    valor = min;
  }
  if (delta > 0) {
    return (valor >= max) ? min : (valor + 1U);
  }
  return (valor <= min) ? max : (valor - 1U);
}

static void menu_rtc_ajusta_campo(int8_t delta) {
  if (menu_config_item == 8U) {
    uint32_t dia = rtc_data_ddmmaa / 10000U;
    uint32_t mes = (rtc_data_ddmmaa / 100U) % 100U;
    uint32_t ano = rtc_data_ddmmaa % 100U;

    if (mes < 1U || mes > 12U) {
      mes = 1U;
    }
    if (ano > 99U) {
      ano = 0U;
    }

    if (menu_rtc_campo == 0U) {
      dia = menu_ajusta_circular(dia, 1U, rtc_days_in_month(mes, ano), delta);
    } else if (menu_rtc_campo == 1U) {
      mes = menu_ajusta_circular(mes, 1U, 12U, delta);
      if (dia > rtc_days_in_month(mes, ano)) {
        dia = rtc_days_in_month(mes, ano);
      }
    } else {
      ano = menu_ajusta_circular(ano, 0U, 99U, delta);
      if (dia > rtc_days_in_month(mes, ano)) {
        dia = rtc_days_in_month(mes, ano);
      }
    }

    rtc_data_ddmmaa = (dia * 10000U) + (mes * 100U) + ano;
    return;
  }

  if (menu_config_item == 9U) {
    uint32_t hora = rtc_hora_hhmmss / 10000U;
    uint32_t minuto = (rtc_hora_hhmmss / 100U) % 100U;

    if (menu_rtc_campo == 0U) {
      hora = menu_ajusta_circular(hora, 0U, 23U, delta);
    } else {
      minuto = menu_ajusta_circular(minuto, 0U, 59U, delta);
    }

    rtc_hora_hhmmss = (hora * 10000U) + (minuto * 100U);
  }
}

static void menu_display_lista(void) {
  switch (menu_config_item) {
  case 0:
    display_set_digits(DSP_P, DSP_0, DSP_1, DSP_OFF, DSP_OFF, DSP_T);
    break;
  case 1:
    display_set_digits(DSP_P, DSP_0, DSP_2, DSP_H, DSP_I, DSP_S);
    break;
  case 2:
    display_set_digits(DSP_P, DSP_0, DSP_3, DSP_A, DSP_L, DSP_T);
    break;
  case 3:
    display_set_digits(DSP_P, DSP_0, DSP_4, DSP_B, DSP_A, DSP_I);
    break;
  case 4:
    display_set_digits(DSP_P, DSP_0, DSP_5, DSP_Z, DSP_U, DSP_R);
    break;
  case 5:
    display_set_digits(DSP_P, DSP_0, DSP_6, DSP_G, DSP_U, DSP_R);
    break;
  case 6:
    display_set_digits(DSP_P, DSP_0, DSP_7, DSP_L, DSP_O, DSP_G);
    break;
  case 7:
    display_set_digits(DSP_P, DSP_0, DSP_8, DSP_P, DSP_E, DSP_R);
    break;
  case 8:
    display_set_digits(DSP_P, DSP_0, DSP_9, DSP_D, DSP_A, DSP_T);
    break;
  case 9:
    display_set_digits(DSP_P, DSP_1, DSP_0, DSP_H, DSP_O, DSP_R);
    break;
  default:
    display_set_digits(DSP_P, DSP_0, DSP_1, DSP_OFF, DSP_OFF, DSP_T);
    break;
  }
  display_set_decimal_points(2, 0);
}

static void menu_display_rtc_edicao(uint32_t valor) {
  uint8_t digitos[6] = {
      (uint8_t)((valor / 100000U) % 10U), (uint8_t)((valor / 10000U) % 10U),
      (uint8_t)((valor / 1000U) % 10U),  (uint8_t)((valor / 100U) % 10U),
      (uint8_t)((valor / 10U) % 10U),    (uint8_t)(valor % 10U)};
  uint8_t apagar = 0U;
  uint8_t inicio = (uint8_t)(menu_rtc_campo * 2U);

  if (menu_rtc_visualiza_timeout == 0U) {
    apagar = ((HAL_GetTick() / 500U) & 1U) ? 1U : 0U;
  }

  if (apagar) {
    digitos[inicio] = DSP_OFF;
    digitos[inicio + 1U] = DSP_OFF;
  }

  display_set_digits(digitos[0], digitos[1], digitos[2], digitos[3],
                     digitos[4], digitos[5]);
  display_set_decimal_points(2, 0);
}

static void menu_display_edicao(void) {
  uint32_t valor = (uint32_t)menu_get_valor_atual();
  uint8_t l2 = DSP_OFF;
  uint8_t l1 = DSP_OFF;
  uint8_t l0 = DSP_OFF;

  switch (menu_config_item) {
  case 0:
    l2 = DSP_S;
    l1 = DSP_P;
    break;
  case 1:
    l2 = DSP_H;
    l1 = DSP_I;
    l0 = DSP_S;
    break;
  case 2:
    l2 = DSP_A;
    l1 = DSP_L;
    l0 = DSP_T;
    break;
  case 3:
    l2 = DSP_B;
    l1 = DSP_A;
    l0 = DSP_I;
    break;
  case 4:
    l2 = DSP_Z;
    l1 = DSP_U;
    l0 = DSP_R;
    break;
  case 5:
    l2 = DSP_G;
    l1 = DSP_U;
    l0 = DSP_R;
    break;
  case 6:
    l2 = DSP_R;
    l1 = DSP_S;
    l0 = DSP_T;
    break;
  case 7:
    l2 = DSP_P;
    l1 = DSP_E;
    l0 = DSP_R;
    break;
  case 8:
  case 9:
    menu_display_rtc_edicao(valor);
    return;
  default:
    break;
  }

  if (menu_config_item >= 4U && menu_config_item <= 5U) {
    if (valor > 999999U) {
      valor = 999999U;
    }
    display_atualiza(valor);
    display_set_decimal_points(2, 0);
    return;
  }

  display_set_digits(l2, l1, l0, (valor / 100U) % 10U,
                     (valor / 10U) % 10U, valor % 10U);
  display_set_decimal_points(2, (menu_config_item <= 3U) ? 0x02 : 0);
}

static void modo_secagem(void) {
  controle_soprador_e_alarmes();

  atualiza_display_principal();

  trata_clicks_diagnostico();
  trata_menu_secagem_press();
}

static void modo_menu_checkpoint(void) {
  static uint8_t relogio_anterior = 0;
  static uint8_t sair_pendente = 0;
  int32_t valor = menu_get_valor_atual();
  uint8_t enter_solto = relogio_anterior && !btn_relogio_status;
  relogio_anterior = btn_relogio_status;

  if (menu_bloqueia_primeira_soltura) {
    enter_solto = 0;
    if (!btn_relogio_status) {
      menu_bloqueia_primeira_soltura = 0;
      relogio_anterior = 0;
    }
  }

  controle_soprador_e_alarmes();

  if (!menu_editando) {
    if (btn_relogio_status && tac_relogio_timeout >= 60U) {
      sair_pendente = 1;
      splash_screen(4, DSP_MINUS, DSP_S, DSP_A, DSP_I, DSP_R, DSP_MINUS);
      return;
    }

    if (btn_max_status && btn_max_timeout == 0) {
      menu_config_item = (uint8_t)((menu_config_item + 1U) % 10U);
      btn_max_timeout = 5;
    } else if (!btn_max_status) {
      btn_max_timeout = 1;
    }

    if (btn_min_status && btn_min_timeout == 0) {
      menu_config_item = (menu_config_item == 0U) ? 9U
                                                  : (uint8_t)(menu_config_item - 1U);
      btn_min_timeout = 5;
    } else if (!btn_min_status) {
      btn_min_timeout = 1;
    }

    menu_display_lista();

    if (enter_solto) {
      if (sair_pendente) {
        sair_pendente = 0;
        modo = 0;
        splash_timeout = 0;
        return;
      }
      menu_editando = 1;
      menu_rtc_campo = 0;
      menu_rtc_visualiza_timeout = 0;
      btn_max_turbo = 0;
      btn_min_turbo = 0;
      splash_timeout = 0;
    }
    return;
  }

  if (btn_max_status && btn_max_timeout == 0) {
    if (menu_item_rtc()) {
      menu_rtc_ajusta_campo(1);
      menu_rtc_visualiza_timeout = 20U;
      btn_max_timeout = 7;
    } else {
      if (btn_max_turbo >= 30)
        valor += 100;
      else if (btn_max_turbo >= 20)
        valor += 20;
      else if (btn_max_turbo >= 10)
        valor += 10;
      else
        valor += 1;

      menu_set_valor_atual(valor);
      if (btn_max_turbo < 30)
        btn_max_turbo++;
      btn_max_timeout = (btn_max_turbo >= 5) ? 4 : 7;
    }
  } else if (!btn_max_status) {
    btn_max_timeout = 2;
    btn_max_turbo = 0;
  }

  if (btn_min_status && btn_min_timeout == 0) {
    if (menu_item_rtc()) {
      menu_rtc_ajusta_campo(-1);
      menu_rtc_visualiza_timeout = 20U;
      btn_min_timeout = 7;
    } else {
      if (btn_min_turbo >= 30)
        valor -= 100;
      else if (btn_min_turbo >= 20)
        valor -= 20;
      else if (btn_min_turbo >= 10)
        valor -= 10;
      else
        valor -= 1;

      menu_set_valor_atual(valor);
      if (btn_min_turbo < 30)
        btn_min_turbo++;
      btn_min_timeout = (btn_min_turbo >= 5) ? 4 : 7;
    }
  } else if (!btn_min_status) {
    btn_min_timeout = 2;
    btn_min_turbo = 0;
  }

  menu_display_edicao();

  if (enter_solto) {
    if (menu_item_rtc() && menu_rtc_campo < (menu_rtc_qtd_campos() - 1U)) {
      menu_rtc_campo++;
      menu_rtc_visualiza_timeout = 0;
      return;
    }

    if (menu_config_item == 6U && datalog_reset_confirm) {
      datalog_reset();
      datalog_reset_confirm = 0;
    }
    if (menu_config_item == 8U || menu_config_item == 9U) {
      if (!rtc_aplica_compacto(rtc_data_ddmmaa, rtc_hora_hhmmss)) {
        splash_screen(8, DSP_E, DSP_R, DSP_R, DSP_OFF, DSP_OFF, DSP_OFF);
        return;
      }
    } else {
      salva_config_secagem_flash();
    }
    menu_editando = 0;
    menu_rtc_campo = 0;
    menu_rtc_visualiza_timeout = 0;
    sair_pendente = 0;
    splash_screen(8, DSP_MINUS, DSP_S, DSP_A, DSP_V, DSP_E, DSP_MINUS);
  }
}

static void modo_diagnostico(void) {
  controle_soprador_e_alarmes();

  atualiza_display_tensoes_diagnostico();

  if (btn_relogio_status || diag_timeout_50ms == 0U) {
    modo = 0;
  }
}

static void processa_tick_50ms(void) {
  if (btn_relogio_status && tac_relogio_timeout < 65535U) {
    tac_relogio_timeout++;
  } else if (!btn_relogio_status) {
    tac_relogio_timeout = 0;
  }

  if (buzzer_periodo_timeout > 0) {
    buzzer_periodo_timeout--;
  }
  if (buzzer_ligado_timeout > 0) {
    buzzer_ligado_timeout--;
  }

  if (tempo_baixa_timeout > 0U) {
    tempo_baixa_timeout--;
  }
  if (rele_comutacao_timeout > 0U) {
    rele_comutacao_timeout--;
  }
  if (diag_timeout_50ms > 0U) {
    diag_timeout_50ms--;
  }
  if (serial_log_pausa_50ms > 0U) {
    serial_log_pausa_50ms--;
  }
  if (menu_rtc_visualiza_timeout > 0U) {
    menu_rtc_visualiza_timeout--;
  }
}

static void prepara_dados_eeprom(void) {
  dados.assinatura = EEPROM_ASSINATURA_APP;
  dados.setpoint_01 = (uint32_t)setpoint_temperatura_dC;
  dados.histerese_temperatura_dC = (uint32_t)histerese_temperatura_dC;
  dados.limite_temperatura_alta_dC = (uint32_t)limite_temperatura_alta_dC;
  dados.limite_temperatura_baixa_dC = (uint32_t)limite_temperatura_baixa_dC;
  dados.zur_umidade = zur_umidade;
  dados.gur_umidade = gur_umidade;
}

static void carrega_config(void) {
  dados = eeprom_read();

  if (dados.assinatura != EEPROM_ASSINATURA_APP) {
    dados.assinatura = EEPROM_ASSINATURA_APP;
    dados.setpoint_01 = (uint32_t)setpoint_temperatura_dC;
    dados.histerese_temperatura_dC = (uint32_t)histerese_temperatura_dC;
    dados.limite_temperatura_alta_dC = (uint32_t)limite_temperatura_alta_dC;
    dados.limite_temperatura_baixa_dC = (uint32_t)limite_temperatura_baixa_dC;
    dados.zur_umidade = 6950U;
    dados.gur_umidade = 314U;
    dados.datalog_periodo_s = datalog_periodo_s;
    eeprom_write_config(&dados);
  }

  if (dados.setpoint_01 > 0U && dados.setpoint_01 <= 999U) {
    setpoint_temperatura_dC = (int32_t)dados.setpoint_01;
  }

  if (dados.histerese_temperatura_dC >= 1U &&
      dados.histerese_temperatura_dC <= 200U) {
    histerese_temperatura_dC = (int32_t)dados.histerese_temperatura_dC;
  }
  if (dados.limite_temperatura_alta_dC > 0U &&
      dados.limite_temperatura_alta_dC <= 999U) {
    limite_temperatura_alta_dC = (int32_t)dados.limite_temperatura_alta_dC;
  }
  if (dados.limite_temperatura_baixa_dC <= 999U) {
    limite_temperatura_baixa_dC = (int32_t)dados.limite_temperatura_baixa_dC;
  }
  if (limite_temperatura_baixa_dC > limite_temperatura_alta_dC) {
    limite_temperatura_baixa_dC = LIMITE_TEMPERATURA_BAIXA_DEFAULT_dC;
    limite_temperatura_alta_dC = LIMITE_TEMPERATURA_ALTA_DEFAULT_dC;
  }
  if (dados.zur_umidade == 0U) {
    dados.zur_umidade = 6950U;
  }
  if (dados.gur_umidade == 0U) {
    dados.gur_umidade = 314U;
  }
  zur_umidade = dados.zur_umidade;
  gur_umidade = dados.gur_umidade;
  if (dados.datalog_periodo_s >= 10U && dados.datalog_periodo_s <= 3600U) {
    datalog_periodo_s = (uint16_t)dados.datalog_periodo_s;
    datalog_timer_s = datalog_periodo_s;
  } else {
    dados.datalog_periodo_s = datalog_periodo_s;
  }

  prepara_dados_eeprom();
}

static void serial_init_rx(void) {
  __disable_irq();
  serial_rx_posicao_fifo = 0;
  serial_rx_posicao_leitura = 0;
  serial_rx_ultimo_byte = 0;
  serial_rx_penultimo_byte = 0;
  serial_rx_timeout_ms = 0;
  serial_rx_overflow = 0;
  serial_rx_error_isr = 0;
  __enable_irq();

  (void)USART1->SR;
  (void)USART1->DR;
  USART1->CR3 |= USART_CR3_EIE;
  USART1->CR1 |= USART_CR1_RXNEIE | USART_CR1_PEIE;
}

void serial_tick_1ms(void) {
  if (serial_rx_timeout_ms > 0U) {
    serial_rx_timeout_ms--;
  }
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

static uint16_t crc16_ibm(const uint8_t *data, uint16_t len) {
  uint16_t crc = 0xFFFFU;

  for (uint16_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t bit = 0; bit < 8U; bit++) {
      if (crc & 1U) {
        crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
      } else {
        crc >>= 1U;
      }
    }
  }

  return crc;
}

static int8_t serial_hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return (int8_t)(c - '0');
  }
  if (c >= 'A' && c <= 'F') {
    return (int8_t)(c - 'A' + 10);
  }
  if (c >= 'a' && c <= 'f') {
    return (int8_t)(c - 'a' + 10);
  }
  return -1;
}

static uint8_t serial_parse_hex_u8(const char *text, uint8_t *value) {
  int8_t hi = serial_hex_nibble(text[0]);
  int8_t lo = serial_hex_nibble(text[1]);

  if (hi < 0 || lo < 0) {
    return 0;
  }
  *value = (uint8_t)(((uint8_t)hi << 4U) | (uint8_t)lo);
  return 1;
}

static uint8_t serial_parse_hex_u16(const char *text, uint16_t *value) {
  int8_t n0 = serial_hex_nibble(text[0]);
  int8_t n1 = serial_hex_nibble(text[1]);
  int8_t n2 = serial_hex_nibble(text[2]);
  int8_t n3 = serial_hex_nibble(text[3]);

  if (n0 < 0 || n1 < 0 || n2 < 0 || n3 < 0) {
    return 0;
  }
  *value = (uint16_t)(((uint16_t)n0 << 12U) | ((uint16_t)n1 << 8U) |
                      ((uint16_t)n2 << 4U) | (uint16_t)n3);
  return 1;
}

static uint8_t serial_parse_u32_strict(const char *text, uint32_t *value) {
  uint32_t result = 0;

  if (text == NULL || *text == '\0') {
    return 0;
  }

  while (*text != '\0') {
    if (*text < '0' || *text > '9') {
      return 0;
    }
    result = (result * 10U) + (uint32_t)(*text - '0');
    text++;
  }

  *value = result;
  return 1;
}

static uint8_t serial_text_is_digits(const char *text, uint8_t expected_len) {
  for (uint8_t i = 0; i < expected_len; i++) {
    if (text[i] < '0' || text[i] > '9') {
      return 0;
    }
  }
  return text[expected_len] == '\0';
}

static void serial_send_packet(uint8_t cmd, const char *payload) {
  char body[180];
  char frame[196];
  uint16_t len = 0;
  uint16_t crc = 0;

  if (payload == NULL) {
    payload = "";
  }

  len = (uint16_t)strlen(payload);
  snprintf(body, sizeof(body), "%02X%02X%s", cmd, len, payload);
  crc = crc16_ibm((const uint8_t *)body, (uint16_t)strlen(body));
  snprintf(frame, sizeof(frame), "\x02%s%04X\r\n", body, crc);
  serial_send_text(frame);
}

static uint16_t serial_get_datalog_total(void) {
  return datalog_ok ? datalog_header.quantidade_valida : 0U;
}

static uint8_t serial_apply_config_list(const char *payload) {
  char work[150];
  char *token = NULL;
  uint8_t aplicou = 0;
  uint8_t item_anterior = menu_config_item;

  strncpy(work, payload, sizeof(work) - 1U);
  work[sizeof(work) - 1U] = '\0';

  token = strtok(work, ",");
  while (token != NULL) {
    char *sep = strchr(token, '=');
    uint32_t valor = 0;

    while (*token == ' ') {
      token++;
    }

    if (sep == NULL) {
      return 0;
    }

    *sep = '\0';
    if (!serial_parse_u32_strict(sep + 1, &valor)) {
      menu_config_item = item_anterior;
      return 0;
    }

    if (strcmp(token, "SP") == 0) {
      menu_config_item = 0;
    } else if (strcmp(token, "HIS") == 0) {
      menu_config_item = 1;
    } else if (strcmp(token, "ALT") == 0) {
      menu_config_item = 2;
    } else if (strcmp(token, "BAI") == 0) {
      menu_config_item = 3;
    } else if (strcmp(token, "ZUR") == 0) {
      menu_config_item = 4;
    } else if (strcmp(token, "GUR") == 0) {
      menu_config_item = 5;
    } else if (strcmp(token, "PER") == 0) {
      menu_config_item = 7;
    } else {
      menu_config_item = item_anterior;
      return 0;
    }

    menu_set_valor_atual((int32_t)valor);
    aplicou = 1;
    token = strtok(NULL, ",");
  }

  menu_config_item = item_anterior;

  if (aplicou) {
    salva_config_secagem_flash();
  }
  return aplicou;
}

static uint8_t serial_apply_config_command(const char *cmd) {
  static const char *nomes[] = {"SP=",  "HIS=", "ALT=", "BAI=",
                                "ZUR=", "GUR=", "PER="};
  const char *payload = cmd;

  if (strncmp(cmd, "04 ", 3) == 0) {
    payload = cmd + 3;
  } else if (strncmp(cmd, "CFG ", 4) == 0) {
    payload = cmd + 4;
  } else {
    uint8_t conhecido = 0;
    for (uint8_t i = 0; i < (sizeof(nomes) / sizeof(nomes[0])); i++) {
      if (strncmp(cmd, nomes[i], strlen(nomes[i])) == 0) {
        conhecido = 1;
        break;
      }
    }
    if (!conhecido) {
      return 0;
    }
  }

  return serial_apply_config_list(payload) ? 1U : 2U;
}

static uint8_t serial_parse_decimal_field(const char **text, uint32_t *value) {
  uint32_t result = 0;
  uint8_t digits = 0;

  while (**text >= '0' && **text <= '9') {
    result = (result * 10U) + (uint32_t)(**text - '0');
    (*text)++;
    digits++;
  }

  if (digits == 0U) {
    return 0;
  }

  *value = result;
  return 1;
}

static uint8_t serial_parse_log_command(const char *cmd, uint32_t *indice,
                                        uint32_t *quantidade) {
  const char *payload = NULL;

  if (strncmp(cmd, "06 ", 3) == 0) {
    payload = cmd + 3;
  } else if (strncmp(cmd, "LOG ", 4) == 0) {
    payload = cmd + 4;
  } else {
    return 0;
  }

  if (!serial_parse_decimal_field(&payload, indice)) {
    return 2;
  }

  *quantidade = 1U;
  if (*payload == ',') {
    payload++;
    if (!serial_parse_decimal_field(&payload, quantidade)) {
      return 2;
    }
  }

  return (*payload == '\0') ? 1U : 2U;
}

static uint8_t serial_log_send_record(uint16_t indice) {
  char payload[150];
  datalog_record_t record = {0};

  if (!datalog_ok || indice >= datalog_header.quantidade_valida) {
    serial_send_packet(0x7F, "E06");
    return 0;
  }

  uint16_t pos = (datalog_header.proximo_indice + datalog_header.capacidade -
                  datalog_header.quantidade_valida + indice) %
                 datalog_header.capacidade;
  if (!ext_eeprom_read(datalog_record_addr(pos), &record, sizeof(record)) ||
      record.crc != datalog_record_crc(&record)) {
    serial_send_packet(0x7F, "E07");
    return 0;
  }

  snprintf(payload, sizeof(payload), "%u,%u,%lu,%d,%u,%04X", indice,
           datalog_header.quantidade_valida, record.timestamp,
           record.temperatura_dC, record.umidade_dUR, record.flags);
  serial_send_packet(0x86, payload);
  return 1;
}

static void serial_log_start(uint16_t indice, uint16_t quantidade) {
  uint16_t total = serial_get_datalog_total();

  serial_log_enviando = 0;
  serial_log_restante = 0;

  if (!datalog_ok || indice >= total) {
    serial_send_packet(0x7F, "E06");
    return;
  }

  serial_log_indice = indice;
  serial_log_restante = total - indice;
  if (quantidade > 0U && quantidade < serial_log_restante) {
    serial_log_restante = quantidade;
  }
  serial_log_enviando = 1;
}

static void serial_log_process(void) {
  if (!serial_log_enviando || serial_log_restante == 0U) {
    serial_log_enviando = 0;
    return;
  }

  if (serial_log_pausa_50ms > 0U) {
    return;
  }

  if (!serial_log_send_record(serial_log_indice)) {
    serial_log_enviando = 0;
    return;
  }

  serial_log_pausa_50ms = 1U;
  serial_log_indice++;
  serial_log_restante--;
  if (serial_log_restante == 0U) {
    serial_log_enviando = 0;
  }
}

static void serial_handle_command(const char *cmd) {
  char payload[150];
  char decoded_cmd[180];
  uint32_t indice = 0;
  uint32_t quantidade = 0;
  uint8_t cfg_result = 0;
  uint8_t log_result = 0;

  if (*cmd == '\x02') {
    uint8_t cmd_id = 0;
    uint8_t tam = 0;
    uint16_t crc_recebido = 0;
    uint16_t crc_calc = 0;
    uint16_t frame_len = (uint16_t)strlen(cmd);
    const char *body = cmd + 1;

    if (frame_len < 9U || !serial_parse_hex_u8(&body[0], &cmd_id) ||
        !serial_parse_hex_u8(&body[2], &tam)) {
      serial_send_packet(0x7F, "E02");
      return;
    }

    if ((uint16_t)(4U + tam + 4U) != (frame_len - 1U)) {
      serial_send_packet(0x7F, "E02");
      return;
    }

    if (!serial_parse_hex_u16(&body[4U + tam], &crc_recebido)) {
      serial_send_packet(0x7F, "E02");
      return;
    }

    crc_calc = crc16_ibm((const uint8_t *)body, (uint16_t)(4U + tam));
    if (crc_calc != crc_recebido) {
      serial_send_packet(0x7F, "E01");
      return;
    }

    if (tam >= sizeof(payload)) {
      serial_send_packet(0x7F, "E02");
      return;
    }

    memcpy(payload, &body[4], tam);
    payload[tam] = '\0';
    snprintf(decoded_cmd, sizeof(decoded_cmd), "%02X %s", cmd_id, payload);
    cmd = decoded_cmd;
  }

  while (*cmd == ' ' || *cmd == '\t') {
    cmd++;
  }

  if (strncmp(cmd, "01", 2) == 0 || strncmp(cmd, "ID", 2) == 0) {
    snprintf(payload, sizeof(payload), "UMMI-SECAGEM,1,%u", datalog_header.capacidade);
    serial_send_packet(0x01, payload);
  } else if (strncmp(cmd, "02", 2) == 0 || strncmp(cmd, "RT", 2) == 0) {
    uint16_t flags = (soprador_ligado ? 0x0001U : 0x0000U) |
                     (temperatura_desconectada ? 0x0010U : 0x0000U) |
                     (umidade_desconectada ? 0x0020U : 0x0000U) |
                     (umidade_fora_faixa ? 0x0040U : 0x0000U) |
                     (alarme_temperatura_alta ? 0x0100U : 0x0000U) |
                     (alarme_temperatura_baixa ? 0x0200U : 0x0000U) |
                     (alarme_sensor_temperatura ? 0x0400U : 0x0000U) |
                     (alarme_sensor_umidade ? 0x0800U : 0x0000U) |
                     (alarme_alimentacao ? 0x1000U : 0x0000U);
    snprintf(payload, sizeof(payload), "%ld,%u,%u,%u,%04X",
             (long)temperatura_dC, umidade_dUR, tensao_5v_mV, tensao_12v_mV,
             flags);
    serial_send_packet(0x82, payload);
  } else if (strncmp(cmd, "03", 2) == 0 || strncmp(cmd, "CFG?", 4) == 0) {
    snprintf(payload, sizeof(payload),
             "SP=%ld,HIS=%ld,ALT=%ld,BAI=%ld,ZUR=%lu,GUR=%lu,PER=%u",
             (long)setpoint_temperatura_dC, (long)histerese_temperatura_dC,
             (long)limite_temperatura_alta_dC,
             (long)limite_temperatura_baixa_dC, zur_umidade, gur_umidade,
             datalog_periodo_s);
    serial_send_packet(0x83, payload);
  } else if ((cfg_result = serial_apply_config_command(cmd)) != 0U) {
    if (cfg_result == 1U) {
      serial_send_packet(0x84, "OK");
    } else {
      serial_send_packet(0x7F, "E04");
    }
  } else if (strncmp(cmd, "05", 2) == 0 || strncmp(cmd, "LOG?", 4) == 0) {
    snprintf(payload, sizeof(payload), "%u,%u,%u,%u,%u,%u", serial_get_datalog_total(),
             datalog_header.capacidade, datalog_header.proximo_indice,
             datalog_periodo_s, datalog_ok, serial_log_enviando);
    serial_send_packet(0x85, payload);
  } else if ((log_result = serial_parse_log_command(cmd, &indice, &quantidade)) !=
             0U) {
    if (log_result != 1U) {
      serial_send_packet(0x7F, "E06");
      return;
    }
    if (indice > 0xFFFFU || quantidade > 0xFFFFU) {
      serial_send_packet(0x7F, "E06");
      return;
    }
    serial_log_start((uint16_t)indice, (uint16_t)quantidade);
  } else if (strncmp(cmd, "07 CONFIRMA", 11) == 0 ||
             strncmp(cmd, "LOGRST", 6) == 0) {
    serial_log_enviando = 0;
    datalog_reset();
    serial_send_packet(0x87, datalog_ok ? "OK" : "E07");
  } else if (strncmp(cmd, "STOPLOG", 7) == 0) {
    serial_log_enviando = 0;
    serial_log_restante = 0;
    serial_send_packet(0x87, "OK");
  } else if (strncmp(cmd, "08 ", 3) == 0) {
    char data_txt[7];
    char hora_txt[7];
    uint32_t data = 0;
    uint32_t hora = 0;
    const char *rtc_texto = cmd + 3;

    if (!serial_text_is_digits(rtc_texto, 12U)) {
      serial_send_packet(0x7F, "E04");
      return;
    }
    memcpy(data_txt, rtc_texto, 6U);
    data_txt[6] = '\0';
    memcpy(hora_txt, &rtc_texto[6], 6U);
    hora_txt[6] = '\0';
    data = strtoul(data_txt, NULL, 10);
    hora = strtoul(hora_txt, NULL, 10);

    if (!rtc_aplica_compacto(data, hora)) {
      serial_send_packet(0x7F, "E04");
    } else {
      serial_send_packet(0x88, "OK");
    }
  } else if (strncmp(cmd, "09", 2) == 0 || strncmp(cmd, "RTC?", 4) == 0) {
    RTC_TimeTypeDef time = {0};
    RTC_DateTypeDef date = {0};

    if (HAL_RTC_GetTime(&hrtc, &time, RTC_FORMAT_BIN) != HAL_OK ||
        HAL_RTC_GetDate(&hrtc, &date, RTC_FORMAT_BIN) != HAL_OK) {
      serial_send_packet(0x7F, "E08");
      return;
    }
    snprintf(payload, sizeof(payload), "20%02u-%02u-%02u,%02u:%02u:%02u",
             date.Year, date.Month, date.Date, time.Hours, time.Minutes,
             time.Seconds);
    serial_send_packet(0x89, payload);
  } else {
    serial_send_packet(0x7F, "E03");
  }
}

static void serial_process(void) {
  uint16_t len = 0;
  uint8_t overflow = 0;
  uint8_t erro_uart = 0;
  uint8_t processar = 0;

  __disable_irq();
  uint16_t disponivel;
  if (serial_rx_posicao_fifo >= serial_rx_posicao_leitura) {
    disponivel = serial_rx_posicao_fifo - serial_rx_posicao_leitura;
  } else {
    disponivel = (uint16_t)(SERIAL_RX_BUFFER_SIZE - serial_rx_posicao_leitura +
                            serial_rx_posicao_fifo);
  }

  overflow = serial_rx_overflow;
  erro_uart = serial_rx_error_isr;
  serial_rx_overflow = 0;
  serial_rx_error_isr = 0;

  if (disponivel > 0U && serial_rx_timeout_ms == 0U) {
    processar = 1;
    while (disponivel > 0U && len < (sizeof(serial_cmd_buffer) - 1U)) {
      uint8_t byte = serial_rx_buffer[serial_rx_posicao_leitura++];
      if (serial_rx_posicao_leitura >= SERIAL_RX_BUFFER_SIZE) {
        serial_rx_posicao_leitura = 0;
      }
      disponivel--;

      if (byte == '\r' || byte == '\n') {
        break;
      }

      serial_cmd_buffer[len++] = (char)byte;
    }

    if (disponivel > 0U && len >= (sizeof(serial_cmd_buffer) - 1U)) {
      overflow = 1;
      serial_rx_posicao_leitura = serial_rx_posicao_fifo;
    }
  }
  __enable_irq();

  if (erro_uart || overflow) {
    serial_send_packet(0x7F, "E02");
    return;
  }

  if (processar && len > 0U) {
    serial_cmd_buffer[len] = '\0';
    serial_handle_command(serial_cmd_buffer);
  }
}

void serial_usart1_irq_handler(void) {
  uint32_t sr = USART1->SR;

  if ((sr & (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)) != 0U) {
    (void)USART1->DR;
    serial_rx_posicao_fifo = 0;
    serial_rx_posicao_leitura = 0;
    serial_rx_ultimo_byte = 0;
    serial_rx_penultimo_byte = 0;
    serial_rx_timeout_ms = 0;
    serial_rx_error_isr = 1;
    return;
  }

  if ((sr & USART_SR_RXNE) != 0U) {
    uint8_t byte = (uint8_t)USART1->DR;
    uint16_t proxima_posicao = (uint16_t)(serial_rx_posicao_fifo + 1U);
    if (proxima_posicao >= SERIAL_RX_BUFFER_SIZE) {
      proxima_posicao = 0;
    }

    if (proxima_posicao == serial_rx_posicao_leitura) {
      serial_rx_overflow = 1;
      serial_rx_timeout_ms = 0;
      return;
    }

    serial_rx_penultimo_byte = serial_rx_ultimo_byte;
    serial_rx_ultimo_byte = byte;
    serial_rx_buffer[serial_rx_posicao_fifo] = byte;
    serial_rx_posicao_fifo = proxima_posicao;

    if (byte == '\r' || byte == '\n') {
      serial_rx_timeout_ms = 0;
    } else {
      serial_rx_timeout_ms = SERIAL_RX_TIMEOUT_MS;
    }
  }
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line
     number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
     line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
