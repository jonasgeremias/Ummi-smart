// Driver para a memória EEPROM 24x256, utilizando a interface I2C do STM32F4
// Inicializa a comunicação com a memória EEPROM, verificando se o dispositivo
// está pronto

#include <stdint.h>
#include "24x256.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_i2c.h"

extern I2C_HandleTypeDef hi2c1; // Declaração do handle do I2C, definido em main.c

void ee_wait_ready(void) { // Aguarda até que o dispositivo esteja pronto para a
                           // próxima operação
  while (HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_ADDRESS, 5, 10) != HAL_OK);
}

// Inicializa a comunicação com a memória EEPROM, verificando se o dispositivo
// está pronto
uint8_t
ee_init(void) { // Verifica se o dispositivo está pronto para comunicação
  return HAL_I2C_IsDeviceReady(&hi2c1, EEPROM_ADDRESS, 5, 10) == HAL_OK;
}

// Escreve um byte na memória EEPROM no endereço especificado
void ee_write_byte(uint16_t mem_address, uint8_t data) {
  HAL_I2C_Mem_Write(&EEPROM_I2C,
                    EEPROM_ADDRESS,
                    mem_address,
                    I2C_MEMADD_SIZE_16BIT,
                    &data,
                    1,
                    HAL_MAX_DELAY);

  ee_wait_ready();
}

void ee_write_data(uint16_t mem_address, const void *ptr, uint16_t size) {

   const uint8_t *data = (const uint8_t *)ptr;

    while (size > 0) {
    uint16_t page_offset = mem_address % EE_PAGE_SIZE;
    uint16_t space = EE_PAGE_SIZE - page_offset;
    uint16_t chunk = (size < space) ? size : space;

    HAL_I2C_Mem_Write(&EEPROM_I2C, EEPROM_ADDRESS, mem_address, I2C_MEMADD_SIZE_16BIT,
                      (uint8_t*)data, chunk, HAL_MAX_DELAY);

    ee_wait_ready();

    mem_address += chunk;
    data += chunk;
    size -= chunk;
  }
}
// Lê um byte da memória EEPROM do endereço especificado
uint8_t ee_read_byte(uint16_t mem_address) {
   uint8_t data;
   HAL_I2C_Mem_Read(&EEPROM_I2C,
                    EEPROM_ADDRESS,
                    mem_address,
                    I2C_MEMADD_SIZE_16BIT,
                    &data,
                    1,
                    HAL_MAX_DELAY);
   return data;
}

void ee_read_data(uint16_t mem_address, void *ptr, uint16_t size) {
  HAL_I2C_Mem_Read(&EEPROM_I2C,
                  EEPROM_ADDRESS, 
                  mem_address, 
                  I2C_MEMADD_SIZE_16BIT,
                  (uint8_t *)ptr, 
                  size, 
                  HAL_MAX_DELAY);
}

void ee_clear_page(uint16_t page_address) {
  uint8_t buffer[EE_PAGE_SIZE];

   for (int i = 0; i < EE_PAGE_SIZE; i++) {
      buffer[i] = EE_DEFAULT_ERASED_VALUE;
   }

   HAL_I2C_Mem_Write(&EEPROM_I2C, 
                     EEPROM_ADDRESS, 
                     page_address,
                     I2C_MEMADD_SIZE_16BIT, 
                     buffer, 
                     EE_PAGE_SIZE,
                     HAL_MAX_DELAY);

   ee_wait_ready();
}

void ee_erase_all(void) {
  uint32_t addr = 0;
  for (addr = 0; addr < EE_24X256_SIZE; addr += EE_PAGE_SIZE) {
    ee_clear_page(addr);
  }
}

void ee_write_int32_redundant(uint16_t addr, uint32_t value){
   for (uint8_t i = 0; i < EE_REDUNDANCY_COPIES; i++){
      ee_write_data(addr + (i * 4), &value, 4);
   }
}

void ee_write_int32_safe(uint16_t addr, uint32_t value){
   uint32_t atual = ee_read_int32_redundant(addr);

   if (atual != value){
      ee_write_int32_redundant(addr, value);
   }
}

uint32_t ee_read_int32_redundant(uint16_t addr){
   uint32_t v[3];

   // Lê as 3 cópias redundantes
   for (uint8_t i = 0; i < EE_REDUNDANCY_COPIES; i++){
      ee_read_data(addr + (i * 4), &v[i], 4);
   }

   // caso 01: todas as cópias são iguais, valor confiável
   if (v[0] == v[1] && v[1] == v[2]){
      return v[0]; // Todas as cópias são iguais, valor confiável
   }

   // caso 02: v[0] é diferente, mas v[1] e v[2] são iguais, corrige v[0]
   if (v[1] == v[2]){
      ee_write_data(addr, &v[1], 4); // Corrige a cópia corrompida
      return v[1]; // v[0] é diferente, mas v[1] e v[2] são iguais, valor confiável
   }

   // caso 03: v[0] e v[2] são iguais, mas v[1] é diferente, corrige v[1]
   if (v[0] == v[2]){
      ee_write_data(addr + 4, &v[0], 4); // Corrige a cópia corrompida
      return v[0]; // v[1] é diferente, mas v[0] e v[2] são iguais, valor confiável
   }

   // caso 04: v[0] e v[1] são iguais, mas v[2] é diferente, corrige v[2]
   if (v[0] == v[1]){
      ee_write_data(addr + 8, &v[0], 4); // Corrige a cópia corrompida
      return v[0]; // v[2] é diferente, mas v[0] e v[1] são iguais, valor confiável
   }

   // caso críticos: todos diferentes
   return EE_ERROR_VALUE;
}

uint8_t ee_read_int8_redundant(uint16_t addr)
{
    uint8_t v[3];

    for (uint8_t i = 0; i < 3; i++)
    {
        v[i] = ee_read_byte(addr + i);
    }

    if (v[0] == v[1] && v[1] == v[2])
        return v[0];

    if (v[1] == v[2])
    {
        ee_write_byte(addr, v[1]);
        return v[1];
    }

    if (v[0] == v[2])
    {
        ee_write_byte(addr + 1, v[0]);
        return v[0];
    }

    if (v[0] == v[1])
    {
        ee_write_byte(addr + 2, v[0]);
        return v[0];
    }

    return EE_ERROR_VALUE_8;
}