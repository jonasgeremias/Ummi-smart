/*
 * datalog.c — Datalogger em EEPROM externa I2C @0xA0.
 *
 * Suporta dois chips, selecionados por g_config.eeprom_tipo (default de fabrica
 * em config.h via EEPROM_TIPO_DEFAULT):
 *   EEPROM_24LC256 -> 32 KB, pagina de 64 bytes  (hardware legado do PIC)
 *   EEPROM_24AA512 -> 64 KB, pagina de 128 bytes (hardware atual soldado)
 * Ambos usam endereco de memoria de 16 bits e device address 0x50.
 */
#include "datalog.h"
#include "config.h"
#include "rtc_app.h"
#include "sensors.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stddef.h>

extern I2C_HandleTypeDef hi2c1;

#define EXT_EEPROM_ADDR (0x50U << 1)
#define EXT_EEPROM_TO_MS 25U

/* Parametros por tipo de chip (selecionado em runtime). */
#define EE_24LC256_SIZE 32768U
#define EE_24LC256_PAGE 64U
#define EE_24AA512_SIZE 65536U
#define EE_24AA512_PAGE 128U

/* Valores ativos (default 24AA512 = hardware atual; ajustados em datalog_init). */
static uint32_t ext_size = EE_24AA512_SIZE;
static uint16_t ext_page = EE_24AA512_PAGE;

#define HDR_ADDR 0x0200U
#define HDR_BKP_ADDR 0x0300U
#define REC_BASE_ADDR 0x0400U

#define DATALOG_ASSINATURA 0x554D4C47U /* "UMLG" */
#define DATALOG_VERSAO 1U

typedef struct {
  uint32_t assinatura;
  uint16_t versao;
  uint16_t record_size;
  uint16_t capacidade;
  uint16_t proximo_indice;
  uint16_t quantidade_valida;
  uint16_t reservado;
  uint32_t seq_global;
  uint16_t crc;
} datalog_header_t;

static datalog_header_t hdr;
static bool g_ok;
static uint32_t ultima_chave_log = 0xFFFFFFFFU; /* anti-duplicidade */

/* ---------------- EEPROM primitivas ---------------- */
/* Recupera o barramento I2C travado (NACK/BUSY) re-inicializando o periferico. */
static void i2c_recupera(void) {
  HAL_I2C_DeInit(&hi2c1);
  HAL_I2C_Init(&hi2c1);
}

static bool ext_ready(uint32_t to_ms) {
  uint32_t t0 = HAL_GetTick();
  do {
    if (HAL_I2C_IsDeviceReady(&hi2c1, EXT_EEPROM_ADDR, 1, 2) == HAL_OK) {
      return true;
    }
  } while ((HAL_GetTick() - t0) < to_ms);
  i2c_recupera();
  return false;
}

static bool ext_read(uint16_t addr, void *data, uint16_t size) {
  if (!ext_ready(EXT_EEPROM_TO_MS)) return false;
  return HAL_I2C_Mem_Read(&hi2c1, EXT_EEPROM_ADDR, addr, I2C_MEMADD_SIZE_16BIT,
                          (uint8_t *)data, size, EXT_EEPROM_TO_MS) == HAL_OK;
}

static bool ext_write(uint16_t addr, const void *data, uint16_t size) {
  const uint8_t *b = (const uint8_t *)data;
  while (size > 0U) {
    uint16_t off = addr % ext_page;
    uint16_t space = ext_page - off;
    uint16_t chunk = (size < space) ? size : space;
    if (!ext_ready(EXT_EEPROM_TO_MS)) return false;
    if (HAL_I2C_Mem_Write(&hi2c1, EXT_EEPROM_ADDR, addr, I2C_MEMADD_SIZE_16BIT,
                          (uint8_t *)b, chunk, EXT_EEPROM_TO_MS) != HAL_OK) {
      return false;
    }
    if (!ext_ready(EXT_EEPROM_TO_MS)) return false;
    addr += chunk;
    b += chunk;
    size -= chunk;
  }
  return true;
}

