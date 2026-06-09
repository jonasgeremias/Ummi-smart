/*
 * ui.c — Interface de usuario do Ummi Control.
 */
#include "ui.h"
#include "config.h"
#include "sensors.h"
#include "rtc_app.h"
#include "datalog.h"
#include "display.h"
#include "timer.h"
#include "control.h"
#include "stm32f4xx_hal.h"

/* ---- estados de UI ---- */
enum { UI_TERMOMETRO = 0, UI_RELOGIO, UI_MENU, UI_TESTE };

/* ---- itens de menu (Pnn). Cobrem todo o pacote de configuracao da serial. ---- */
enum {
  /* Rele 1 - Temperatura: habilita, logica, setpoint, HN, HP */
  IT_R1T_HAB = 0, IT_R1T_LOG, IT_R1T_SP, IT_R1T_HN, IT_R1T_HP,
  /* Rele 1 - Umidade */
  IT_R1U_HAB, IT_R1U_LOG, IT_R1U_SP, IT_R1U_HN, IT_R1U_HP,
  /* Rele 2 - Temperatura */
  IT_R2T_HAB, IT_R2T_LOG, IT_R2T_SP, IT_R2T_HN, IT_R2T_HP,
  /* Rele 2 - Umidade */
  IT_R2U_HAB, IT_R2U_LOG, IT_R2U_SP, IT_R2U_HN, IT_R2U_HP,
  /* Alarme temperatura: modo, limite alto, limite baixo */
  IT_ALT_MODO, IT_ALT_ALTA, IT_ALT_BAIXA,
  /* Alarme umidade */
  IT_ALU_MODO, IT_ALU_ALTA, IT_ALU_BAIXA,
  /* Buzzer: tipo de toque */
  IT_BUZ,
  /* Calibracao UR */
  IT_ZUR, IT_GUR,
  /* Datalogger: habilita, base, intervalo, reset */
  IT_LOG_HAB, IT_LOG_BASE, IT_LOG_INT, IT_RST,
  /* Restaurar configuracao de fabrica (limpa GUR/ZUR e todo o resto) */
  IT_DEF,
  /* Exibicao: escala C/F */
  IT_ESC,
  /* EEPROM externa (24LC256 / 24AA512) */
  IT_EEPROM,
  /* RTC */
  IT_DAT, IT_HOR,
  IT_QTD
};

/* Tipo de cada item (define edicao e renderizacao). */
enum { T_NUM = 0, T_ENUM, T_ESCALA, T_RTCDATA, T_RTCHORA, T_ACTION, T_ZG };

typedef struct {
  uint8_t tipo;
  uint8_t lbl[3]; /* mnemonico de 3 digitos (tela de navegacao e edicao) */
  uint8_t nenum;  /* nro de estados (T_ENUM/T_ESCALA/T_ACTION) */
} menu_def_t;

static const menu_def_t menu_tab[IT_QTD] = {
  /* Rele 1 Temp */
  {T_ENUM, {DSP_1, DSP_T, DSP_H}, 2}, /* habilita */
  {T_ENUM, {DSP_1, DSP_T, DSP_L}, 2}, /* logica   */
  {T_NUM,  {DSP_1, DSP_T, DSP_P}, 0}, /* setpoint */
  {T_NUM,  {DSP_1, DSP_T, DSP_N}, 0}, /* HN       */
  {T_NUM,  {DSP_1, DSP_T, DSP_A}, 0}, /* HP       */
  /* Rele 1 Umidade */
  {T_ENUM, {DSP_1, DSP_U, DSP_H}, 2},
  {T_ENUM, {DSP_1, DSP_U, DSP_L}, 2},
  {T_NUM,  {DSP_1, DSP_U, DSP_P}, 0},
  {T_NUM,  {DSP_1, DSP_U, DSP_N}, 0},
  {T_NUM,  {DSP_1, DSP_U, DSP_A}, 0},
  /* Rele 2 Temp */
  {T_ENUM, {DSP_2, DSP_T, DSP_H}, 2},
  {T_ENUM, {DSP_2, DSP_T, DSP_L}, 2},
  {T_NUM,  {DSP_2, DSP_T, DSP_P}, 0},
  {T_NUM,  {DSP_2, DSP_T, DSP_N}, 0},
  {T_NUM,  {DSP_2, DSP_T, DSP_A}, 0},
  /* Rele 2 Umidade */
  {T_ENUM, {DSP_2, DSP_U, DSP_H}, 2},
  {T_ENUM, {DSP_2, DSP_U, DSP_L}, 2},
  {T_NUM,  {DSP_2, DSP_U, DSP_P}, 0},
  {T_NUM,  {DSP_2, DSP_U, DSP_N}, 0},
  {T_NUM,  {DSP_2, DSP_U, DSP_A}, 0},
  /* Alarme temperatura */
  {T_ENUM, {DSP_A, DSP_T, DSP_D}, 4}, /* modo */
  {T_NUM,  {DSP_A, DSP_T, DSP_A}, 0}, /* alta */
  {T_NUM,  {DSP_A, DSP_T, DSP_B}, 0}, /* baixa */
  /* Alarme umidade */
  {T_ENUM, {DSP_A, DSP_U, DSP_D}, 4},
  {T_NUM,  {DSP_A, DSP_U, DSP_A}, 0},
  {T_NUM,  {DSP_A, DSP_U, DSP_B}, 0},
  /* Buzzer */
  {T_ENUM, {DSP_B, DSP_U, DSP_Z}, 4},
  /* Calibracao UR */
  {T_ZG, {DSP_Z, DSP_U, DSP_R}, 0},
  {T_ZG, {DSP_G, DSP_U, DSP_R}, 0},
  /* Datalogger */
  {T_ENUM,   {DSP_L, DSP_G, DSP_H}, 2}, /* habilita */
  {T_ENUM,   {DSP_L, DSP_G, DSP_B}, 3}, /* base s/m/h */
  {T_NUM,    {DSP_L, DSP_G, DSP_I}, 0}, /* intervalo */
  {T_ACTION, {DSP_R, DSP_S, DSP_T}, 2}, /* reset datalogger */
  /* Restaurar padroes de fabrica */
  {T_ACTION, {DSP_D, DSP_E, DSP_F}, 2}, /* dEF */
  /* Exibicao */
  {T_ESCALA, {DSP_E, DSP_S, DSP_C}, 2}, /* C/F */
  /* EEPROM */
  {T_ENUM, {DSP_E, DSP_E, DSP_P}, 2},
  /* RTC */
  {T_RTCDATA, {DSP_D, DSP_A, DSP_T}, 0},
  {T_RTCHORA, {DSP_H, DSP_O, DSP_R}, 0},
};

