/*
 * 24x256.h
 *
 *  Created on: Apr 7, 2026
 *      Author: Lucas
 */

#ifndef INC_24X256_H_
#define INC_24X256_H_

#include "stm32f4xx_hal.h"

extern I2C_HandleTypeDef hi2c1;

//configuração do endereço do dispositivo
#define EEPROM_I2C hi2c1
#define EEPROM_ADDRESS (0x50 << 1) // A0, A1, A2 = GND

#define EE_24X256_SIZE 32768 // 32KB
#define EE_PAGE_SIZE 64

#define EE_DEFAULT_ERASED_VALUE 0x70 // valor lido de uma célula apagada (0xFF) com pull-up de 10k

#define EE_REDUNDANCY_COPIES 3 // número de cópias redundantes para dados críticos
#define EE_ADDR_CONTADOR 0x0000 // endereço do contador
#define EE_ADDR_ULTIMA_CONF 0x0010 // endereço da última configuração salva
#define EE_ADDR_SETPOINT 0x0020 // endereço do setpoint
#define EE_ADDR_SETPOINT_OBRIGATORIO 0x0030 // endereço do setpoint obrigatório
#define EE_ERROR_VALUE 0xFFFFFFFF // valor de erro para leitura redundante
#define EE_ERROR_VALUE_8 0xFF // valor de erro para leitura redundante de 8 bits

uint8_t ee_init(void);
void ee_wait_ready(void);

void ee_write_byte(uint16_t mem_address, uint8_t data);
uint8_t ee_read_byte(uint16_t mem_address);

void ee_write_data(uint16_t mem_address, const void *ptr, uint16_t size);
void ee_read_data(uint16_t mem_address, void *ptr, uint16_t size);

void ee_clear_page(uint16_t page_address);
void ee_erase_all(void);

uint32_t ee_read_int32_redundant(uint16_t addr);
void ee_write_int32_redundant(uint16_t addr, uint32_t value);
void ee_write_int32_safe(uint16_t addr, uint32_t value);

uint8_t ee_read_int8_redundant(uint16_t addr);

#endif /* INC_24X256_H_ */
