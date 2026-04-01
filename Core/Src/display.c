/*
 * display.c
 *
 *  Created on: Apr 1, 2026
 *      Author: Lucas
 */
#include "display.h"
#include "defines.h"
#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "main.h"

uint8_t display = 0;
uint8_t unidade = 6;
uint8_t dezena = 5;
uint8_t centena = 4;
uint8_t milhar = 3;
uint8_t dezena_milhar = 2;
uint8_t centena_milhar = 1;


const uint8_t tabela_display[] = {
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
    0b01101111, // 16 = G
    0b01110110, // 17 = H
    0b00000100, // 18 = I
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


void escreve_segmentos(uint8_t valor) {
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

void display_scan() {
  uint8_t byte_display;
  uint8_t anterior;

  anterior = display - 1;
  if (display == 0)
    anterior = 5;

  desliga_displays(anterior);

  switch (display) {
  case 0:
    byte_display = tabela_display[unidade];
    escreve_segmentos(byte_display);
    HAL_GPIO_WritePin(DISPLAY_UNIDADE_PORT, DISPLAY_UNIDADE, HIGH);
    break;

  case 1:
    byte_display = tabela_display[dezena];
    escreve_segmentos(byte_display);
    HAL_GPIO_WritePin(DISPLAY_DEZENA_PORT, DISPLAY_DEZENA, HIGH);
    break;

  case 2:
    byte_display = tabela_display[centena];
    escreve_segmentos(byte_display);
    HAL_GPIO_WritePin(DISPLAY_CENTENA_PORT, DISPLAY_CENTENA, HIGH);
    break;

  case 3:
    byte_display = tabela_display[milhar];
    escreve_segmentos(byte_display);
    HAL_GPIO_WritePin(DISPLAY_MILHAR_UNIDADE_PORT, DISPLAY_MILHAR_UNIDADE, HIGH);
    break;

  case 4:
    byte_display = tabela_display[dezena_milhar];
    escreve_segmentos(byte_display);
    HAL_GPIO_WritePin(DISPLAY_MILHAR_DEZENA_PORT, DISPLAY_MILHAR_DEZENA, HIGH);
    break;

  case 5:
    byte_display = tabela_display[centena_milhar];
    escreve_segmentos(byte_display);
    HAL_GPIO_WritePin(DISPLAY_MILHAR_CENTENA_PORT, DISPLAY_MILHAR_CENTENA, HIGH);
    break;
  }

  display++;

  if (display > 5)
    display = 0;
}

void desliga_displays(uint8_t d) {
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