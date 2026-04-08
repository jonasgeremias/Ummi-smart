/*
 * timer.h
 *
 *  Created on: Apr 1, 2026
 *      Author: Lucas
 */

#ifndef INC_TIMER_H_
#define INC_TIMER_H_


#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>


extern volatile uint16_t timer_3ms;
extern volatile uint16_t timer_5ms;
extern volatile uint16_t timer_10ms;
extern volatile uint16_t timer_50ms;
extern volatile uint16_t timer_100ms;
extern volatile uint16_t timer_500ms;
extern volatile uint16_t timer_1000ms;

extern volatile uint8_t timer_flag_3ms;
extern volatile uint8_t timer_flag_5ms;
extern volatile uint8_t timer_flag_10ms;
extern volatile uint8_t timer_flag_50ms;
extern volatile uint8_t timer_flag_100ms;
extern volatile uint8_t timer_flag_500ms;
extern volatile uint8_t timer_flag_1000ms;
extern volatile uint8_t timer_flag_5s;
extern volatile uint8_t timer_cnt_1000ms;

// botão max
extern volatile uint16_t btn_max_timeout;
extern volatile uint8_t btn_max_status;
extern volatile uint16_t btn_max_contador;
extern volatile uint8_t btn_max_borda_anterior;
extern volatile uint8_t btn_max_turbo;
extern volatile uint8_t btn_max_evento;

// botão min
extern volatile uint16_t btn_min_timeout;
extern volatile uint8_t btn_min_status;
extern volatile uint16_t btn_min_contador;
extern volatile uint8_t btn_min_borda_anterior;
extern volatile uint8_t btn_min_turbo;
extern volatile uint8_t btn_min_evento;

// botão relogio
extern volatile uint8_t btn_relogio_status;
extern volatile uint8_t btn_relogio_borda_anterior;
extern volatile uint16_t btn_relogio_contador;
extern volatile uint16_t btn_relogio_hold;
extern volatile uint8_t btn_relogio_evento;

// entrada digital
extern volatile uint8_t entrada_digital_status;
extern volatile uint8_t entrada_digital_borda_anterior;
extern volatile uint16_t entrada_digital_contador;

//outras variaveis




void mainIsr(void);
void tmr_3ms(void);
void tmr_5ms(void);
void tmr_10ms(void);
void tmr_50ms(void);
void tmr_100ms(void);
void tmr_500ms(void);
void tmr_1000ms(void);
// void tmr_5s(void);

void display_scan(void);
void debounce_btns(void);
void debounce_entradas_digitais(void);
void btn_relogio_processado(void);
// void serialHandleTimeout1ms(void);

#endif /* INC_TIMER_H_ */
