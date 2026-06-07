/*
 * eeprom.c
 *
 * Persistencia separada:
 * - contador e ultima conferencia na EEPROM externa 24x256 via I2C;
 * - presets de inspecao na flash interna do STM32, emulando EEPROM.
 */

#include "eeprom.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"
#include <stdbool.h>
#include <string.h>

#define EEPROM_ASSINATURA 0x554D4D32U

#define EXT_EEPROM_I2C_ADDRESS (0x50U << 1)
#define EXT_EEPROM_STORAGE_ADDR 0x0000U
#define EXT_EEPROM_PAGE_SIZE 64U
#define EXT_EEPROM_SLOT_SIZE EXT_EEPROM_PAGE_SIZE
#define EXT_EEPROM_SLOT_COUNT 2U
#define EXT_EEPROM_TIMEOUT_MS 20U

#define FLASH_PRESET_START_ADDR 0x08010000U
#define FLASH_PRESET_END_ADDR 0x08020000U
#define FLASH_PRESET_SECTOR FLASH_SECTOR_4

extern I2C_HandleTypeDef hi2c1;

typedef struct {
  uint32_t assinatura;
  uint32_t sequencia;
  uint32_t contador;
  uint32_t ultima_conferencia;
  uint32_t checksum;
} count_record_t;

typedef struct {
  uint32_t assinatura;
  uint32_t sequencia;
  uint32_t setpoint_01;
  uint32_t setpoint_obrigatorio_01;
  uint32_t histerese_temperatura_dC;
  uint32_t limite_temperatura_alta_dC;
  uint32_t limite_temperatura_baixa_dC;
  uint32_t zur_umidade;
  uint32_t gur_umidade;
  uint32_t datalog_periodo_s;
  uint32_t checksum;
} preset_record_t;

eeprom_data_t dados;

static eeprom_data_t count_buffer = {0};
static eeprom_data_t count_last_saved = {0};
static uint8_t count_pending = 0;
static bool ext_eeprom_available = false;
static uint32_t ext_eeprom_retry_tick = 0;
static uint8_t ext_next_slot = 0;
static uint32_t ext_next_sequence = 1;
static uint32_t flash_next_sequence = 1;

static bool ext_wait_ready(uint32_t timeout_ms);
static bool ext_read_raw(uint16_t address, void *data, uint16_t size);
static bool ext_write_raw(uint16_t address, const void *data, uint16_t size);
static uint16_t ext_slot_addr(uint8_t slot);
static bool ext_read_latest_count(eeprom_data_t *data);
static bool ext_write_count_record(const eeprom_data_t *data);

static uint32_t checksum_bytes(const void *data, uint32_t size);
static uint32_t count_checksum(const count_record_t *record);
static bool count_record_is_valid(const count_record_t *record);
static uint32_t preset_checksum(const preset_record_t *record);
static bool preset_record_is_valid(const preset_record_t *record);
static bool flash_read_latest_presets(eeprom_data_t *data);
static bool flash_write_config_record(const eeprom_data_t *data);

void eeprom_init(void) {
  ext_eeprom_available = ext_wait_ready(EXT_EEPROM_TIMEOUT_MS);
  dados = eeprom_read();
  count_last_saved.assinatura = EEPROM_ASSINATURA;
  count_last_saved.contador = dados.contador;
  count_last_saved.ultima_conferencia = dados.ultima_conferencia;
  count_last_saved.setpoint_01 = 0;
  count_last_saved.setpoint_obrigatorio_01 = 0;
}

eeprom_data_t eeprom_read(void) {
  eeprom_data_t result = {0};

  (void)ext_read_latest_count(&result);
  (void)flash_read_latest_presets(&result);

  return result;
}

void eeprom_write(eeprom_data_t *data) {
  eeprom_data_t count_only = {
      .assinatura = EEPROM_ASSINATURA,
      .contador = data->contador,
      .ultima_conferencia = data->ultima_conferencia,
      .setpoint_01 = 0,
      .setpoint_obrigatorio_01 = 0,
  };

  if (memcmp(&count_only, &count_last_saved, sizeof(eeprom_data_t)) == 0) {
    return;
  }

  count_buffer = count_only;
  count_pending = 1;
}

void eeprom_write_presets(uint32_t setpoint_01,
                          uint32_t setpoint_obrigatorio_01) {
  eeprom_data_t config = dados;
  config.assinatura = EEPROM_ASSINATURA;
  config.setpoint_01 = setpoint_01;
  config.setpoint_obrigatorio_01 = setpoint_obrigatorio_01;
  eeprom_write_config(&config);
}

void eeprom_write_config(const eeprom_data_t *data) {
  if (flash_write_config_record(data)) {
    dados.assinatura = EEPROM_ASSINATURA;
    dados.setpoint_01 = data->setpoint_01;
    dados.setpoint_obrigatorio_01 = data->setpoint_obrigatorio_01;
    dados.histerese_temperatura_dC = data->histerese_temperatura_dC;
    dados.limite_temperatura_alta_dC = data->limite_temperatura_alta_dC;
    dados.limite_temperatura_baixa_dC = data->limite_temperatura_baixa_dC;
    dados.zur_umidade = data->zur_umidade;
    dados.gur_umidade = data->gur_umidade;
  }
}

