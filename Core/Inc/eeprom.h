#ifndef INC_EEPROM_H_
#define INC_EEPROM_H_

#include <stdint.h>

typedef struct{
   uint32_t assinatura;
   uint32_t setpoint_01;
   uint32_t histerese_temperatura_dC;
   uint32_t limite_temperatura_alta_dC;
   uint32_t limite_temperatura_baixa_dC;
   uint32_t zur_umidade;
   uint32_t gur_umidade;
   uint32_t datalog_periodo_s;
} eeprom_data_t;

extern eeprom_data_t dados;

void eeprom_init(void);

eeprom_data_t eeprom_read(void);
void eeprom_write_config(const eeprom_data_t *data);



#endif /* INC_EEPROM_H_ */