/* ---------------- CRC ---------------- */
static uint8_t crc_xor8(const uint8_t *d, uint16_t n) {
  uint8_t c = 0;
  for (uint16_t i = 0; i < n; i++) c ^= d[i];
  return c;
}

static uint16_t header_crc(const datalog_header_t *h) {
  /* FNV-1a 16 bits sobre todos os campos antes do crc (exclui crc/padding). */
  const uint8_t *b = (const uint8_t *)h;
  uint32_t hsh = 2166136261UL;
  uint32_t n = offsetof(datalog_header_t, crc);
  for (uint32_t i = 0; i < n; i++) {
    hsh ^= b[i];
    hsh *= 16777619UL;
  }
  return (uint16_t)((hsh >> 16) ^ (hsh & 0xFFFFU));
}

static uint16_t capacidade_calc(void) {
  return (uint16_t)((ext_size - REC_BASE_ADDR) / DATALOG_RECORD_SIZE);
}

/* Seleciona tamanho/pagina conforme o chip configurado em g_config. */
static void aplica_tipo_eeprom(void) {
  if (g_config.eeprom_tipo == EEPROM_24LC256) {
    ext_size = EE_24LC256_SIZE;
    ext_page = EE_24LC256_PAGE;
  } else { /* EEPROM_24AA512 (default) */
    ext_size = EE_24AA512_SIZE;
    ext_page = EE_24AA512_PAGE;
  }
}

static uint16_t rec_addr(uint16_t idx) {
  return (uint16_t)(REC_BASE_ADDR + (uint32_t)idx * DATALOG_RECORD_SIZE);
}

static bool grava_header(void) {
  hdr.crc = header_crc(&hdr);
  bool a = ext_write(HDR_ADDR, &hdr, sizeof(hdr));
  bool b = ext_write(HDR_BKP_ADDR, &hdr, sizeof(hdr));
  return a && b;
}

static void formata_header(void) {
  memset(&hdr, 0, sizeof(hdr));
  hdr.assinatura = DATALOG_ASSINATURA;
  hdr.versao = DATALOG_VERSAO;
  hdr.record_size = DATALOG_RECORD_SIZE;
  hdr.capacidade = capacidade_calc();
  hdr.proximo_indice = 0;
  hdr.quantidade_valida = 0;
  hdr.seq_global = 1;
}

static bool header_valido(const datalog_header_t *h) {
  return h->assinatura == DATALOG_ASSINATURA && h->versao == DATALOG_VERSAO &&
         h->record_size == DATALOG_RECORD_SIZE &&
         h->capacidade == capacidade_calc() && h->crc == header_crc(h);
}

void datalog_init(void) {
  datalog_header_t tmp;
  g_ok = false;

  aplica_tipo_eeprom();

  if (ext_read(HDR_ADDR, &tmp, sizeof(tmp)) && header_valido(&tmp)) {
    hdr = tmp;
    g_ok = true;
  } else if (ext_read(HDR_BKP_ADDR, &tmp, sizeof(tmp)) && header_valido(&tmp)) {
    /* recupera do backup e regrava o primario */
    hdr = tmp;
    g_ok = grava_header();
  } else {
    /* ambos invalidos: reset total */
    datalog_reset();
    return;
  }

  if (hdr.proximo_indice >= hdr.capacidade) hdr.proximo_indice = 0;
  if (hdr.quantidade_valida > hdr.capacidade) {
    hdr.quantidade_valida = hdr.capacidade;
  }
}

bool datalog_reset(void) {
  formata_header();
  ultima_chave_log = 0xFFFFFFFFU;
  g_ok = grava_header();
  return g_ok;
}

