/*
 * display.h
 *
 *  Created on: Apr 1, 2026
 *      Author: Lucas
 */

#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include <stdint.h>

//void display_init(void);
void display_scan(void);
void escreve_segmentos(uint8_t valor);
void desliga_displays(uint8_t d);
void display_atualiza(uint32_t valor);

extern uint8_t unidade;
extern uint8_t dezena;
extern uint8_t centena;
extern uint8_t milhar;
extern uint8_t dezena_milhar;
extern uint8_t centena_milhar;

extern volatile uint16_t splash_timeout;
extern volatile uint8_t splash_digits[6];

#endif /* INC_DISPLAY_H_ */
