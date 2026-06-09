/*
 * rtc_app.c — RTC interno do STM32 (LSE 32.768 kHz).
 */
#include "rtc_app.h"
#include "stm32f4xx_hal.h"

extern RTC_HandleTypeDef hrtc;

#define RTC_BKP_MAGIC 0x32A5U
#define RTC_BKP_REG RTC_BKP_DR0

uint8_t rtc_dias_no_mes(uint8_t mes, uint8_t ano) {
  static const uint8_t dias[12] = {31, 28, 31, 30, 31, 30,
                                    31, 31, 30, 31, 30, 31};
  uint32_t anof = 2000U + ano;
  if (mes < 1U || mes > 12U) {
    return 0U;
  }
  if (mes == 2U && ((anof % 4U == 0U && anof % 100U != 0U) ||
                    (anof % 400U == 0U))) {
    return 29U;
  }
  return dias[mes - 1U];
}

static bool dt_valido(const rtc_dt_t *dt) {
  if (dt->mes < 1U || dt->mes > 12U) return false;
  if (dt->ano > 99U) return false;
  if (dt->dia < 1U || dt->dia > rtc_dias_no_mes(dt->mes, dt->ano)) return false;
  if (dt->hora > 23U || dt->min > 59U || dt->seg > 59U) return false;
  return true;
}

void rtc_app_init(void) {
  /* Marca o dominio backup como inicializado para detectar perda de hora. */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_REG) != RTC_BKP_MAGIC) {
    /* RTC nao inicializado (primeira energizacao / sem VBAT): data padrao. */
    rtc_dt_t def = {1U, 1U, 26U, 0U, 0U, 0U};
    (void)rtc_app_set(&def);
  }
}

bool rtc_app_relogio_valido(void) {
  return HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_REG) == RTC_BKP_MAGIC;
}

bool rtc_app_get(rtc_dt_t *dt) {
  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};

  if (HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) return false;
  if (HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK) return false;

  dt->hora = t.Hours;
  dt->min = t.Minutes;
  dt->seg = t.Seconds;
  dt->dia = d.Date;
  dt->mes = d.Month;
  dt->ano = d.Year;
  return true;
}

bool rtc_app_set(const rtc_dt_t *dt) {
  RTC_TimeTypeDef t = {0};
  RTC_DateTypeDef d = {0};

  if (!dt_valido(dt)) return false;

  t.Hours = dt->hora;
  t.Minutes = dt->min;
  t.Seconds = dt->seg;
  t.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  t.StoreOperation = RTC_STOREOPERATION_RESET;
  d.Date = dt->dia;
  d.Month = dt->mes;
  d.Year = dt->ano;
  d.WeekDay = RTC_WEEKDAY_MONDAY;

  if (HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN) != HAL_OK) return false;
  if (HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN) != HAL_OK) return false;

  HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_REG, RTC_BKP_MAGIC);
  return true;
}

uint32_t rtc_app_data_ddmmaa(void) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return 0U;
  return (uint32_t)dt.dia * 10000U + (uint32_t)dt.mes * 100U + dt.ano;
}

uint32_t rtc_app_hora_hhmmss(void) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return 0U;
  return (uint32_t)dt.hora * 10000U + (uint32_t)dt.min * 100U + dt.seg;
}

uint16_t rtc_app_minuto_do_dia(void) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return 0U;
  return (uint16_t)((uint16_t)dt.hora * 60U + dt.min);
}

bool rtc_app_set_compacto(uint32_t data_ddmmaa, uint32_t hora_hhmmss) {
  rtc_dt_t dt;
  dt.dia = (uint8_t)(data_ddmmaa / 10000U);
  dt.mes = (uint8_t)((data_ddmmaa / 100U) % 100U);
  dt.ano = (uint8_t)(data_ddmmaa % 100U);
  dt.hora = (uint8_t)(hora_hhmmss / 10000U);
  dt.min = (uint8_t)((hora_hhmmss / 100U) % 100U);
  dt.seg = (uint8_t)(hora_hhmmss % 100U);
  return rtc_app_set(&dt);
}

bool rtc_app_set_dia(uint8_t dia) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return false;
  dt.dia = dia;
  return rtc_app_set(&dt);
}
bool rtc_app_set_mes(uint8_t mes) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return false;
  dt.mes = mes;
  if (dt.dia > rtc_dias_no_mes(dt.mes, dt.ano)) {
    dt.dia = rtc_dias_no_mes(dt.mes, dt.ano);
  }
  return rtc_app_set(&dt);
}
bool rtc_app_set_ano(uint8_t ano) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return false;
  dt.ano = ano;
  if (dt.dia > rtc_dias_no_mes(dt.mes, dt.ano)) {
    dt.dia = rtc_dias_no_mes(dt.mes, dt.ano);
  }
  return rtc_app_set(&dt);
}
bool rtc_app_set_hora(uint8_t hora) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return false;
  dt.hora = hora;
  return rtc_app_set(&dt);
}
bool rtc_app_set_minuto(uint8_t minuto) {
  rtc_dt_t dt;
  if (!rtc_app_get(&dt)) return false;
  dt.min = minuto;
  /* preserva os segundos correntes (ajuste de campo, nao reinicia o relogio) */
  return rtc_app_set(&dt);
}
