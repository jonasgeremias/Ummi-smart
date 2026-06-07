/*
 * timer.c
 *
 *  Created on: Apr 1, 2026
 *      Author: Lucas
 */

#include "timer.h"
#include "display.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include "defines.h"

volatile uint16_t timer_1ms = 0;
volatile uint16_t timer_3ms = 0;
volatile uint16_t timer_5ms = 0;
volatile uint16_t timer_10ms = 0;
volatile uint16_t timer_50ms = 0;
volatile uint16_t timer_100ms = 0;
volatile uint16_t timer_500ms = 0;
volatile uint16_t timer_1000ms = 0;
volatile uint8_t timer_flag_1ms = 0;
volatile uint8_t timer_flag_3ms = 0;
volatile uint8_t timer_flag_5ms = 0;
volatile uint8_t timer_flag_10ms = 0;
volatile uint8_t timer_flag_50ms = 0;
volatile uint8_t timer_flag_100ms = 0;
volatile uint8_t timer_flag_500ms = 0;
volatile uint8_t timer_flag_1000ms = 0;
volatile uint8_t timer_cnt_1000ms = 0;


static uint16_t correcao = 0;
volatile uint16_t splash_timeout = 0;

// botão max
volatile uint16_t btn_max_timeout = 0;
volatile uint8_t btn_max_status = 0;
volatile uint16_t btn_max_contador = 0;
volatile uint8_t btn_max_borda_anterior = 0;
volatile uint8_t btn_max_turbo = 0;
volatile uint8_t btn_max_evento = 0;

// botão min
volatile uint16_t btn_min_timeout = 0;
volatile uint8_t btn_min_status = 0;
volatile uint16_t btn_min_contador = 0;
volatile uint8_t btn_min_borda_anterior = 0;
volatile uint8_t btn_min_turbo = 0;
volatile uint8_t btn_min_evento = 0;

// botão relogio
volatile uint8_t btn_relogio_status = 0;
volatile uint8_t btn_relogio_borda_anterior = 0;
volatile uint16_t btn_relogio_contador = 0;
volatile uint16_t btn_relogio_hold = 0;
volatile uint8_t btn_relogio_evento = 0;

// entradas digitais
volatile uint16_t entrada_digital_contador = 0;
volatile uint8_t entrada_digital_status = 0;
volatile uint8_t entrada_digital_borda_anterior = 0;
volatile uint16_t entrada_digital_pulsos_pendentes = 0;

volatile uint8_t entrada_digital_02_contador = 0;
volatile uint8_t entrada_digital_02_status = 0;
volatile uint8_t entrada_digital_02_borda_anterior = 0;
volatile uint8_t entrada_digital_02_evento_pendente = 0;

// outras variaveis

static void tmr_1ms(void);
static void tmr_3ms(void);
static void tmr_5ms(void);
static void tmr_10ms(void);
static void tmr_50ms(void);
static void tmr_100ms(void);
static void tmr_500ms(void);
static void tmr_1000ms(void);
static void debounce_btns(void);
static void debounce_entradas_digitais(void);

void mainIsr(void) {

  // 1 ms - Timer base para serial
  //serialHandleTimeout1ms();

  // scan_display();
  if (++timer_1ms >= 1) {
    timer_1ms = 0;
    tmr_1ms();
  }

  if (++timer_3ms >= 3) {
    timer_3ms = 0;
    tmr_3ms();
  }

  if (++timer_5ms >= 5) {
    timer_5ms = 0;
    tmr_5ms();
  }

  if (++timer_10ms >= 10) {
    timer_10ms = 0;
    tmr_10ms();
  }

  if (++timer_50ms >= 50) {
    timer_50ms = 0;
    tmr_50ms();
  }
  if (++timer_100ms >= 100) {
    timer_100ms = 0;
    tmr_100ms();
  }

  if (++timer_500ms >= 500) {
    timer_500ms = 0;
    tmr_500ms();
  }

  if (++timer_1000ms >= 1000) {
    timer_1000ms = 0;
    tmr_1000ms();

    if (correcao >= 100) {
      correcao = 0;
      timer_cnt_1000ms--;
    }
  }
}

static void tmr_1ms(void) {
  timer_flag_1ms = 1; 
  display_scan();
}

static void tmr_3ms(void) {
  timer_flag_3ms = 1;
  debounce_btns();
  debounce_entradas_digitais();
}
static void tmr_5ms(void) { timer_flag_5ms = 1; }
static void tmr_10ms(void) {
  timer_flag_10ms = 1;
}
static void tmr_50ms(void) {
  timer_flag_50ms = 1;
  if (splash_timeout > 0)
    splash_timeout--;
  if (btn_max_timeout > 0)
    btn_max_timeout--;
  if (btn_min_timeout > 0)
    btn_min_timeout--;
  display_tick_50ms();
}
static void tmr_100ms(void) { timer_flag_100ms = 1; }
static void tmr_500ms(void) { timer_flag_500ms = 1; }
static void tmr_1000ms(void) {
  timer_flag_1000ms = 1; 
}

static void debounce_btns(void) {
  if (HAL_GPIO_ReadPin(BOTAO_RELOGIO_PORT, BOTAO_RELOGIO) == LOW) {
    if (btn_relogio_contador < 2)
      btn_relogio_contador++;
    else
      btn_relogio_status = 1;
  } else {
    btn_relogio_contador = 0;
    btn_relogio_status = 0;
  }

  if (HAL_GPIO_ReadPin(BOTAO_MIN_PORT, BOTAO_MIN) == LOW) {
    if (btn_min_contador < 2)
      btn_min_contador++;
    else
      btn_min_status = 1;
  } else {
    btn_min_contador = 0;
    btn_min_status = 0;
  }

  if (HAL_GPIO_ReadPin(BOTAO_MAX_PORT, BOTAO_MAX) == LOW) {
    if (btn_max_contador < 2)
      btn_max_contador++;
    else
      btn_max_status = 1;
  } else {
    btn_max_contador = 0;
    btn_max_status = 0;
  }
}

static void debounce_entradas_digitais(void) {
  if (HAL_GPIO_ReadPin(INPUT_01_PORT, INPUT_01) == INPUT_01_ACTIVE_LEVEL) {
    if (entrada_digital_contador < INPUT_DEBOUNCE_TICKS)
      entrada_digital_contador++;
    else
      entrada_digital_status = 1;
  } else {
    entrada_digital_contador = 0;
    entrada_digital_status = 0;
  }

  if (entrada_digital_status && !entrada_digital_borda_anterior) {
    if (entrada_digital_pulsos_pendentes < 65535U) {
      entrada_digital_pulsos_pendentes++;
    }
  }
  entrada_digital_borda_anterior = entrada_digital_status;

  if (HAL_GPIO_ReadPin(INPUT_02_PORT, INPUT_02) == INPUT_02_ACTIVE_LEVEL) {
    if (entrada_digital_02_contador < INPUT_DEBOUNCE_TICKS)
      entrada_digital_02_contador++;
    else
      entrada_digital_02_status = 1;
  } else {
    entrada_digital_02_contador = 0;
    entrada_digital_02_status = 0;
  }

  if (entrada_digital_02_status && !entrada_digital_02_borda_anterior) {
    entrada_digital_02_evento_pendente = 1;
  }
  entrada_digital_02_borda_anterior = entrada_digital_02_status;

}

