/*
 * control.c — Controle de reles, buzzer, eventos horarios e alarmes.
 */
#include "control.h"
#include "config.h"
#include "sensors.h"
#include "rtc_app.h"
#include "defines.h"
#include "main.h"
#include "stm32f4xx_hal.h"

/* Tempo minimo entre comutacoes do mesmo rele (anti-chattering), em ticks 50ms. */
#define COMUTACAO_MIN_50MS 100U /* 5 s */
/* Tempo de estabilizacao do alarme de temperatura baixa, em ticks 50ms. */
#define ESTAB_BAIXA_50MS 6000U /* 5 min */

static bool rele1_estado;
static bool rele2_estado;
static bool buzzer_estado;

static bool alm_t_alta, alm_t_baixa, alm_ur_alta, alm_ur_baixa;
static bool falha_t, falha_ur, falha_alim;

/* estados latch por fonte (histerese) */
static bool r1_temp_on, r1_ur_on, r2_temp_on, r2_ur_on;
static uint16_t r1_comut_to, r2_comut_to;
static uint16_t estab_baixa_to = ESTAB_BAIXA_50MS;

/* cadencia do buzzer */
static uint16_t buzzer_div;

/* inibicao geral das saidas (estado seguro durante o menu) */
static bool saidas_inibidas;

static void escreve_pino(GPIO_TypeDef *port, uint16_t pin, bool on) {
  HAL_GPIO_WritePin(port, pin, on ? HIGH : LOW);
}

/* histerese de 2 pontos. retorna novo estado latch. */
static bool histerese(bool atual, int32_t med, const controle_grandeza_t *g) {
  int32_t liga_baixo = g->setpoint - g->hn;  /* aquecer/umidif liga aqui */
  int32_t desliga_alto = g->setpoint + g->hp;
  bool trava_off = (g->hn == 0 && g->hp == 0); /* R07: nunca desliga */

  if (g->logica == RELE_LOGICA_AQUECER) {
    if (!atual && med <= liga_baixo) return true;
    if (atual && med >= desliga_alto && !trava_off) return false;
  } else { /* REFRIGERAR */
    if (!atual && med >= desliga_alto) return true;
    if (atual && med <= liga_baixo && !trava_off) return false;
  }
  return atual;
}

/* evento horario ativo? suporta cruzar meia-noite. */
static bool evento_ativo(const evento_horario_t *ev, uint16_t m) {
  if (ev->liga_min == 0xFFFFU || ev->liga_min == ev->desliga_min) return false;
  if (ev->liga_min < ev->desliga_min) {
    return (m >= ev->liga_min) && (m < ev->desliga_min);
  }
  /* cruza meia-noite */
  return (m >= ev->liga_min) || (m < ev->desliga_min);
}

static bool algum_evento_ativo(const evento_horario_t *evs, uint16_t m) {
  for (uint8_t i = 0; i < EVENTOS_POR_FONTE; i++) {
    if (evento_ativo(&evs[i], m)) return true;
  }
  return false;
}

static void atualiza_falhas(void) {
  int32_t t = sensors_temp_C();

  falha_t = sensors_adc_erro() || sensors_temp_desconectada() ||
            !sensors_estavel() || t < -50 || t > 999;
  falha_ur = sensors_umidade_desconectada() || sensors_umidade_fora_faixa();
  falha_alim = (sensors_v5_mV() < g_config.v5_min_mV) ||
               (sensors_v5_mV() > g_config.v5_max_mV) ||
               (sensors_v12_mV() < g_config.v12_min_mV) ||
               (sensors_v12_mV() > g_config.v12_max_mV);
}

