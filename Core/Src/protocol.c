/*
 * protocol.c — Implementacao do protocolo Ummi Manager sobre USART1.
 *
 * NOTA (LAC-02): o layout byte-a-byte das respostas de status/config do PIC
 * original nao esta totalmente especificado em REQUISITOS. As estruturas aqui
 * seguem o documentado e devem ser validadas contra o software "Ummi Manager".
 */
#include "protocol.h"
#include "config.h"
#include "sensors.h"
#include "rtc_app.h"
#include "datalog.h"
#include "control.h"
#include "stm32f4xx_hal.h"

#define FRAME_INICIO '*'
#define ACK_OK 'F'
#define DL_NEXT '#'

#define RX_FIFO_SIZE 64U
#define FRAME_MAX 10U
#define FRAME_TIMEOUT_MS 504U /* CONT_REST do original */

/* ---- FIFO de recepcao (preenchido pela ISR) ---- */
static volatile uint8_t rx_fifo[RX_FIFO_SIZE];
static volatile uint16_t rx_head, rx_tail;
static volatile uint8_t rx_err;

/* ---- montagem do frame ---- */
static uint8_t frame[FRAME_MAX];
static uint8_t frame_len;
static uint8_t frame_nbcr;
static volatile uint16_t frame_timeout_ms;

/* ---- download ---- */
static uint8_t dl_ativo;
static uint16_t dl_indice;
static uint16_t dl_total;

/* ---- salvamento adiado da config (evita erase/program da Flash a cada
 * comando da rajada do Ummi Manager; o ACK e imediato, a gravacao ocorre
 * ~CFG_SAVE_DEBOUNCE_MS apos o ultimo comando de escrita). ---- */
#define CFG_SAVE_DEBOUNCE_MS 800U
static uint8_t cfg_dirty;
static volatile uint16_t cfg_save_to;

