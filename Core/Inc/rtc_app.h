/*
 * rtc_app.h
 *
 * Camada de aplicacao sobre o RTC interno do STM32 (substitui o DS1307).
 * Formato 24 h. Datas/horas "compactas": DDMMAA e HHMMSS em decimal.
 */
#ifndef INC_RTC_APP_H_
#define INC_RTC_APP_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint8_t dia, mes, ano; /* ano = 0..99 (20xx) */
  uint8_t hora, min, seg;
} rtc_dt_t;

void rtc_app_init(void);
bool rtc_app_get(rtc_dt_t *dt);
uint32_t rtc_app_data_ddmmaa(void); /* DDMMAA */
uint32_t rtc_app_hora_hhmmss(void); /* HHMMSS */
uint16_t rtc_app_minuto_do_dia(void); /* 0..1439 */

bool rtc_app_set(const rtc_dt_t *dt);
bool rtc_app_set_compacto(uint32_t data_ddmmaa, uint32_t hora_hhmmss);

/* Ajustes por campo (comandos serial d/e/a/h/i do Ummi Manager). */
bool rtc_app_set_dia(uint8_t dia);
bool rtc_app_set_mes(uint8_t mes);
bool rtc_app_set_ano(uint8_t ano);
bool rtc_app_set_hora(uint8_t hora);
bool rtc_app_set_minuto(uint8_t minuto); /* zera segundos */

uint8_t rtc_dias_no_mes(uint8_t mes, uint8_t ano);
bool rtc_app_relogio_valido(void);

#endif /* INC_RTC_APP_H_ */