static void atualiza_alarmes(void) {
  int32_t t = sensors_temp_C();
  int32_t ur = (int32_t)sensors_umidade_pct();

  alm_t_alta = false;
  alm_t_baixa = false;
  if (!falha_t) {
    if ((g_config.alm_temp == ALM_ALTA || g_config.alm_temp == ALM_AMBOS) &&
        t >= g_config.alm_temp_alta_dC) {
      alm_t_alta = true;
    }
    if ((g_config.alm_temp == ALM_BAIXA || g_config.alm_temp == ALM_AMBOS)) {
      if (t > g_config.alm_temp_baixa_dC) {
        estab_baixa_to = ESTAB_BAIXA_50MS;
      } else if (estab_baixa_to == 0U) {
        alm_t_baixa = true;
      }
    } else {
      estab_baixa_to = ESTAB_BAIXA_50MS;
    }
  } else {
    estab_baixa_to = ESTAB_BAIXA_50MS;
  }

  alm_ur_alta = false;
  alm_ur_baixa = false;
  if (!falha_ur) {
    if ((g_config.alm_ur == ALM_ALTA || g_config.alm_ur == ALM_AMBOS) &&
        ur >= g_config.alm_ur_alta_dUR) {
      alm_ur_alta = true;
    }
    if ((g_config.alm_ur == ALM_BAIXA || g_config.alm_ur == ALM_AMBOS) &&
        ur <= g_config.alm_ur_baixa_dUR) {
      alm_ur_baixa = true;
    }
  }
}

void control_init(void) {
  rele1_estado = false;
  rele2_estado = false;
  buzzer_estado = false;
  r1_temp_on = r1_ur_on = r2_temp_on = r2_ur_on = false;
  r1_comut_to = r2_comut_to = 0;
  estab_baixa_to = ESTAB_BAIXA_50MS;
  escreve_pino(SAIDA_01_PORT, SAIDA_01, false);
  escreve_pino(SAIDA_02_PORT, SAIDA_02, false);
  escreve_pino(BUZZER_PORT, BUZZER, false);
}

void control_inibe(bool inibir) { saidas_inibidas = inibir; }

void control_tick_50ms(void) {
  uint16_t minuto = rtc_app_minuto_do_dia();
  bool novo1, novo2;

  /* Estado seguro: enquanto inibido (ex.: menu), tudo desligado. */
  if (saidas_inibidas) {
    rele1_estado = rele2_estado = false;
    buzzer_estado = false;
    r1_temp_on = r1_ur_on = r2_temp_on = r2_ur_on = false;
    escreve_pino(SAIDA_01_PORT, SAIDA_01, false);
    escreve_pino(SAIDA_02_PORT, SAIDA_02, false);
    escreve_pino(BUZZER_PORT, BUZZER, false);
    escreve_pino(LED_01_PORT, LED_01, false);
    escreve_pino(LED_02_PORT, LED_02, false);
    return;
  }

  if (r1_comut_to > 0U) r1_comut_to--;
  if (r2_comut_to > 0U) r2_comut_to--;
  if (estab_baixa_to > 0U) estab_baixa_to--;

  atualiza_falhas();
  atualiza_alarmes();

  /* Rele 1 */
  {
    int32_t t = sensors_temp_C();
    int32_t ur = (int32_t)sensors_umidade_pct();
    bool evento = algum_evento_ativo(g_config.rele1.eventos, minuto);
    bool fonte_temp = false, fonte_ur = false;

    if (g_config.rele1.temperatura.habilita) {
      r1_temp_on = falha_t ? false
                           : histerese(r1_temp_on, t, &g_config.rele1.temperatura);
      /* Seguranca: alarme de temperatura alta inibe o aquecimento (corte
       * independente da histerese, mesmo no modo trava). */
      if (alm_t_alta &&
          g_config.rele1.temperatura.logica == RELE_LOGICA_AQUECER) {
        r1_temp_on = false;
      }
      fonte_temp = r1_temp_on;
    }
    if (g_config.rele1.umidade.habilita) {
      r1_ur_on = falha_ur ? false
                          : histerese(r1_ur_on, ur, &g_config.rele1.umidade);
      fonte_ur = r1_ur_on;
    }
    novo1 = evento || fonte_temp || fonte_ur;
    if (falha_alim) novo1 = false;

    if (novo1 != rele1_estado) {
      if (!novo1 && falha_alim) {
        rele1_estado = false; /* OFF seguro imediato */
        r1_comut_to = COMUTACAO_MIN_50MS;
      } else if (r1_comut_to == 0U) {
        rele1_estado = novo1;
        r1_comut_to = COMUTACAO_MIN_50MS;
      }
    }
  }

  /* Rele 2 */
  {
    int32_t t = sensors_temp_C();
    int32_t ur = (int32_t)sensors_umidade_pct();
    bool evento = algum_evento_ativo(g_config.rele2.eventos, minuto);
    bool fonte_temp = false, fonte_ur = false;

    if (g_config.rele2.temperatura.habilita) {
      r2_temp_on = falha_t ? false
                           : histerese(r2_temp_on, t, &g_config.rele2.temperatura);
      /* Seguranca: alarme de temperatura alta inibe o aquecimento. */
      if (alm_t_alta &&
          g_config.rele2.temperatura.logica == RELE_LOGICA_AQUECER) {
        r2_temp_on = false;
      }
      fonte_temp = r2_temp_on;
    }
    if (g_config.rele2.umidade.habilita) {
      r2_ur_on = falha_ur ? false
                          : histerese(r2_ur_on, ur, &g_config.rele2.umidade);
      fonte_ur = r2_ur_on;
    }
    novo2 = evento || fonte_temp || fonte_ur;
    if (falha_alim) novo2 = false;

    if (novo2 != rele2_estado) {
      if (!novo2 && falha_alim) {
        rele2_estado = false;
        r2_comut_to = COMUTACAO_MIN_50MS;
      } else if (r2_comut_to == 0U) {
        rele2_estado = novo2;
        r2_comut_to = COMUTACAO_MIN_50MS;
      }
    }
  }

  escreve_pino(SAIDA_01_PORT, SAIDA_01, rele1_estado);
  escreve_pino(SAIDA_02_PORT, SAIDA_02, rele2_estado);

  /* ----- Buzzer ----- */
  {
    bool alarme = alm_t_alta || alm_t_baixa || alm_ur_alta || alm_ur_baixa ||
                  falha_t || falha_ur || falha_alim ||
                  algum_evento_ativo(g_config.alm_eventos, minuto);
    bool saida;

    if (!alarme) {
      buzzer_div = 0;
      buzzer_estado = false;
    } else {
      uint16_t periodo; /* em ticks de 50ms (meio-periodo) */
      switch (g_config.alm_tipo) {
      case BUZZER_CONTINUO:
        periodo = 0;
        break;
      case BUZZER_LENTO:
        periodo = 20; /* 1 s on / 1 s off */
        break;
      case BUZZER_MEDIO:
        periodo = 10; /* 0,5 s */
        break;
      case BUZZER_RAPIDO:
      default:
        periodo = 4; /* 0,2 s */
        break;
      }
      if (periodo == 0U) {
        buzzer_estado = true;
      } else {
        buzzer_div++;
        if (buzzer_div >= (uint16_t)(2U * periodo)) buzzer_div = 0;
        buzzer_estado = (buzzer_div < periodo);
      }
    }
    saida = buzzer_estado;
    escreve_pino(BUZZER_PORT, BUZZER, saida);
  }

  /* ----- LEDs de status ----- */
  escreve_pino(LED_01_PORT, LED_01, rele1_estado);
  escreve_pino(LED_02_PORT, LED_02, rele2_estado);
  escreve_pino(LED_03_PORT, LED_03,
               alm_t_alta || alm_t_baixa || alm_ur_alta || alm_ur_baixa);
  escreve_pino(LED_04_PORT, LED_04, falha_t || falha_ur || falha_alim);
}

