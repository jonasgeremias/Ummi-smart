/*
 * ui.h — Interface de usuario: display (Termometro/Relogio/alternada),
 * maximos/minimos, menu de parametros (inversor) com telas para GUR/ZUR e
 * demais, e modo teste (tensoes 5 V/12 V).
 */
#ifndef INC_UI_H_
#define INC_UI_H_

#include <stdint.h>
#include <stdbool.h>

void ui_init(void);
/* Chamar todo ciclo do laco principal (render + entrada). */
void ui_tick(void);
/* Chamar a cada 50 ms (timeouts internos). */
void ui_tick_50ms(void);
/* Mostra a marca BE1 1.0 imediatamente no boot (nao bloqueante). */
void ui_splash_marca(void);
/* Sequencia de splash inicial (BE1 1.0, data, hora). */
void ui_splash_inicial(void);

#endif /* INC_UI_H_ */
