/*
 * eeprom.c
 *
 *  Created on: Mar 24, 2026
 *      Author: Lucas
 */

#include "eeprom.h"
#include "main.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define EEPROM_START_ADDR 0x08010000
#define EEPROM_END_ADDR 0x0801FFFF
#define EEPROM_ASSINATURA 0xA5A5A5A5

eeprom_data_t dados;

static eeprom_data_t eeprom_buffer = {
    0}; // Buffer para armazenar os dados da EEPROM
static uint8_t eeprom_pending =
    0; // Flag para indicar que uma leitura da EEPROM está pendente
static eeprom_data_t eeprom_last_saved = {
    0}; // Última vez que os dados foram salvos na EEPROM

// INICIA EEPROM VIRTUAL
void eeprom_init(void) {
  eeprom_last_saved = eeprom_read(); // Lê os dados da EEPROM ao iniciar
}

// LÊ DADOS DA EEPROM
eeprom_data_t eeprom_read(void) {
  uint32_t addr = EEPROM_START_ADDR;
  eeprom_data_t last = {0};

  while (addr < EEPROM_END_ADDR) {
    eeprom_data_t *ptr = (eeprom_data_t *)addr;

    if (ptr->assinatura != EEPROM_ASSINATURA)
      break;

    last = *ptr;
    addr += sizeof(eeprom_data_t);
  }

  return last;
}

// ESCREVE DADOS NA EEPROM
void eeprom_write(eeprom_data_t *data) {
  if (memcmp(data, &eeprom_last_saved, sizeof(eeprom_data_t)) == 0)
    return;

  eeprom_buffer = *data;
  eeprom_buffer.assinatura = EEPROM_ASSINATURA;
  eeprom_pending = 1;
}

bool eeprom_is_pending(void) { return eeprom_pending; }

void eeprom_process(void) {
  if (!eeprom_pending)

    return;

  eeprom_pending = 0;
  uint32_t addr = EEPROM_START_ADDR;

  while (addr < EEPROM_END_ADDR) {
    eeprom_data_t *ptr = (eeprom_data_t *)addr;

    if (ptr->assinatura != EEPROM_ASSINATURA)
      break;

    addr += sizeof(eeprom_data_t);
  }

  HAL_FLASH_Unlock();
  FLASH_EraseInitTypeDef erase;
  uint32_t error;

  erase.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase.Sector = FLASH_SECTOR_4;
  erase.NbSectors = 1;
  erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;

  if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) {
    Error_Handler();
  }

  addr = EEPROM_START_ADDR;

  uint32_t *ptr = (uint32_t *)&eeprom_buffer;

  for (int i = 0; i < (sizeof(eeprom_data_t) + 3) / 4; i++) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + (i * 4), ptr[i]) !=
        HAL_OK) {
      Error_Handler();
    }
  }

  HAL_FLASH_Lock();
  eeprom_last_saved = eeprom_buffer;

  //   static uint8_t estado = 0;

  // switch (estado) {

  // case 0:
  //     if (eeprom_flagaux_salvar) {
  //         estado = 1;
  //     }
  //     break;

  // case 1:
  //     eeprom_write(&dados);
  //     eeprom_flagaux_salvar = 0;
  //     estado = 0;
  //     break;
  // }
}