/*
 * config.c
 *
 * Persistencia do mapa de configuracao do Ummi Control em Flash interna
 * (setor 4 do STM32F410RB: 0x08010000-0x0801FFFF, 64 KB), gravacao
 * log-structured para distribuir o desgaste (igual estrategia de eeprom.c).
 */
#include "config.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Watchdog: realimentado antes do erase longo da Flash. A implementacao fica
 * isolada em main.c (dono do periferico); aqui so chamamos a funcao. */
extern void iwdg_refresh(void);

#define FLASH_CFG_START_ADDR 0x08010000U
#define FLASH_CFG_END_ADDR 0x08020000U
#define FLASH_CFG_SECTOR FLASH_SECTOR_4

typedef struct {
  uint32_t assinatura;
  uint32_t sequencia;
  ummi_config_t cfg;
  uint32_t crc;
} config_record_t;

ummi_config_t g_config;

static uint32_t cfg_next_sequence = 1U;

static uint32_t fnv1a(const void *data, uint32_t size) {
  const uint8_t *b = (const uint8_t *)data;
  uint32_t h = 2166136261UL;
  for (uint32_t i = 0; i < size; i++) {
    h ^= b[i];
    h *= 16777619UL;
  }
  return h;
}

static uint32_t record_crc(const config_record_t *r) {
  /* CRC sobre sequencia+cfg (exclui assinatura e o proprio crc) */
  return fnv1a(&r->sequencia, sizeof(r->sequencia) + sizeof(r->cfg));
}

static bool record_valido(const config_record_t *r) {
  return (r->assinatura == CONFIG_ASSINATURA) && (r->crc == record_crc(r));
}

void config_defaults(void) {
  memset(&g_config, 0, sizeof(g_config));

  g_config.escala_celsius = 1U;
  g_config.exibicao_alternada = 0U;
  g_config.eeprom_tipo = EEPROM_TIPO_DEFAULT;

  /* HIH5031 @5V na tensao DEPOIS do divisor 1k/1k8 (mV no ADC):
   * dUR = (mV*gur - zur)/100, com mV de 487 (0%) a 2531 (100%).
   * gur=49 (~0,489), zur=23863 (0% em 487 mV). Ajuste fino: menu P28/P29. */
  g_config.zur = 23863U;
  g_config.gur = 49U;

  g_config.log_habilitado = 1U;
  g_config.log_base = LOG_BASE_SEGUNDOS;
  g_config.log_intervalo = 60U;

  g_config.alm_temp = ALM_DESABILITADO;
  g_config.alm_ur = ALM_DESABILITADO;
  g_config.alm_tipo = BUZZER_MEDIO;
  g_config.alm_temp_alta_dC = 70;   /* 70 C (grau inteiro) */
  g_config.alm_temp_baixa_dC = 25;  /* 25 C (grau inteiro) */
  g_config.alm_ur_alta_dUR = 90;    /* 90 % (inteiro) */
  g_config.alm_ur_baixa_dUR = 10;   /* 10 % (inteiro) */

  for (uint8_t i = 0; i < EVENTOS_POR_FONTE; i++) {
    g_config.alm_eventos[i].liga_min = 0xFFFFU;
    g_config.alm_eventos[i].desliga_min = 0xFFFFU;
    g_config.rele1.eventos[i].liga_min = 0xFFFFU;
    g_config.rele1.eventos[i].desliga_min = 0xFFFFU;
    g_config.rele2.eventos[i].liga_min = 0xFFFFU;
    g_config.rele2.eventos[i].desliga_min = 0xFFFFU;
  }

  /* Rele 1: aquecedor por temperatura, SP 45 C, H 1 C (grau inteiro) */
  g_config.rele1.temperatura.habilita = 1U;
  g_config.rele1.temperatura.logica = RELE_LOGICA_AQUECER;
  g_config.rele1.temperatura.setpoint = 45;
  g_config.rele1.temperatura.hn = 1;
  g_config.rele1.temperatura.hp = 1;
  g_config.rele1.umidade.habilita = 0U;

  /* Rele 2: desabilitado por padrao */
  g_config.rele2.temperatura.habilita = 0U;
  g_config.rele2.umidade.habilita = 0U;

  g_config.v5_min_mV = 4500U;
  g_config.v5_max_mV = 5500U;
  g_config.v12_min_mV = 10000U;
  g_config.v12_max_mV = 15000U;
}

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void valida_grandeza(controle_grandeza_t *g, int32_t sp_max) {
  if (g->habilita > 1U) g->habilita = 0U;
  if (g->logica > 1U) g->logica = RELE_LOGICA_AQUECER;
  g->setpoint = clamp_i32(g->setpoint, 0, sp_max);
  g->hn = clamp_i32(g->hn, 0, sp_max);
  g->hp = clamp_i32(g->hp, 0, sp_max);
}

static void valida_eventos(evento_horario_t *ev) {
  for (uint8_t i = 0; i < EVENTOS_POR_FONTE; i++) {
    if (ev[i].liga_min != 0xFFFFU && ev[i].liga_min > 1439U) {
      ev[i].liga_min = 0xFFFFU;
      ev[i].desliga_min = 0xFFFFU;
    }
    if (ev[i].desliga_min != 0xFFFFU && ev[i].desliga_min > 1439U) {
      ev[i].desliga_min = ev[i].desliga_min % 1440U;
    }
  }
}

