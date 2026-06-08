/*
 * eeprom.c
 *
 * Persistencia de configuracao na flash interna do STM32, emulando EEPROM.
 */

#include "eeprom.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

#define EEPROM_ASSINATURA 0x554D4D32U

#define FLASH_PRESET_START_ADDR 0x08010000U
#define FLASH_PRESET_END_ADDR 0x08020000U
#define FLASH_PRESET_SECTOR FLASH_SECTOR_4

typedef struct {
  uint32_t assinatura;
  uint32_t sequencia;
  uint32_t setpoint_01;
  uint32_t histerese_temperatura_dC;
  uint32_t limite_temperatura_alta_dC;
  uint32_t limite_temperatura_baixa_dC;
  uint32_t zur_umidade;
  uint32_t gur_umidade;
  uint32_t datalog_periodo_s;
  uint32_t checksum;
} preset_record_t;

eeprom_data_t dados;

static uint32_t flash_next_sequence = 1;

static uint32_t checksum_bytes(const void *data, uint32_t size);
static uint32_t preset_checksum(const preset_record_t *record);
static bool preset_record_is_valid(const preset_record_t *record);
static bool flash_read_latest_presets(eeprom_data_t *data);
static bool flash_write_config_record(const eeprom_data_t *data);

void eeprom_init(void) {
  dados = eeprom_read();
}

eeprom_data_t eeprom_read(void) {
  eeprom_data_t result = {0};

  (void)flash_read_latest_presets(&result);

  return result;
}

void eeprom_write_config(const eeprom_data_t *data) {
  if (flash_write_config_record(data)) {
    dados.assinatura = EEPROM_ASSINATURA;
    dados.setpoint_01 = data->setpoint_01;
    dados.histerese_temperatura_dC = data->histerese_temperatura_dC;
    dados.limite_temperatura_alta_dC = data->limite_temperatura_alta_dC;
    dados.limite_temperatura_baixa_dC = data->limite_temperatura_baixa_dC;
    dados.zur_umidade = data->zur_umidade;
    dados.gur_umidade = data->gur_umidade;
    dados.datalog_periodo_s = data->datalog_periodo_s;
  }
}

static bool flash_read_latest_presets(eeprom_data_t *data) {
  preset_record_t best = {0};
  bool found = false;

  for (uint32_t addr = FLASH_PRESET_START_ADDR;
       (addr + sizeof(preset_record_t)) <= FLASH_PRESET_END_ADDR;
       addr += sizeof(preset_record_t)) {
    const preset_record_t *record = (const preset_record_t *)addr;

    if (record->assinatura == 0xFFFFFFFFU) {
      break;
    }

    if (!preset_record_is_valid(record)) {
      continue;
    }

    if (!found || ((int32_t)(record->sequencia - best.sequencia) > 0)) {
      best = *record;
      found = true;
    }
  }

  if (!found) {
    flash_next_sequence = 1;
    return false;
  }

  data->assinatura = EEPROM_ASSINATURA;
  data->setpoint_01 = best.setpoint_01;
  data->histerese_temperatura_dC = best.histerese_temperatura_dC;
  data->limite_temperatura_alta_dC = best.limite_temperatura_alta_dC;
  data->limite_temperatura_baixa_dC = best.limite_temperatura_baixa_dC;
  data->zur_umidade = best.zur_umidade;
  data->gur_umidade = best.gur_umidade;
  data->datalog_periodo_s = best.datalog_periodo_s;
  flash_next_sequence = best.sequencia + 1U;
  return true;
}

static bool flash_write_config_record(const eeprom_data_t *data) {
  uint32_t addr = FLASH_PRESET_START_ADDR;
  preset_record_t record = {
      .assinatura = EEPROM_ASSINATURA,
      .sequencia = flash_next_sequence,
      .setpoint_01 = data->setpoint_01,
      .histerese_temperatura_dC = data->histerese_temperatura_dC,
      .limite_temperatura_alta_dC = data->limite_temperatura_alta_dC,
      .limite_temperatura_baixa_dC = data->limite_temperatura_baixa_dC,
      .zur_umidade = data->zur_umidade,
      .gur_umidade = data->gur_umidade,
      .datalog_periodo_s = data->datalog_periodo_s,
      .checksum = 0,
  };
  record.checksum = preset_checksum(&record);

  while ((addr + sizeof(preset_record_t)) <= FLASH_PRESET_END_ADDR) {
    const preset_record_t *slot = (const preset_record_t *)addr;
    if (slot->assinatura == 0xFFFFFFFFU) {
      break;
    }
    addr += sizeof(preset_record_t);
  }

  HAL_FLASH_Unlock();

  if ((addr + sizeof(preset_record_t)) > FLASH_PRESET_END_ADDR) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;

    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.Sector = FLASH_PRESET_SECTOR;
    erase.NbSectors = 1;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) {
      HAL_FLASH_Lock();
      return false;
    }
    addr = FLASH_PRESET_START_ADDR;
  }

  const uint32_t *words = (const uint32_t *)&record;
  for (uint32_t i = 0; i < ((sizeof(record) + 3U) / 4U); i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (i * 4U),
                          words[i]) != HAL_OK) {
      HAL_FLASH_Lock();
      return false;
    }
  }

  HAL_FLASH_Lock();
  flash_next_sequence++;
  return true;
}

static uint32_t checksum_bytes(const void *data, uint32_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t hash = 2166136261UL;

  for (uint32_t i = 0; i < size; i++) {
    hash ^= bytes[i];
    hash *= 16777619UL;
  }

  return hash;
}

static uint32_t preset_checksum(const preset_record_t *record) {
  return checksum_bytes(&record->sequencia,
                        sizeof(record->sequencia) + sizeof(record->setpoint_01) +
                            sizeof(record->histerese_temperatura_dC) +
                            sizeof(record->limite_temperatura_alta_dC) +
                            sizeof(record->limite_temperatura_baixa_dC) +
                            sizeof(record->zur_umidade) +
                            sizeof(record->gur_umidade) +
                            sizeof(record->datalog_periodo_s));
}

static bool preset_record_is_valid(const preset_record_t *record) {
  if (record->assinatura != EEPROM_ASSINATURA) {
    return false;
  }

  return record->checksum == preset_checksum(record);
}