bool eeprom_is_pending(void) { return count_pending; }

void eeprom_process(void) {
  if (!count_pending) {
    return;
  }

  if (!ext_eeprom_available) {
    if ((int32_t)(HAL_GetTick() - ext_eeprom_retry_tick) < 0) {
      return;
    }

    ext_eeprom_available = ext_wait_ready(EXT_EEPROM_TIMEOUT_MS);
    if (!ext_eeprom_available) {
      ext_eeprom_retry_tick = HAL_GetTick() + 1000U;
      return;
    }
  }

  if (ext_write_count_record(&count_buffer)) {
    count_last_saved = count_buffer;
    count_pending = 0;
  }
}

static bool ext_read_latest_count(eeprom_data_t *data) {
  count_record_t best = {0};
  bool found = false;

  if (!ext_eeprom_available) {
    return false;
  }

  for (uint8_t slot = 0; slot < EXT_EEPROM_SLOT_COUNT; slot++) {
    count_record_t record = {0};

    if (!ext_read_raw(ext_slot_addr(slot), &record, sizeof(record))) {
      continue;
    }

    if (!count_record_is_valid(&record)) {
      continue;
    }

    if (!found || ((int32_t)(record.sequencia - best.sequencia) > 0)) {
      best = record;
      found = true;
      ext_next_slot = (uint8_t)((slot + 1U) % EXT_EEPROM_SLOT_COUNT);
    }
  }

  if (!found) {
    ext_next_slot = 0;
    ext_next_sequence = 1;
    return false;
  }

  data->assinatura = EEPROM_ASSINATURA;
  data->contador = best.contador;
  data->ultima_conferencia = best.ultima_conferencia;
  ext_next_sequence = best.sequencia + 1U;
  return true;
}

static bool ext_write_count_record(const eeprom_data_t *data) {
  count_record_t record = {
      .assinatura = EEPROM_ASSINATURA,
      .sequencia = ext_next_sequence,
      .contador = data->contador,
      .ultima_conferencia = data->ultima_conferencia,
      .checksum = 0,
  };
  record.checksum = count_checksum(&record);

  if (!ext_write_raw(ext_slot_addr(ext_next_slot), &record, sizeof(record))) {
    return false;
  }

  ext_next_slot = (uint8_t)((ext_next_slot + 1U) % EXT_EEPROM_SLOT_COUNT);
  ext_next_sequence++;
  return true;
}

static bool ext_wait_ready(uint32_t timeout_ms) {
  uint32_t start = HAL_GetTick();

  do {
    if (HAL_I2C_IsDeviceReady(&hi2c1, EXT_EEPROM_I2C_ADDRESS, 1, 2) ==
        HAL_OK) {
      return true;
    }
  } while ((HAL_GetTick() - start) < timeout_ms);

  return false;
}

static bool ext_read_raw(uint16_t address, void *data, uint16_t size) {
  if (!ext_wait_ready(EXT_EEPROM_TIMEOUT_MS)) {
    return false;
  }

  return HAL_I2C_Mem_Read(&hi2c1, EXT_EEPROM_I2C_ADDRESS, address,
                          I2C_MEMADD_SIZE_16BIT, (uint8_t *)data, size,
                          EXT_EEPROM_TIMEOUT_MS) == HAL_OK;
}

static bool ext_write_raw(uint16_t address, const void *data, uint16_t size) {
  const uint8_t *bytes = (const uint8_t *)data;

  while (size > 0U) {
    uint16_t page_offset = address % EXT_EEPROM_PAGE_SIZE;
    uint16_t page_space = EXT_EEPROM_PAGE_SIZE - page_offset;
    uint16_t chunk = (size < page_space) ? size : page_space;

    if (HAL_I2C_Mem_Write(&hi2c1, EXT_EEPROM_I2C_ADDRESS, address,
                          I2C_MEMADD_SIZE_16BIT, (uint8_t *)bytes, chunk,
                          EXT_EEPROM_TIMEOUT_MS) != HAL_OK) {
      return false;
    }

    if (!ext_wait_ready(EXT_EEPROM_TIMEOUT_MS)) {
      return false;
    }

    address += chunk;
    bytes += chunk;
    size -= chunk;
  }

  return true;
}

static uint16_t ext_slot_addr(uint8_t slot) {
  return (uint16_t)(EXT_EEPROM_STORAGE_ADDR +
                    ((uint16_t)slot * EXT_EEPROM_SLOT_SIZE));
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
  data->setpoint_obrigatorio_01 = best.setpoint_obrigatorio_01;
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
      .setpoint_obrigatorio_01 = data->setpoint_obrigatorio_01,
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

static uint32_t count_checksum(const count_record_t *record) {
  return checksum_bytes(&record->sequencia,
                        sizeof(record->sequencia) + sizeof(record->contador) +
                            sizeof(record->ultima_conferencia));
}

static bool count_record_is_valid(const count_record_t *record) {
  if (record->assinatura != EEPROM_ASSINATURA) {
    return false;
  }

  return record->checksum == count_checksum(record);
}

static uint32_t preset_checksum(const preset_record_t *record) {
  return checksum_bytes(&record->sequencia,
                        sizeof(record->sequencia) + sizeof(record->setpoint_01) +
                            sizeof(record->setpoint_obrigatorio_01) +
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