void config_valida(ummi_config_t *c) {
  if (c->escala_celsius > 1U) c->escala_celsius = 1U;
  if (c->exibicao_alternada > 1U) c->exibicao_alternada = 0U;
  if (c->eeprom_tipo > EEPROM_24AA512) c->eeprom_tipo = EEPROM_TIPO_DEFAULT;

  if (c->zur > 65535U) c->zur = 23863U;
  if (c->gur < 1U || c->gur > 65535U) c->gur = 49U;

  if (c->log_habilitado > 1U) c->log_habilitado = 1U;
  if (c->log_base > LOG_BASE_HORAS) c->log_base = LOG_BASE_SEGUNDOS;
  if (c->log_intervalo < 1U) c->log_intervalo = 1U;

  if (c->alm_temp > ALM_AMBOS) c->alm_temp = ALM_DESABILITADO;
  if (c->alm_ur > ALM_AMBOS) c->alm_ur = ALM_DESABILITADO;
  if (c->alm_tipo > BUZZER_RAPIDO) c->alm_tipo = BUZZER_MEDIO;
  c->alm_temp_alta_dC = clamp_i32(c->alm_temp_alta_dC, TEMP_MIN_GRAU, TEMP_MAX_GRAU);
  c->alm_temp_baixa_dC = clamp_i32(c->alm_temp_baixa_dC, TEMP_MIN_GRAU, TEMP_MAX_GRAU);
  c->alm_ur_alta_dUR = clamp_i32(c->alm_ur_alta_dUR, 0, 100);
  c->alm_ur_baixa_dUR = clamp_i32(c->alm_ur_baixa_dUR, 0, 100);
  /* Coerencia: o limite baixo nunca pode exceder o alto (o caminho serial
   * grava os dois campos direto, sem ordenar). */
  if (c->alm_temp_baixa_dC > c->alm_temp_alta_dC) {
    c->alm_temp_baixa_dC = c->alm_temp_alta_dC;
  }
  if (c->alm_ur_baixa_dUR > c->alm_ur_alta_dUR) {
    c->alm_ur_baixa_dUR = c->alm_ur_alta_dUR;
  }

  valida_eventos(c->alm_eventos);

  valida_grandeza(&c->rele1.temperatura, TEMP_MAX_GRAU);
  valida_grandeza(&c->rele1.umidade, 100);
  valida_eventos(c->rele1.eventos);
  valida_grandeza(&c->rele2.temperatura, TEMP_MAX_GRAU);
  valida_grandeza(&c->rele2.umidade, 100);
  valida_eventos(c->rele2.eventos);

  if (c->v5_min_mV == 0U || c->v5_min_mV >= c->v5_max_mV) {
    c->v5_min_mV = 4500U;
    c->v5_max_mV = 5500U;
  }
  if (c->v12_min_mV == 0U || c->v12_min_mV >= c->v12_max_mV) {
    c->v12_min_mV = 10000U;
    c->v12_max_mV = 15000U;
  }
}

static bool flash_le_mais_recente(ummi_config_t *out) {
  config_record_t melhor = {0};
  bool achou = false;

  for (uint32_t addr = FLASH_CFG_START_ADDR;
       (addr + sizeof(config_record_t)) <= FLASH_CFG_END_ADDR;
       addr += sizeof(config_record_t)) {
    const config_record_t *r = (const config_record_t *)addr;
    if (r->assinatura == 0xFFFFFFFFU) {
      break; /* fim dos registros gravados */
    }
    if (!record_valido(r)) {
      continue;
    }
    if (!achou || ((int32_t)(r->sequencia - melhor.sequencia) > 0)) {
      melhor = *r;
      achou = true;
    }
  }

  if (!achou) {
    cfg_next_sequence = 1U;
    return false;
  }

  *out = melhor.cfg;
  cfg_next_sequence = melhor.sequencia + 1U;
  return true;
}

void config_init(void) {
  ummi_config_t lida;

  config_defaults();

  if (flash_le_mais_recente(&lida)) {
    g_config = lida;
    config_valida(&g_config);
  } else {
    /* Nenhum registro valido (chip novo ou apos mudanca de layout/assinatura):
     * persiste os defaults para garantir um registro UMC valido e tornar
     * irrelevantes eventuais registros antigos no setor. Executa antes do
     * IWDG ser ligado, entao o erase (se necessario) nao corre risco de reset. */
    config_valida(&g_config);
    (void)config_salva();
  }
}

bool config_salva(void) {
  uint32_t addr = FLASH_CFG_START_ADDR;
  config_record_t record;

  config_valida(&g_config);

  memset(&record, 0, sizeof(record));
  record.assinatura = CONFIG_ASSINATURA;
  record.sequencia = cfg_next_sequence;
  record.cfg = g_config;
  record.crc = record_crc(&record);

  /* encontra primeiro slot livre */
  while ((addr + sizeof(config_record_t)) <= FLASH_CFG_END_ADDR) {
    const config_record_t *slot = (const config_record_t *)addr;
    if (slot->assinatura == 0xFFFFFFFFU) {
      break;
    }
    addr += sizeof(config_record_t);
  }

  HAL_FLASH_Unlock();

  if ((addr + sizeof(config_record_t)) > FLASH_CFG_END_ADDR) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t erro = 0;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_CFG_SECTOR;
    erase.NbSectors = 1U;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    /* O erase do setor de 64 KB bloqueia o nucleo por ~1-2 s; realimenta o
     * IWDG (~2,5 s) imediatamente antes para nao resetar no meio da gravacao. */
    iwdg_refresh();
    if (HAL_FLASHEx_Erase(&erase, &erro) != HAL_OK) {
      HAL_FLASH_Lock();
      return false;
    }
    addr = FLASH_CFG_START_ADDR;
  }

  const uint32_t *words = (const uint32_t *)&record;
  for (uint32_t i = 0; i < ((sizeof(record) + 3U) / 4U); i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (i * 4U), words[i]) !=
        HAL_OK) {
      HAL_FLASH_Lock();
      return false;
    }
  }

  HAL_FLASH_Lock();
  cfg_next_sequence++;
  return true;
}
