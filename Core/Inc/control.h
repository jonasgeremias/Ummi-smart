/*
 * control.h — Controle de saidas do Ummi Control.
 *
 * 2 reles por temperatura (aquecer/refrigerar) e umidade (umidif/desumid)
 * com histerese de 2 pontos; 8 eventos horarios por fonte; combinacao OR;
 * buzzer com 4 padroes; alarmes; estado seguro e anti-chattering.
 */
#ifndef INC_CONTROL_H_
#define INC_CONTROL_H_

#include <stdint.h>
#include <stdbool.h>

void control_init(void);
/* Chamar a cada 50 ms. */
void control_tick_50ms(void);

/* Inibe (true) ou libera (false) todas as saidas (reles e buzzer).
 * Usado para forcar estado seguro enquanto se navega no menu. */
void control_inibe(bool inibir);

bool control_rele1(void);
bool control_rele2(void);
bool control_buzzer_ativo(void);

bool control_alm_temp_alta(void);
bool control_alm_temp_baixa(void);
bool control_alm_ur_alta(void);
bool control_alm_ur_baixa(void);
bool control_falha_sensor_temp(void);
bool control_falha_sensor_ur(void);
bool control_falha_alimentacao(void);

/* Bitfield de status (para protocolo serial / "R"). */
uint16_t control_status_flags(void);

#endif /* INC_CONTROL_H_ */