static uint8_t ui_modo;
static uint8_t menu_item;
static uint8_t menu_editando;
static uint8_t menu_bloqueia_soltura;
static uint8_t menu_rtc_campo;
static uint8_t menu_rtc_visu_to;
static uint8_t reset_log_confirm;
static uint8_t def_confirm; /* confirmacao do restaurar padroes */

/* alternancia automatica Term/Relogio */
static uint16_t alterna_to;
/* exibicao momentanea de data/maxmin */
static uint8_t momentaneo_to;
static uint8_t momentaneo_tipo; /* 0=nenhum 1=data 2=maxT 3=minT 4=maxU 5=minU */

/* maximos/minimos */
static int32_t max_temp_dC, min_temp_dC;
static uint16_t max_ur_dUR, min_ur_dUR;
static uint8_t maxmin_init;

/* botao relogio segurado (entrar/sair menu, e ajuste RTC no modo relogio) */
static uint16_t tac_relogio_50ms;
/* deteccao de 5x botao HORA(+) para modo teste */
static uint8_t teste_clicks;
static uint8_t teste_solto;
static uint16_t teste_to;
static uint8_t teste_pagina; /* 0=12V/5V 1=AD umidade 2=AD temperatura 3=AD ref */

static int32_t clamp(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

/* ---- splash helper ---- */
static void splash(uint16_t to_50ms, uint8_t d5, uint8_t d4, uint8_t d3,
                   uint8_t d2, uint8_t d1, uint8_t d0) {
  splash_timeout = to_50ms;
  splash_digits[0] = d0;
  splash_digits[1] = d1;
  splash_digits[2] = d2;
  splash_digits[3] = d3;
  splash_digits[4] = d4;
  splash_digits[5] = d5;
}

/* Mostra a marca/versao imediatamente no boot (antes das inits longas), para
 * o display nao piscar valores default. Timeout longo; sobreposto depois pelo
 * ui_splash_inicial. Nao bloqueia. */
void ui_splash_marca(void) {
  splash(200, DSP_B, DSP_E, DSP_1, DSP_OFF, (uint8_t)FW_VERSAO_MAJOR,
         (uint8_t)FW_VERSAO_MINOR);
  display_set_decimal_points(200, 0x02);
}

void ui_splash_inicial(void) {
  /* Marca "BE1" + versao FW_VERSAO_MAJOR.FW_VERSAO_MINOR -> "BE1 1.0".
   * Ponto decimal (0x02) fica na dezena (d1 = major), formando "1.0". */
  splash(24, DSP_B, DSP_E, DSP_1, DSP_OFF, (uint8_t)FW_VERSAO_MAJOR,
         (uint8_t)FW_VERSAO_MINOR);
  display_set_decimal_points(24, 0x02);
  HAL_Delay(1200);
  display_set_decimal_points(2, 0);
  splash_timeout = 0;
  display_atualiza(rtc_app_data_ddmmaa());
  HAL_Delay(1200);
  display_atualiza(rtc_app_hora_hhmmss());
  HAL_Delay(1200);
}

void ui_init(void) {
  ui_modo = UI_TERMOMETRO;
  menu_item = 0;
  menu_editando = 0;
  maxmin_init = 0;
  teste_solto = 1;
  alterna_to = 100; /* ~5 s */
}

/* ============== maximos/minimos ============== */
static void atualiza_maxmin(void) {
  if (!sensors_estavel()) return;
  if (sensors_temp_desconectada() && sensors_umidade_desconectada()) return;

  int32_t t = sensors_temp_C();      /* grau inteiro */
  uint16_t u = sensors_umidade_pct(); /* % inteiro */

  if (!maxmin_init) {
    max_temp_dC = min_temp_dC = t;
    max_ur_dUR = min_ur_dUR = u;
    maxmin_init = 1;
    return;
  }
  if (!sensors_temp_desconectada()) {
    if (t > max_temp_dC) max_temp_dC = t;
    if (t < min_temp_dC) min_temp_dC = t;
  }
  if (!sensors_umidade_desconectada()) {
    if (u > max_ur_dUR) max_ur_dUR = u;
    if (u < min_ur_dUR) min_ur_dUR = u;
  }
}

static void zera_maxmin(void) {
  maxmin_init = 0;
  splash(8, DSP_MINUS, DSP_R, DSP_S, DSP_T, DSP_MINUS, DSP_OFF);
}

/* Valor do termometro exibido com taxa reduzida (legibilidade ~2x/s). */
static uint32_t disp_t, disp_u;
static uint8_t disp_tdesc, disp_udesc;
static uint16_t disp_refresh_to;

/* ============== render principal (Termometro) ============== */
static void render_termometro(void) {
  uint32_t u, t;

  if (momentaneo_to > 0U && momentaneo_tipo != 0U) {
    int32_t valt = 0;
    uint16_t valu = 0;
    switch (momentaneo_tipo) {
    case 1: /* data */
      display_atualiza(rtc_app_data_ddmmaa());
      display_set_decimal_points(2, 0x14);
      return;
    case 2: valt = max_temp_dC; break;
    case 3: valt = min_temp_dC; break;
    case 4: valu = max_ur_dUR; break;
    case 5: valu = min_ur_dUR; break;
    default: break;
    }
    if (momentaneo_tipo == 2 || momentaneo_tipo == 3) {
      uint32_t tt = (valt < 0) ? 0U : (uint32_t)valt; /* grau inteiro */
      if (tt > 999U) tt = 999U;
      display_set_digits(momentaneo_tipo == 2 ? DSP_A : DSP_B, DSP_OFF, DSP_OFF,
                         (tt / 100U) % 10U, (tt / 10U) % 10U, tt % 10U);
      display_set_decimal_points(2, 0); /* temperatura sem casa decimal */
    } else {
      uint32_t uu = (uint32_t)valu; /* % inteiro */
      if (uu > 999U) uu = 999U;
      display_set_digits(momentaneo_tipo == 4 ? DSP_A : DSP_B, DSP_OFF, DSP_OFF,
                         (uu / 100U) % 10U, (uu / 10U) % 10U, uu % 10U);
      display_set_decimal_points(2, 0); /* UR inteira: sem ponto */
    }
    return;
  }

  if (!sensors_estavel()) {
    display_set_digits(DSP_MINUS, DSP_MINUS, DSP_MINUS, DSP_MINUS, DSP_MINUS,
                       DSP_MINUS);
    display_set_decimal_points(2, 0);
    return;
  }

  /* Taxa reduzida: o NUMERO so e re-amostrado a cada ~500 ms (evita piscar).
   * O controle/relés continuam usando o valor rapido. */
  if (disp_refresh_to == 0U) {
    int32_t temp = g_config.escala_celsius ? sensors_temp_C() : sensors_temp_F();
    uint32_t tt = (temp < 0) ? 0U : (uint32_t)temp;
    uint32_t uu = sensors_umidade_pct(); /* % inteiro */
    disp_t = (tt > 999U) ? 999U : tt;
    disp_u = (uu > 999U) ? 999U : uu;
    disp_tdesc = sensors_temp_desconectada();
    disp_udesc = sensors_umidade_desconectada();
    disp_refresh_to = 10U; /* 10 x 50 ms = 500 ms */
  }
  t = disp_t;
  u = disp_u;
  uint8_t t_desc = disp_tdesc;
  uint8_t u_desc = disp_udesc;

  /* trio esquerdo = umidade, trio direito = temperatura */
  display_set_digits(u_desc ? DSP_MINUS : ((u / 100U) % 10U),
                     u_desc ? DSP_MINUS : ((u / 10U) % 10U),
                     u_desc ? DSP_MINUS : (u % 10U),
                     t_desc ? DSP_MINUS : ((t / 100U) % 10U),
                     t_desc ? DSP_MINUS : ((t / 10U) % 10U),
                     t_desc ? DSP_MINUS : (t % 10U));
  /* umidade e temperatura inteiras: sem ponto decimal */
  display_set_decimal_points(2, 0);
}

static void render_relogio(void) {
  display_atualiza(rtc_app_hora_hhmmss());
  display_set_decimal_points(2, 0x14); /* HH.MM.SS */
}

/* ============== modo teste (5V/12V e analogica pura) ============== */
/* Mostra "letra" + 5 digitos (valor 0..99999) sem ponto decimal. */
static void teste_mostra_ad(uint8_t letra, uint16_t v) {
  /* v e o ADC bruto (0..4095); cabe nos 5 digitos sem clamp */
  display_set_digits(letra, (uint32_t)(v / 10000U) % 10U,
                     (uint32_t)(v / 1000U) % 10U, (uint32_t)(v / 100U) % 10U,
                     (uint32_t)(v / 10U) % 10U, (uint32_t)v % 10U);
  display_set_decimal_points(2, 0);
}

static void render_teste(void) {
  switch (teste_pagina) {
  case 1: teste_mostra_ad(DSP_U, sensors_ad_umidade()); return;    /* uXXXXX */
  case 2: teste_mostra_ad(DSP_T, sensors_ad_temperatura()); return;/* tXXXXX */
  case 3: teste_mostra_ad(DSP_R, sensors_ad_referencia()); return; /* rXXXXX */
  default: break;
  }
  /* pagina 0: tensoes 12V / 5V */
  uint32_t v12 = ((uint32_t)sensors_v12_mV() + 50U) / 100U; /* dV */
  uint32_t v5 = ((uint32_t)sensors_v5_mV() + 50U) / 100U;
  if (v12 > 999U) v12 = 999U;
  if (v5 > 999U) v5 = 999U;
  display_set_digits((v12 / 100U) % 10U, (v12 / 10U) % 10U, v12 % 10U,
                     (v5 / 100U) % 10U, (v5 / 10U) % 10U, v5 % 10U);
  display_set_decimal_points(2, 0x12); /* 12.0 05.0 */
}

static uint32_t menu_rtc_data; /* edicao temporaria */
static uint32_t menu_rtc_hora;

/* Resolve um item de rele para a grandeza/campo correspondente.
 * campo: 0=habilita 1=logica 2=setpoint 3=HN 4=HP. is_ur=1 para umidade. */
static controle_grandeza_t *grandeza_de(uint8_t item, uint8_t *campo,
                                        uint8_t *is_ur) {
  if (item > IT_R2U_HP) return NULL; /* itens de rele sao os 20 primeiros */
  uint8_t g = (uint8_t)(item / 5U); /* 0=R1T 1=R1U 2=R2T 3=R2U */
  *campo = (uint8_t)(item % 5U);
  *is_ur = (uint8_t)(g & 1U);
  switch (g) {
  case 0: return &g_config.rele1.temperatura;
  case 1: return &g_config.rele1.umidade;
  case 2: return &g_config.rele2.temperatura;
  default: return &g_config.rele2.umidade;
  }
}

/* ============== menu: valor atual (na unidade exibida) ============== */
static int32_t menu_get(void) {
  uint8_t campo, is_ur;
  controle_grandeza_t *g = grandeza_de(menu_item, &campo, &is_ur);
  if (g != NULL) {
    switch (campo) {
    case 0: return g->habilita;
    case 1: return g->logica;
    case 2: return g->setpoint; /* temp em grau, UR em % (ambos inteiros) */
    case 3: return g->hn;
    default: return g->hp;
    }
  }
  switch (menu_item) {
  case IT_ALT_MODO: return g_config.alm_temp;
  case IT_ALT_ALTA: return g_config.alm_temp_alta_dC;
  case IT_ALT_BAIXA: return g_config.alm_temp_baixa_dC;
  case IT_ALU_MODO: return g_config.alm_ur;
  case IT_ALU_ALTA: return g_config.alm_ur_alta_dUR; /* % inteiro */
  case IT_ALU_BAIXA: return g_config.alm_ur_baixa_dUR;
  case IT_BUZ: return g_config.alm_tipo;
  case IT_ZUR: return (int32_t)g_config.zur;
  case IT_GUR: return (int32_t)g_config.gur;
  case IT_LOG_HAB: return g_config.log_habilitado;
  case IT_LOG_BASE: return g_config.log_base;
  case IT_LOG_INT: return g_config.log_intervalo;
  case IT_RST: return reset_log_confirm;
  case IT_DEF: return def_confirm;
  case IT_ESC: return g_config.escala_celsius;
  case IT_EEPROM: return g_config.eeprom_tipo;
  case IT_DAT: return (int32_t)rtc_app_data_ddmmaa();
  case IT_HOR: return (int32_t)rtc_app_hora_hhmmss();
  default: return 0;
  }
}

static void menu_set(int32_t v) {
  uint8_t campo, is_ur;
  controle_grandeza_t *g = grandeza_de(menu_item, &campo, &is_ur);
  if (g != NULL) {
    int32_t lim = is_ur ? 100 : TEMP_MAX_GRAU; /* UR 0..100%, temp 0..999 */
    switch (campo) {
    case 0: g->habilita = (v != 0) ? 1U : 0U; break;
    case 1: g->logica = (v != 0) ? 1U : 0U; break;
    case 2: g->setpoint = clamp(v, 0, lim); break;
    /* HN/HP minimo 1: evita HN=HP=0 (trava "nunca desliga") por engano no menu.
     * O modo trava continua acessivel deliberadamente pela serial. */
    case 3: g->hn = clamp(v, 1, lim); break;
    default: g->hp = clamp(v, 1, lim); break;
    }
    return;
  }
  switch (menu_item) {
  case IT_ALT_MODO: g_config.alm_temp = (uint8_t)clamp(v, 0, 3); break;
  case IT_ALT_ALTA:
    g_config.alm_temp_alta_dC = clamp(v, g_config.alm_temp_baixa_dC, TEMP_MAX_GRAU);
    break;
  case IT_ALT_BAIXA:
    g_config.alm_temp_baixa_dC = clamp(v, 0, g_config.alm_temp_alta_dC);
    break;
  case IT_ALU_MODO: g_config.alm_ur = (uint8_t)clamp(v, 0, 3); break;
  case IT_ALU_ALTA:
    g_config.alm_ur_alta_dUR = clamp(v, g_config.alm_ur_baixa_dUR, 100);
    break;
  case IT_ALU_BAIXA:
    g_config.alm_ur_baixa_dUR = clamp(v, 0, g_config.alm_ur_alta_dUR);
    break;
  case IT_BUZ: g_config.alm_tipo = (uint8_t)clamp(v, 0, 3); break;
  case IT_ZUR: g_config.zur = (uint32_t)clamp(v, 0, 65535); break;
  case IT_GUR: g_config.gur = (uint32_t)clamp(v, 1, 65535); break;
  case IT_LOG_HAB: g_config.log_habilitado = (v != 0) ? 1U : 0U; break;
  case IT_LOG_BASE: g_config.log_base = (uint8_t)clamp(v, 0, 2); break;
  case IT_LOG_INT: g_config.log_intervalo = (uint16_t)clamp(v, 1, 3600); break;
  case IT_RST: reset_log_confirm = (v > 0) ? 1U : 0U; break;
  case IT_DEF: def_confirm = (v > 0) ? 1U : 0U; break;
  case IT_ESC: g_config.escala_celsius = (v != 0) ? 1U : 0U; break;
  case IT_EEPROM: g_config.eeprom_tipo = (uint8_t)clamp(v, 0, 1); break;
  default: break;
  }
}

static uint8_t item_rtc(void) {
  uint8_t t = menu_tab[menu_item].tipo;
  return (t == T_RTCDATA || t == T_RTCHORA);
}
static uint8_t rtc_qtd_campos(void) {
  return (menu_tab[menu_item].tipo == T_RTCDATA) ? 3U : 2U;
}
/* itens editados por passo +/-1 com retorno (enum/escala/acao) */
static uint8_t item_step_enum(void) {
  uint8_t t = menu_tab[menu_item].tipo;
  return (t == T_ENUM || t == T_ESCALA || t == T_ACTION);
}
static int32_t item_nenum(void) {
  int32_t n = menu_tab[menu_item].nenum;
  return (n < 1) ? 1 : n;
}

static uint32_t ajusta_circular(uint32_t v, uint32_t mn, uint32_t mx, int8_t d) {
  if (v < mn || v > mx) v = mn;
  if (d > 0) return (v >= mx) ? mn : (v + 1U);
  return (v <= mn) ? mx : (v - 1U);
}

static void menu_rtc_ajusta(int8_t d) {
  if (menu_item == IT_DAT) {
    uint32_t dia = menu_rtc_data / 10000U;
    uint32_t mes = (menu_rtc_data / 100U) % 100U;
    uint32_t ano = menu_rtc_data % 100U;
    if (mes < 1U || mes > 12U) mes = 1U;
    if (ano > 99U) ano = 0U;
    if (menu_rtc_campo == 0U) {
      dia = ajusta_circular(dia, 1U, rtc_dias_no_mes((uint8_t)mes, (uint8_t)ano), d);
    } else if (menu_rtc_campo == 1U) {
      mes = ajusta_circular(mes, 1U, 12U, d);
      if (dia > rtc_dias_no_mes((uint8_t)mes, (uint8_t)ano))
        dia = rtc_dias_no_mes((uint8_t)mes, (uint8_t)ano);
    } else {
      ano = ajusta_circular(ano, 0U, 99U, d);
      if (dia > rtc_dias_no_mes((uint8_t)mes, (uint8_t)ano))
        dia = rtc_dias_no_mes((uint8_t)mes, (uint8_t)ano);
    }
    menu_rtc_data = dia * 10000U + mes * 100U + ano;
  } else {
    uint32_t hora = menu_rtc_hora / 10000U;
    uint32_t min = (menu_rtc_hora / 100U) % 100U;
    if (menu_rtc_campo == 0U) hora = ajusta_circular(hora, 0U, 23U, d);
    else min = ajusta_circular(min, 0U, 59U, d);
    menu_rtc_hora = hora * 10000U + min * 100U;
  }
}

static void menu_lista(void) {
  const uint8_t *lbl = menu_tab[menu_item].lbl;
  uint8_t n = (uint8_t)(menu_item + 1U);
  display_set_digits(DSP_P, (n / 10U) % 10U, n % 10U, lbl[0], lbl[1], lbl[2]);
  display_set_decimal_points(2, 0);
}

/* Texto de 3 digitos para os estados de um item enumerado. */
static void enum_text(uint8_t item, int32_t v, uint8_t o[3]) {
  uint8_t campo, is_ur;
  o[0] = o[1] = o[2] = DSP_OFF;
  if (grandeza_de(item, &campo, &is_ur) != NULL) {
    if (campo == 0) { /* habilita: "on"/"oF" */
      o[0] = DSP_O; o[1] = v ? DSP_N : DSP_F;
    } else {          /* logica: "Aq"(aquecer)/"rE"(refrigerar) */
      if (v) { o[0] = DSP_R; o[1] = DSP_E; }
      else { o[0] = DSP_A; o[1] = DSP_Q; }
    }
    return;
  }
  switch (item) {
  case IT_ALT_MODO:
  case IT_ALU_MODO: /* 0=oF 1=Alt 2=bAI 3=Anb */
    if (v == 0) { o[0] = DSP_O; o[1] = DSP_F; }
    else if (v == 1) { o[0] = DSP_A; o[1] = DSP_L; o[2] = DSP_T; }
    else if (v == 2) { o[0] = DSP_B; o[1] = DSP_A; o[2] = DSP_I; }
    else { o[0] = DSP_A; o[1] = DSP_N; o[2] = DSP_B; }
    break;
  case IT_BUZ: /* 0=Con 1=LEn 2=nED 3=rAP */
    if (v == 0) { o[0] = DSP_C; o[1] = DSP_O; o[2] = DSP_N; }
    else if (v == 1) { o[0] = DSP_L; o[1] = DSP_E; o[2] = DSP_N; }
    else if (v == 2) { o[0] = DSP_N; o[1] = DSP_E; o[2] = DSP_D; }
    else { o[0] = DSP_R; o[1] = DSP_A; o[2] = DSP_P; }
    break;
  case IT_LOG_BASE: /* 0=SEG 1=nIn 2=hor */
    if (v == 0) { o[0] = DSP_S; o[1] = DSP_E; o[2] = DSP_G; }
    else if (v == 1) { o[0] = DSP_N; o[1] = DSP_I; o[2] = DSP_N; }
    else { o[0] = DSP_H; o[1] = DSP_O; o[2] = DSP_R; }
    break;
  case IT_EEPROM: /* 0=256 (24LC256) 1=512 (24AA512) */
    if (v) { o[0] = DSP_5; o[1] = DSP_1; o[2] = DSP_2; }
    else { o[0] = DSP_2; o[1] = DSP_5; o[2] = DSP_6; }
    break;
  default: /* on/oF (habilita log, exib, reset) */
    o[0] = DSP_O; o[1] = v ? DSP_N : DSP_F;
    break;
  }
}

static void menu_edicao_rtc(uint32_t valor) {
  uint8_t dg[6] = {(uint8_t)((valor / 100000U) % 10U),
                   (uint8_t)((valor / 10000U) % 10U),
                   (uint8_t)((valor / 1000U) % 10U),
                   (uint8_t)((valor / 100U) % 10U),
                   (uint8_t)((valor / 10U) % 10U), (uint8_t)(valor % 10U)};
  uint8_t apagar = 0U;
  uint8_t ini = (uint8_t)(menu_rtc_campo * 2U);
  if (menu_rtc_visu_to == 0U) apagar = ((HAL_GetTick() / 500U) & 1U) ? 1U : 0U;
  if (apagar) {
    dg[ini] = DSP_OFF;
    dg[ini + 1U] = DSP_OFF;
  }
  display_set_digits(dg[0], dg[1], dg[2], dg[3], dg[4], dg[5]);
  display_set_decimal_points(2, 0);
}

static void menu_edicao(void) {
  uint8_t tipo = menu_tab[menu_item].tipo;
  const uint8_t *lbl = menu_tab[menu_item].lbl;
  int32_t v = menu_get();

  if (tipo == T_RTCDATA) { menu_edicao_rtc(menu_rtc_data); return; }
  if (tipo == T_RTCHORA) { menu_edicao_rtc(menu_rtc_hora); return; }

  if (tipo == T_ZG) { /* zero/ganho UR: numero de ate 6 digitos */
    if (v < 0) v = 0;
    if (v > 999999) v = 999999;
    display_atualiza((uint32_t)v);
    display_set_decimal_points(2, 0);
    return;
  }
  if (tipo == T_ESCALA) {
    display_set_digits(DSP_E, DSP_S, DSP_C, DSP_OFF, DSP_OFF, v ? DSP_C : DSP_F);
    display_set_decimal_points(2, 0);
    return;
  }
  if (tipo == T_ENUM || tipo == T_ACTION) {
    uint8_t txt[3];
    enum_text(menu_item, v, txt);
    display_set_digits(lbl[0], lbl[1], lbl[2], txt[0], txt[1], txt[2]);
    display_set_decimal_points(2, 0);
    return;
  }

  /* T_NUM: mnemonico (esquerda) + valor inteiro de 3 digitos (direita) */
  if (v < 0) v = 0;
  if (v > 999) v = 999;
  display_set_digits(lbl[0], lbl[1], lbl[2], (uint32_t)(v / 100) % 10U,
                     (uint32_t)(v / 10) % 10U, (uint32_t)v % 10U);
  display_set_decimal_points(2, 0);
}

/* ============== maquina de menu ============== */
static void salva_menu(void) {
  config_salva();
}

static void processa_menu(void) {
  static uint8_t relogio_ant = 0;
  static uint8_t sair_pend = 0;
  int32_t v = menu_get();
  uint8_t enter_solto = relogio_ant && !btn_relogio_status;
  relogio_ant = btn_relogio_status;

  if (menu_bloqueia_soltura) {
    enter_solto = 0;
    if (!btn_relogio_status) {
      menu_bloqueia_soltura = 0;
      relogio_ant = 0;
    }
  }

  if (!menu_editando) {
    if (btn_relogio_status && tac_relogio_50ms >= 60U) {
      sair_pend = 1;
      splash(4, DSP_MINUS, DSP_S, DSP_A, DSP_I, DSP_R, DSP_MINUS);
      return;
    }
    if (btn_max_status && btn_max_timeout == 0) {
      menu_item = (uint8_t)((menu_item + 1U) % IT_QTD);
      btn_max_timeout = 5;
    } else if (!btn_max_status) {
      btn_max_timeout = 1;
    }
    if (btn_min_status && btn_min_timeout == 0) {
      menu_item = (menu_item == 0U) ? (uint8_t)(IT_QTD - 1U)
                                    : (uint8_t)(menu_item - 1U);
      btn_min_timeout = 5;
    } else if (!btn_min_status) {
      btn_min_timeout = 1;
    }
    menu_lista();

    if (enter_solto) {
      if (sair_pend) {
        sair_pend = 0;
        ui_modo = UI_TERMOMETRO;
        splash_timeout = 0;
        return;
      }
      menu_editando = 1;
      menu_rtc_campo = 0;
      menu_rtc_visu_to = 0;
      btn_max_turbo = 0;
      btn_min_turbo = 0;
      if (menu_tab[menu_item].tipo == T_RTCDATA)
        menu_rtc_data = rtc_app_data_ddmmaa();
      if (menu_tab[menu_item].tipo == T_RTCHORA)
        menu_rtc_hora = rtc_app_hora_hhmmss();
      splash_timeout = 0;
    }
    return;
  }

  /* editando */
  if (btn_max_status && btn_max_timeout == 0) {
    if (item_rtc()) {
      menu_rtc_ajusta(1);
      menu_rtc_visu_to = 20U;
      btn_max_timeout = 7;
    } else if (item_step_enum()) {
      int32_t n = item_nenum();
      menu_set((v + 1) % n); /* avanca o estado com retorno */
      btn_max_timeout = 7;
    } else {
      v += (btn_max_turbo >= 30) ? 100 : (btn_max_turbo >= 20) ? 20
            : (btn_max_turbo >= 10) ? 10 : 1;
      menu_set(v);
      if (btn_max_turbo < 30) btn_max_turbo++;
      btn_max_timeout = (btn_max_turbo >= 5) ? 4 : 7;
    }
  } else if (!btn_max_status) {
    btn_max_timeout = 2;
    btn_max_turbo = 0;
  }

  if (btn_min_status && btn_min_timeout == 0) {
    if (item_rtc()) {
      menu_rtc_ajusta(-1);
      menu_rtc_visu_to = 20U;
      btn_min_timeout = 7;
    } else if (item_step_enum()) {
      int32_t n = item_nenum();
      menu_set((v + n - 1) % n); /* retrocede o estado com retorno */
      btn_min_timeout = 7;
    } else {
      v -= (btn_min_turbo >= 30) ? 100 : (btn_min_turbo >= 20) ? 20
            : (btn_min_turbo >= 10) ? 10 : 1;
      menu_set(v);
      if (btn_min_turbo < 30) btn_min_turbo++;
      btn_min_timeout = (btn_min_turbo >= 5) ? 4 : 7;
    }
  } else if (!btn_min_status) {
    btn_min_timeout = 2;
    btn_min_turbo = 0;
  }

  menu_edicao();

  if (enter_solto) {
    if (item_rtc() && menu_rtc_campo < (rtc_qtd_campos() - 1U)) {
      menu_rtc_campo++;
      menu_rtc_visu_to = 0;
      return;
    }
    if (menu_item == IT_RST && reset_log_confirm) {
      datalog_reset();
      reset_log_confirm = 0;
    }
    if (menu_item == IT_DEF && def_confirm) {
      config_defaults(); /* restaura tudo (GUR/ZUR, setpoints, etc.) */
      def_confirm = 0;
    }
    if (item_rtc()) {
      if (!rtc_app_set_compacto(menu_rtc_data, menu_rtc_hora)) {
        splash(8, DSP_E, DSP_R, DSP_R, DSP_OFF, DSP_OFF, DSP_OFF);
        return;
      }
    } else {
      salva_menu();
      /* EEPROM ou restauracao de fabrica: reavalia o datalogger */
      if (menu_item == IT_EEPROM || menu_item == IT_DEF) datalog_init();
    }
    menu_editando = 0;
    menu_rtc_campo = 0;
    sair_pend = 0;
    splash(8, DSP_MINUS, DSP_S, DSP_A, DSP_V, DSP_E, DSP_MINUS);
  }
}

/* ============== entrada na tela principal ============== */
static void processa_principal(void) {
  static uint8_t enter_pend = 0;

  /* combo HORA+MINUTO => zera max/min */
  if (btn_max_status && btn_min_status) {
    zera_maxmin();
    btn_max_timeout = 20;
    btn_min_timeout = 20;
    return;
  }

  /* segurar T/R 3s arma entrada no menu */
  if (btn_relogio_status && tac_relogio_50ms >= 60U) {
    enter_pend = 1;
    splash(4, DSP_S, DSP_E, DSP_T, DSP_U, DSP_P, DSP_OFF);
    return;
  }
  if (!btn_relogio_status && enter_pend) {
    enter_pend = 0;
    menu_item = 0;
    menu_editando = 0;
    menu_bloqueia_soltura = 1;
    ui_modo = UI_MENU;
    splash_timeout = 0;
    menu_lista();
    return;
  }

  /* T/R curto (solto sem segurar 3s) alterna Termometro/Relogio */
  /* detectado por borda de soltura quando tac < 60 */
  {
    static uint8_t tr_ant = 0;
    if (tr_ant && !btn_relogio_status && tac_relogio_50ms < 60U && !enter_pend) {
      ui_modo = (ui_modo == UI_TERMOMETRO) ? UI_RELOGIO : UI_TERMOMETRO;
      alterna_to = 100;
    }
    tr_ant = btn_relogio_status;
  }

  /* HORA(+) momentaneo: mostra max temp; MINUTO(-) momentaneo: mostra data */
  if (btn_max_status && !btn_min_status) {
    momentaneo_tipo = 2; /* max temp */
    momentaneo_to = 20;
  } else if (btn_min_status && !btn_max_status) {
    momentaneo_tipo = 1; /* data */
    momentaneo_to = 20;
  }

  /* 5x HORA(+) => modo teste */
  if (!btn_max_status) {
    teste_solto = 1;
  } else if (teste_solto) {
    teste_solto = 0;
    if (teste_clicks < 5U) teste_clicks++;
    if (teste_clicks >= 5U) {
      teste_clicks = 0;
      teste_to = 600; /* ~30 s */
      teste_pagina = 0;
      ui_modo = UI_TESTE;
      splash(10, DSP_T, DSP_E, DSP_S, DSP_T, DSP_OFF, DSP_OFF);
    }
  }
}

void ui_tick(void) {
  /* Seguranca: no menu de configuracao, todas as saidas ficam desligadas. */
  control_inibe(ui_modo == UI_MENU);

  atualiza_maxmin();

  switch (ui_modo) {
  case UI_TERMOMETRO:
    processa_principal();
    /* sem alternancia automatica com o relogio (so troca manual por T/R) */
    if (ui_modo == UI_TERMOMETRO && splash_timeout == 0U) render_termometro();
    break;
  case UI_RELOGIO:
    processa_principal();
    if (ui_modo == UI_RELOGIO && splash_timeout == 0U) render_relogio();
    break;
  case UI_MENU:
    processa_menu();
    break;
  case UI_TESTE: {
    static uint8_t min_ant = 0;
    /* tecla Minuto avança a página (12V/5V -> u -> t -> ref) */
    if (btn_min_status && !min_ant) {
      teste_pagina = (uint8_t)((teste_pagina + 1U) % 4U);
      teste_to = 600U; /* reativa o timeout ao interagir */
    }
    min_ant = btn_min_status;
    render_teste();
    if (btn_relogio_status || teste_to == 0U) {
      ui_modo = UI_TERMOMETRO;
      splash_timeout = 0;
    }
    break;
  }
  default:
    ui_modo = UI_TERMOMETRO;
    break;
  }
}

void ui_tick_50ms(void) {
  if (btn_relogio_status && tac_relogio_50ms < 65535U) tac_relogio_50ms++;
  else if (!btn_relogio_status) tac_relogio_50ms = 0;

  if (momentaneo_to > 0U) momentaneo_to--;
  else momentaneo_tipo = 0;
  if (menu_rtc_visu_to > 0U) menu_rtc_visu_to--;
  if (teste_to > 0U) teste_to--;
  if (alterna_to > 0U) alterna_to--;
  if (disp_refresh_to > 0U) disp_refresh_to--;
}