/* ====================================================================== */
/* Baixo nivel USART1                                                     */
/* ====================================================================== */
void protocol_init(void) {
  uint32_t pclk = HAL_RCC_GetPCLK2Freq();

  rx_head = rx_tail = 0;
  rx_err = 0;
  frame_len = 0;
  frame_nbcr = 0;
  dl_ativo = 0;

  /* Reconfigura USART1 como assincrono 8N1 (sobrepoe init sync do CubeMX). */
  USART1->CR1 = 0;
  USART1->CR2 = 0; /* 1 stop, sem clock (CLKEN=0) */
  USART1->CR3 = 0;
  USART1->BRR = (pclk + (UMMI_BAUD / 2U)) / UMMI_BAUD; /* oversampling 16 */
  USART1->CR3 |= USART_CR3_EIE;
  USART1->CR1 |= USART_CR1_RXNEIE | USART_CR1_PEIE | USART_CR1_TE |
                 USART_CR1_RE | USART_CR1_UE;

  HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void tx_byte(uint8_t b) {
  uint32_t t0 = HAL_GetTick();
  while ((USART1->SR & USART_SR_TXE) == 0U) {
    if ((HAL_GetTick() - t0) > 20U) return;
  }
  USART1->DR = b;
}

/* Envia buffer + CRC XOR (inclui todos os bytes enviados). */
static void tx_com_crc(const uint8_t *d, uint16_t n) {
  uint8_t crc = 0;
  for (uint16_t i = 0; i < n; i++) {
    crc ^= d[i];
    tx_byte(d[i]);
  }
  tx_byte(crc);
}

static void tx_ack(void) { tx_byte(ACK_OK); }

/* ====================================================================== */
/* NBCR — tamanho total do frame pela faixa do comando                    */
/* ====================================================================== */
static uint8_t nbcr_do_comando(uint8_t cmd) {
  if (cmd >= 'y') return 5U;
  if (cmd >= 'a') return 4U;
  if (cmd >= 'A') return 3U;
  if (cmd >= ':') return 9U;
  return 10U;
}

/* ====================================================================== */
/* Respostas especificas                                                  */
/* ====================================================================== */
static void resp_versao(void) {
  uint8_t b[3] = {'V', (uint8_t)FW_VERSAO_MAJOR, (uint8_t)FW_VERSAO_MINOR};
  tx_com_crc(b, 3);
}

static void resp_temp_instantanea(void) {
  int32_t t = g_config.escala_celsius ? sensors_temp_C() : sensors_temp_F();
  uint8_t neg = 0;
  if (t < 0) { neg = 1; t = -t; }
  uint8_t b[4];
  b[0] = 'X';
  b[1] = (uint8_t)(neg | (g_config.escala_celsius ? 0x80U : 0x00U));
  b[2] = (uint8_t)((t >> 8) & 0xFF);
  b[3] = (uint8_t)(t & 0xFF);
  tx_com_crc(b, 4);
}

static void resp_ur_instantanea(void) {
  uint16_t ur = sensors_umidade_pct();
  uint8_t b[3];
  b[0] = 'U';
  b[1] = (uint8_t)((ur >> 8) & 0xFF);
  b[2] = (uint8_t)(ur & 0xFF);
  tx_com_crc(b, 3);
}

static void resp_status_saidas(void) {
  uint16_t fl = control_status_flags();
  uint8_t b[5];
  b[0] = 'R';
  b[1] = control_buzzer_ativo() ? 1U : 0U;
  b[2] = control_rele1() ? 1U : 0U;
  b[3] = control_rele2() ? 1U : 0U;
  b[4] = (uint8_t)(fl & 0xFF);
  tx_com_crc(b, 5);
}

static void resp_status_completo(void) {
  rtc_dt_t dt;
  int32_t tc = sensors_temp_C(); /* grau inteiro */
  int32_t tf = sensors_temp_F(); /* grau inteiro */
  uint16_t ur = sensors_umidade_pct();
  uint16_t v5 = sensors_v5_mV();
  uint16_t v12 = sensors_v12_mV();
  uint16_t fl = control_status_flags();
  uint8_t neg = (tc < 0) ? 1U : 0U;
  int32_t tca = neg ? -tc : tc;
  uint8_t b[24];

  if (!rtc_app_get(&dt)) {
    dt.dia = dt.mes = dt.ano = dt.hora = dt.min = dt.seg = 0;
  }
  b[0] = 'T';
  b[1] = (uint8_t)((tca >> 8) & 0xFF);
  b[2] = (uint8_t)(tca & 0xFF);
  b[3] = neg;
  b[4] = (uint8_t)((tf >> 8) & 0xFF);
  b[5] = (uint8_t)(tf & 0xFF);
  b[6] = (uint8_t)((ur >> 8) & 0xFF);
  b[7] = (uint8_t)(ur & 0xFF);
  b[8] = (uint8_t)((v5 >> 8) & 0xFF);
  b[9] = (uint8_t)(v5 & 0xFF);
  b[10] = (uint8_t)((v12 >> 8) & 0xFF);
  b[11] = (uint8_t)(v12 & 0xFF);
  b[12] = dt.dia;
  b[13] = dt.mes;
  b[14] = dt.ano;
  b[15] = dt.hora;
  b[16] = dt.min;
  b[17] = dt.seg;
  b[18] = (uint8_t)((fl >> 8) & 0xFF);
  b[19] = (uint8_t)(fl & 0xFF);
  b[20] = g_config.escala_celsius;
  b[21] = (uint8_t)(datalog_total() >> 8);
  b[22] = (uint8_t)(datalog_total() & 0xFF);
  b[23] = g_config.log_habilitado;
  tx_com_crc(b, 24);
}

/* Resposta de config (comando W): envia um resumo dos principais campos. */
static void resp_configs(void) {
  uint8_t b[16];
  b[0] = 'W';
  b[1] = g_config.escala_celsius;
  b[2] = g_config.exibicao_alternada;
  b[3] = (uint8_t)(g_config.zur >> 8);
  b[4] = (uint8_t)(g_config.zur & 0xFF);
  b[5] = (uint8_t)(g_config.gur >> 8);
  b[6] = (uint8_t)(g_config.gur & 0xFF);
  b[7] = g_config.log_habilitado;
  b[8] = g_config.log_base;
  b[9] = (uint8_t)(g_config.log_intervalo >> 8);
  b[10] = (uint8_t)(g_config.log_intervalo & 0xFF);
  b[11] = g_config.alm_temp;
  b[12] = g_config.alm_ur;
  b[13] = g_config.alm_tipo;
  b[14] = g_config.rele1.temperatura.habilita;
  b[15] = g_config.rele2.temperatura.habilita;
  tx_com_crc(b, 16);
}

/* ====================================================================== */
/* Download do datalogger                                                 */
/* ====================================================================== */
static void download_inicia(void) {
  dl_total = datalog_total();
  dl_indice = 0;
  dl_ativo = (dl_total > 0U) ? 1U : 0U;
  if (!dl_ativo) {
    const uint8_t fim[3] = {'F', 'I', 'M'};
    tx_com_crc(fim, 3);
    return;
  }
  /* envia o primeiro bloco imediatamente */
}

static void download_envia_proximo(void) {
  uint8_t bloco[9];

  if (!dl_ativo) return;
  if (dl_indice >= dl_total) {
    const uint8_t fim[3] = {'F', 'I', 'M'};
    tx_com_crc(fim, 3);
    dl_ativo = 0;
    return;
  }
  if (!datalog_le_bloco(dl_indice, bloco)) {
    const uint8_t fim[3] = {'F', 'I', 'M'};
    tx_com_crc(fim, 3);
    dl_ativo = 0;
    return;
  }
  /* o bloco ja contem 8 dados + 1 CRC (XOR) gravado; reenvia como esta */
  for (uint8_t i = 0; i < 9U; i++) tx_byte(bloco[i]);
  dl_indice++;
}

/* ====================================================================== */
/* Dispatcher                                                             */
/* ====================================================================== */
static void aplica_comando(void) {
  uint8_t cmd = frame[1];
  const uint8_t *d = &frame[2]; /* dados apos o comando */
  uint8_t escreveu = 0;

  switch (cmd) {
  /* --- exibicao / escala --- */
  case 'J': g_config.escala_celsius = 1U; escreveu = 1; break; /* °C */
  case 'H': g_config.escala_celsius = 0U; escreveu = 1; break; /* °F */
  case 'F': g_config.exibicao_alternada = 1U; escreveu = 1; break;
  case 'G': g_config.exibicao_alternada = 0U; escreveu = 1; break;

  /* --- RTC (1 byte de dado) --- */
  /* ACK so apos gravacao bem-sucedida (valor invalido nao recebe 'F'). */
  case 'd': if (rtc_app_set_dia(d[0])) tx_ack(); return;
  case 'e': if (rtc_app_set_mes(d[0])) tx_ack(); return;
  case 'a': if (rtc_app_set_ano(d[0])) tx_ack(); return;
  case 'h': if (rtc_app_set_hora(d[0])) tx_ack(); return;
  case 'i': if (rtc_app_set_minuto(d[0])) tx_ack(); return;

  /* --- calibracao UR (2 bytes) --- */
  case 'z': g_config.zur = (uint32_t)(((uint16_t)d[0] << 8) | d[1]); escreveu = 1; break;
  case 'y': g_config.gur = (uint32_t)(((uint16_t)d[0] << 8) | d[1]); escreveu = 1; break;

  /* --- datalogger --- */
  case 'D': g_config.log_habilitado = 1U; escreveu = 1; break;
  case 'E': g_config.log_habilitado = 0U; escreveu = 1; break;
  case 'Z': datalog_reset(); tx_ack(); return;
  case 's': g_config.log_base = LOG_BASE_SEGUNDOS; g_config.log_intervalo = d[0]; escreveu = 1; break;
  case 'm': g_config.log_base = LOG_BASE_MINUTOS; g_config.log_intervalo = d[0]; escreveu = 1; break;
  case 'c': g_config.log_base = LOG_BASE_HORAS; g_config.log_intervalo = d[0]; escreveu = 1; break;

  /* --- alarme/buzzer e reles (config por bytes) --- */
  case 'b': g_config.alm_tipo = d[0]; escreveu = 1; break; /* tipo do toque */
  case '0': /* alarme buzzer por temperatura: tipo,alta(2),baixa(2) */
    g_config.alm_temp = d[0];
    g_config.alm_temp_alta_dC = (int32_t)(((uint16_t)d[1] << 8) | d[2]);
    g_config.alm_temp_baixa_dC = (int32_t)(((uint16_t)d[3] << 8) | d[4]);
    escreveu = 1;
    break;
  case '1': /* alarme buzzer por UR */
    g_config.alm_ur = d[0];
    g_config.alm_ur_alta_dUR = (int32_t)(((uint16_t)d[1] << 8) | d[2]);
    g_config.alm_ur_baixa_dUR = (int32_t)(((uint16_t)d[3] << 8) | d[4]);
    escreveu = 1;
    break;
  case ':': /* Rele1 por temperatura: hab,logica,SP(2),HN,HP -> 6 bytes */
    g_config.rele1.temperatura.habilita = d[0];
    g_config.rele1.temperatura.logica = d[1];
    g_config.rele1.temperatura.setpoint = (int32_t)(((uint16_t)d[2] << 8) | d[3]);
    g_config.rele1.temperatura.hn = d[4];
    g_config.rele1.temperatura.hp = d[5];
    escreveu = 1;
    break;
  case ';': /* Rele1 por UR */
    g_config.rele1.umidade.habilita = d[0];
    g_config.rele1.umidade.logica = d[1];
    g_config.rele1.umidade.setpoint = (int32_t)(((uint16_t)d[2] << 8) | d[3]);
    g_config.rele1.umidade.hn = d[4];
    g_config.rele1.umidade.hp = d[5];
    escreveu = 1;
    break;
  case '<': /* Rele2 por temperatura */
    g_config.rele2.temperatura.habilita = d[0];
    g_config.rele2.temperatura.logica = d[1];
    g_config.rele2.temperatura.setpoint = (int32_t)(((uint16_t)d[2] << 8) | d[3]);
    g_config.rele2.temperatura.hn = d[4];
    g_config.rele2.temperatura.hp = d[5];
    escreveu = 1;
    break;
  case '=': /* Rele2 por UR */
    g_config.rele2.umidade.habilita = d[0];
    g_config.rele2.umidade.logica = d[1];
    g_config.rele2.umidade.setpoint = (int32_t)(((uint16_t)d[2] << 8) | d[3]);
    g_config.rele2.umidade.hn = d[4];
    g_config.rele2.umidade.hp = d[5];
    escreveu = 1;
    break;
  case '2': /* evento alarme horario: idx,ligaH,ligaM,desligaH,desligaM (7 bytes) */
    if (d[0] < EVENTOS_POR_FONTE) {
      g_config.alm_eventos[d[0]].liga_min = (uint16_t)(d[1] * 60U + d[2]);
      g_config.alm_eventos[d[0]].desliga_min = (uint16_t)(d[3] * 60U + d[4]);
    }
    escreveu = 1;
    break;
  case '3': /* evento Rele1 horario */
    if (d[0] < EVENTOS_POR_FONTE) {
      g_config.rele1.eventos[d[0]].liga_min = (uint16_t)(d[1] * 60U + d[2]);
      g_config.rele1.eventos[d[0]].desliga_min = (uint16_t)(d[3] * 60U + d[4]);
    }
    escreveu = 1;
    break;
  case '4': /* evento Rele2 horario */
    if (d[0] < EVENTOS_POR_FONTE) {
      g_config.rele2.eventos[d[0]].liga_min = (uint16_t)(d[1] * 60U + d[2]);
      g_config.rele2.eventos[d[0]].desliga_min = (uint16_t)(d[3] * 60U + d[4]);
    }
    escreveu = 1;
    break;

  /* --- tipo da EEPROM externa do datalogger (0=24LC256, 1=24AA512) --- */
  case 'k':
    if (d[0] <= EEPROM_24AA512 && d[0] != g_config.eeprom_tipo) {
      g_config.eeprom_tipo = d[0];
      datalog_init(); /* re-avalia capacidade/pagina para o novo chip */
    }
    escreveu = 1;
    break;

  /* --- leituras / status --- */
  case 'T': resp_status_completo(); return;
  case 'V': resp_versao(); return;
  case 'X': resp_temp_instantanea(); return;
  case 'U': resp_ur_instantanea(); return;
  case 'R': resp_status_saidas(); return;
  case 'W': resp_configs(); return;
  case 'C': download_inicia(); if (dl_ativo) download_envia_proximo(); return;

  default:
    /* comando desconhecido: ignora (sem eco para nao confundir o PC) */
    return;
  }

  if (escreveu) {
    config_valida(&g_config);
    /* marca para gravar apos a rajada; ACK imediato (compat. Ummi Manager) */
    cfg_dirty = 1U;
    cfg_save_to = CFG_SAVE_DEBOUNCE_MS;
    tx_ack();
  }
}

/* ====================================================================== */
/* Maquina de frames                                                      */
/* ====================================================================== */
static void frame_reset(void) {
  frame_len = 0;
  frame_nbcr = 0;
}

static void processa_byte(uint8_t b) {
  /* handshake de download tem prioridade */
  if (dl_ativo && b == DL_NEXT) {
    download_envia_proximo();
    return;
  }

  if (frame_len == 0U) {
    if (b != FRAME_INICIO) {
      return; /* aguarda inicio de frame */
    }
    frame[0] = b;
    frame_len = 1U;
    frame_timeout_ms = FRAME_TIMEOUT_MS;
    return;
  }

  frame[frame_len++] = b;

  if (frame_len == 2U) {
    frame_nbcr = nbcr_do_comando(frame[1]);
    if (frame_nbcr > FRAME_MAX) {
      frame_reset();
      return;
    }
  }

  if (frame_len >= 2U && frame_len == frame_nbcr) {
    /* frame completo: valida CRC (XOR de todo o frame == 0) */
    uint8_t crc = 0;
    for (uint8_t i = 0; i < frame_len; i++) crc ^= frame[i];
    if (crc == 0U) {
      aplica_comando();
    }
    frame_reset();
  }
}

void protocol_process(void) {
  /* timeout de sincronismo */
  if (frame_len > 0U && frame_timeout_ms == 0U) {
    frame_reset();
  }

  if (rx_err) {
    rx_err = 0;
    frame_reset();
  }

  while (rx_tail != rx_head) {
    uint8_t b = rx_fifo[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % RX_FIFO_SIZE);
    processa_byte(b);
  }

  /* grava a config so quando a rajada de comandos cessa (debounce) */
  if (cfg_dirty && cfg_save_to == 0U) {
    cfg_dirty = 0U;
    config_salva();
  }
}

/* ====================================================================== */
/* Ganchos de interrupcao / tempo                                          */
/* ====================================================================== */
void serial_tick_1ms(void) {
  if (frame_timeout_ms > 0U) frame_timeout_ms--;
  if (cfg_save_to > 0U) cfg_save_to--;
}

void serial_usart1_irq_handler(void) {
  uint32_t sr = USART1->SR;

  if ((sr & (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)) != 0U) {
    (void)USART1->DR; /* limpa erros (sequencia SR->DR) */
    rx_err = 1;
    return;
  }

  if ((sr & USART_SR_RXNE) != 0U) {
    uint8_t b = (uint8_t)USART1->DR;
    uint16_t prox = (uint16_t)((rx_head + 1U) % RX_FIFO_SIZE);
    if (prox != rx_tail) {
      rx_fifo[rx_head] = b;
      rx_head = prox;
    }
    /* se cheio, descarta (overrun de software) */
  }
}
