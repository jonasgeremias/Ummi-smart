/*
 * display.c
 *
 *  Created on: Apr 1, 2026
 *      Author: Lucas
 */
#include "display.h"
#include "defines.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include "timer.h"
#include <stdint.h>

static volatile uint8_t display = 0;
static volatile uint8_t unidade = 0;
static volatile uint8_t dezena = 0;
static volatile uint8_t centena = 0;
static volatile uint8_t milhar = 0;
static volatile uint8_t dezena_milhar = 0;
static volatile uint8_t centena_milhar = 0;

volatile uint8_t splash_digits[6] = {0};
static volatile uint8_t decimal_point_timeout = 0;
static volatile uint8_t decimal_point_mask = 0;

#define TABELA_DISPLAY_SIZE (sizeof(tabela_display) / sizeof(tabela_display[0]))

static const uint8_t tabela_display[] = {
    //  87654321
    0b00111111, //  0
    0b00000110, //  1
    0b01011011, //  2
    0b01001111, //  3
    0b01100110, //  4
    0b01101101, //  5
    0b01111101, //  6
    0b00000111, //  7
    0b01111111, //  8
    0b01101111, //  9
    0b01110111, // 10 = A
    0b01111100, // 11 = b
    0b00111001, // 12 = C
    0b01011110, // 13 = d
    0b01111001, // 14 = E
    0b01110001, // 15 = F
    0b01111101, // 16 = G
    0b01110110, // 17 = H
    0b00000110, // 18 = I
    0b00011110, // 19 = J
    0b00111000, // 20 = L
    0b01010100, // 21 = N
    0b01110011, // 22 = P
    0b01100111, // 23 = q
    0b01010000, // 24 = r
    0b01101101, // 25 = S
    0b01111000, // 26 = t
    0b00111110, // 27 = U
    0b00000000, // 28 = desligado
    0b01000000, // 29 = "-"
    0b00111111, // 30 = k
    0b00011100  // 31 = U
};

static void escreve_segmentos(uint8_t valor);
static void desliga_displays(uint8_t d);
static void desliga_todos_displays(void);

static void escreve_segmentos(uint8_t valor) {
  HAL_GPIO_WritePin(SEG_A_GPIO_PORT, SEG_A_PIN,
                    (valor & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_B_GPIO_PORT, SEG_B_PIN,
                    (valor & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_C_GPIO_PORT, SEG_C_PIN,
                    (valor & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_D_GPIO_PORT, SEG_D_PIN,
                    (valor & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_E_GPIO_PORT, SEG_E_PIN,
                    (valor & 0x10) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_F_GPIO_PORT, SEG_F_PIN,
                    (valor & 0x20) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_G_GPIO_PORT, SEG_G_PIN,
                    (valor & 0x40) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SEG_DP_GPIO_PORT, SEG_DP_PIN,
                    (valor & 0x80) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void display_atualiza(uint32_t valor) {
  unidade = valor % 10;
  dezena = (valor / 10) % 10;
  centena = (valor / 100) % 10;
  milhar = (valor / 1000) % 10;
  dezena_milhar = (valor / 10000) % 10;
  centena_milhar = (valor / 100000) % 10;
}

void display_set_decimal_points(uint8_t timeout_50ms, uint8_t digit_mask) {
  decimal_point_timeout = timeout_50ms;
  decimal_point_mask = digit_mask;
}

void display_tick_50ms(void) {
  if (decimal_point_timeout > 0) {
    decimal_point_timeout--;
  }
}

void display_scan(void) {
  uint8_t byte_display;
  uint8_t valor = DSP_OFF;

  if (display > 5) {
    display = 0;
  }

  desliga_todos_displays();

  // 🔥 ESCOLHE ENTRE SPLASH OU DISPLAY NORMAL
  if (splash_timeout > 0) {
    valor = splash_digits[display];
  } else {
    switch (display) {
    case 0:
      valor = unidade;
      break;
    case 1:
      valor = dezena;
      break;
    case 2:
      valor = centena;
      break;
    case 3:
      valor = milhar;
      break;
    case 4:
      valor = dezena_milhar;
      break;
    case 5:
      valor = centena_milhar;
      break;
    }
  }

  if (valor >= TABELA_DISPLAY_SIZE) {
    valor = DSP_OFF;
  }

  byte_display = tabela_display[valor];
  if ((decimal_point_timeout > 0) && (decimal_point_mask & (1U << display))) {
    byte_display |= 0x80;
  }
  escreve_segmentos(byte_display);

  // liga display
  switch (display) {
  case 0:
    HAL_GPIO_WritePin(DISPLAY_UNIDADE_PORT, DISPLAY_UNIDADE, HIGH);
    break;
  case 1:
    HAL_GPIO_WritePin(DISPLAY_DEZENA_PORT, DISPLAY_DEZENA, HIGH);
    break;
  case 2:
    HAL_GPIO_WritePin(DISPLAY_CENTENA_PORT, DISPLAY_CENTENA, HIGH);
    break;
  case 3:
    HAL_GPIO_WritePin(DISPLAY_MILHAR_UNIDADE_PORT, DISPLAY_MILHAR_UNIDADE,
                      HIGH);
    break;
  case 4:
    HAL_GPIO_WritePin(DISPLAY_MILHAR_DEZENA_PORT, DISPLAY_MILHAR_DEZENA, HIGH);
    break;
  case 5:
    HAL_GPIO_WritePin(DISPLAY_MILHAR_CENTENA_PORT, DISPLAY_MILHAR_CENTENA,
                      HIGH);
    break;
  }

  display++;
  if (display > 5)
    display = 0;
}

static void desliga_todos_displays(void) {
  for (uint8_t i = 0; i < 6; i++) {
    desliga_displays(i);
  }
}

static void desliga_displays(uint8_t d) {
  switch (d) {
  case 0:
    HAL_GPIO_WritePin(DISPLAY_UNIDADE_PORT, DISPLAY_UNIDADE, LOW);
    break;

  case 1:
    HAL_GPIO_WritePin(DISPLAY_DEZENA_PORT, DISPLAY_DEZENA, LOW);
    break;

  case 2:
    HAL_GPIO_WritePin(DISPLAY_CENTENA_PORT, DISPLAY_CENTENA, LOW);
    break;

  case 3:
    HAL_GPIO_WritePin(DISPLAY_MILHAR_UNIDADE_PORT, DISPLAY_MILHAR_UNIDADE, LOW);
    break;

  case 4:
    HAL_GPIO_WritePin(DISPLAY_MILHAR_DEZENA_PORT, DISPLAY_MILHAR_DEZENA, LOW);
    break;

  case 5:
    HAL_GPIO_WritePin(DISPLAY_MILHAR_CENTENA_PORT, DISPLAY_MILHAR_CENTENA, LOW);
    break;
  }
}
