/*
 * sensors.h
 *
 * Aquisicao analogica do Ummi Control: temperatura (LM35 diferencial),
 * umidade (HIH3051), tensoes 5 V/12 V (modo teste). Filtro de media de
 * 6 amostras + filtro anti-degrau ("Anidro"). Deteccao de sensor desconectado.
 */
#ifndef INC_SENSORS_H_
#define INC_SENSORS_H_

#include <stdint.h>
#include <stdbool.h>

void sensors_init(void);
/* Passo de aquisicao (chamar a cada ~20 ms). Faz 1 amostra/canal e filtra. */
void sensors_update(void);

/* Temperatura filtrada em decimos de grau C (uso interno do filtro). */
int32_t sensors_temp_dC(void);
/* Temperatura em decimos de grau F (derivada). */
int32_t sensors_temp_dF(void);
/* Temperatura em GRAUS INTEIROS C / F (0..999 na faixa util). */
int32_t sensors_temp_C(void);
int32_t sensors_temp_F(void);
/* Umidade relativa filtrada em decimos de % (0..1000) — uso interno. */
uint16_t sensors_umidade_dUR(void);
/* Umidade relativa em % INTEIRO (0..100). */
uint16_t sensors_umidade_pct(void);
/* Tensoes de alimentacao em mV (modo teste). */
uint16_t sensors_v5_mV(void);
uint16_t sensors_v12_mV(void);

bool sensors_temp_desconectada(void);
bool sensors_umidade_desconectada(void);
bool sensors_umidade_fora_faixa(void);
bool sensors_adc_erro(void);
/* true quando o filtro ja acumulou amostras suficientes para exibir/controlar. */
bool sensors_estavel(void);

/* ADC bruto (medio, 0..4095) por canal — diagnostico no modo teste. */
uint16_t sensors_ad_umidade(void);
uint16_t sensors_ad_temperatura(void);
uint16_t sensors_ad_referencia(void);

#endif /* INC_SENSORS_H_ */
