/*
 * display.h
 *
 *  Created on: Apr 1, 2026
 *      Author: Lucas
 */

#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include <stdint.h>


extern volatile uint8_t splash_digits[6];

#define DSP_0 0
#define DSP_1 1
#define DSP_2 2
#define DSP_3 3
#define DSP_4 4
#define DSP_5 5
#define DSP_6 6
#define DSP_7 7
#define DSP_8 8
#define DSP_9 9
#define DSP_A 10
#define DSP_B 11
#define DSP_C 12
#define DSP_D 13
#define DSP_E 14
#define DSP_F 15
#define DSP_G 16
#define DSP_H 17
#define DSP_I 18
#define DSP_J 19
#define DSP_L 20
#define DSP_N 21
#define DSP_P 22
#define DSP_Q 23
#define DSP_R 24
#define DSP_S 25
#define DSP_T 26
#define DSP_U 27
#define DSP_OFF 28
#define DSP_MINUS 29

void display_scan(void);
void display_atualiza(uint32_t valor);

#endif /* INC_DISPLAY_H_ */
