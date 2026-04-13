/*
 * eeprom.h
 *
 *  Created on: Mar 24, 2026
 *      Author: Lucas
 */

#ifndef INC_EEPROM_H_
#define INC_EEPROM_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct{
   uint32_t assinatura;
   uint16_t setpoint_01;
   uint16_t setpoint_obrigatorio_01;
} eeprom_data_t;

extern eeprom_data_t dados;
extern uint8_t eeprom_flagaux_salvar;

void eeprom_init(void); // inicializa virtual eeprom

eeprom_data_t eeprom_read(void); // le o valor da virtual eeprom
//void eeprom_write(uint32_t data); // escreve o valor na virtual eeprom
void eeprom_write(eeprom_data_t *data);
void eeprom_process(void); // processa a escrita na virtual eeprom
bool eeprom_is_pending(void); // retorna se houve uma escrita na virtual eeprom



#endif /* INC_EEPROM_H_ */