bool control_rele1(void) { return rele1_estado; }
bool control_rele2(void) { return rele2_estado; }
bool control_buzzer_ativo(void) { return buzzer_estado; }
bool control_alm_temp_alta(void) { return alm_t_alta; }
bool control_alm_temp_baixa(void) { return alm_t_baixa; }
bool control_alm_ur_alta(void) { return alm_ur_alta; }
bool control_alm_ur_baixa(void) { return alm_ur_baixa; }
bool control_falha_sensor_temp(void) { return falha_t; }
bool control_falha_sensor_ur(void) { return falha_ur; }
bool control_falha_alimentacao(void) { return falha_alim; }

uint16_t control_status_flags(void) {
  return (uint16_t)((rele1_estado ? 0x0001U : 0U) |
                    (rele2_estado ? 0x0002U : 0U) |
                    (buzzer_estado ? 0x0004U : 0U) |
                    (sensors_temp_desconectada() ? 0x0010U : 0U) |
                    (sensors_umidade_desconectada() ? 0x0020U : 0U) |
                    (sensors_umidade_fora_faixa() ? 0x0040U : 0U) |
                    (alm_t_alta ? 0x0100U : 0U) |
                    (alm_t_baixa ? 0x0200U : 0U) |
                    (alm_ur_alta ? 0x0400U : 0U) |
                    (alm_ur_baixa ? 0x0800U : 0U) |
                    (falha_alim ? 0x1000U : 0U));
}