static void monta_registro(uint8_t r[DATALOG_RECORD_SIZE]) {
  rtc_dt_t dt;
  int32_t t_dC;
  uint8_t neg = 0;
  uint16_t ur;

  if (!rtc_app_get(&dt)) {
    memset(&dt, 0, sizeof(dt));
  }

  if (g_config.escala_celsius) {
    t_dC = sensors_temp_C(); /* grau inteiro */
  } else {
    t_dC = sensors_temp_F(); /* grau inteiro */
  }
  if (t_dC < 0) {
    neg = 1;
    t_dC = -t_dC;
  }
  if (t_dC > 0x7FFF) t_dC = 0x7FFF;

  ur = sensors_umidade_pct(); /* % inteiro (0..100) */
  if (ur > 100U) ur = 100U;

  r[0] = dt.dia;
  r[1] = dt.mes;
  r[2] = dt.ano;
  r[3] = (uint8_t)(dt.hora | (g_config.escala_celsius ? 0x80U : 0x00U));
  r[4] = (uint8_t)(dt.min | (neg ? 0x80U : 0x00U));
  r[5] = (uint8_t)((t_dC >> 8) & 0xFF);
  r[6] = (uint8_t)(t_dC & 0xFF);
  r[7] = (uint8_t)ur;
  r[8] = crc_xor8(r, 8);
}

static bool grava_amostra(void) {
  uint8_t r[DATALOG_RECORD_SIZE];

  if (!g_ok || !sensors_estavel()) return false;
  /* nao grava com sensor desconectado */
  if (sensors_temp_desconectada() || sensors_umidade_desconectada()) {
    return false;
  }

  monta_registro(r);

  if (!ext_write(rec_addr(hdr.proximo_indice), r, DATALOG_RECORD_SIZE)) {
    g_ok = false;
    return false;
  }

  hdr.seq_global++;
  hdr.proximo_indice++;
  if (hdr.proximo_indice >= hdr.capacidade) hdr.proximo_indice = 0;
  if (hdr.quantidade_valida < hdr.capacidade) hdr.quantidade_valida++;

  if (!grava_header()) {
    g_ok = false;
    return false;
  }
  return true;
}

void datalog_tick_1s(void) {
  rtc_dt_t dt;
  uint16_t intervalo = g_config.log_intervalo;
  uint32_t chave;
  bool dispara = false;

  if (!g_config.log_habilitado || intervalo == 0U) return;
  if (!rtc_app_get(&dt)) return;

  switch (g_config.log_base) {
  case LOG_BASE_SEGUNDOS:
    if ((dt.seg % intervalo) == 0U) dispara = true;
    chave = ((uint32_t)dt.hora << 12) | ((uint32_t)dt.min << 6) | dt.seg;
    break;
  case LOG_BASE_MINUTOS:
    if (dt.seg == 0U && (dt.min % intervalo) == 0U) dispara = true;
    chave = ((uint32_t)dt.dia << 16) | ((uint32_t)dt.hora << 8) | dt.min;
    break;
  case LOG_BASE_HORAS:
  default:
    if (dt.seg == 0U && dt.min == 0U && (dt.hora % intervalo) == 0U) {
      dispara = true;
    }
    chave = ((uint32_t)dt.mes << 16) | ((uint32_t)dt.dia << 8) | dt.hora;
    break;
  }

  if (dispara && chave != ultima_chave_log) {
    if (grava_amostra()) {
      ultima_chave_log = chave;
    }
  }
}

uint16_t datalog_total(void) { return g_ok ? hdr.quantidade_valida : 0U; }
uint16_t datalog_capacidade(void) { return hdr.capacidade; }
uint16_t datalog_proximo_indice(void) { return hdr.proximo_indice; }
bool datalog_ok(void) { return g_ok; }

bool datalog_le_bloco(uint16_t indice_logico,
                      uint8_t bloco[DATALOG_RECORD_SIZE]) {
  if (!g_ok || indice_logico >= hdr.quantidade_valida) return false;

  /* indice 0 = mais antigo */
  uint16_t fisico = (uint16_t)((hdr.proximo_indice + hdr.capacidade -
                                hdr.quantidade_valida + indice_logico) %
                               hdr.capacidade);
  if (!ext_read(rec_addr(fisico), bloco, DATALOG_RECORD_SIZE)) return false;
  if (bloco[8] != crc_xor8(bloco, 8)) return false;
  return true;
}
