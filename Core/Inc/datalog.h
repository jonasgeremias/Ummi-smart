/*
 * datalog.h — Datalogger em EEPROM externa I2C @0xA0.
 * Chip selecionavel por g_config.eeprom_tipo: 24AA512 (64 KB/pag 128 B, default
 * do hardware atual) ou 24LC256 (32 KB/pag 64 B, hardware legado).
 *
 * Registro de 9 bytes (= bloco de download Ummi Manager):
 *   [0] Dia  [1] Mes  [2] Ano  [3] Hora(b7=1:C / 0:F)
 *   [4] Min(b7=1: temperatura negativa)  [5] TempMSB  [6] TempLSB
 *   [7] UR(%)  [8] CRC = XOR dos bytes 0..7
 * Memoria circular; cabecalho com assinatura/CRC + copia de backup.
 */
#ifndef INC_DATALOG_H_
#define INC_DATALOG_H_

#include <stdint.h>
#include <stdbool.h>

#define DATALOG_RECORD_SIZE 9U

void datalog_init(void);
bool datalog_reset(void);
/* Chamar 1x/segundo: decide e grava conforme intervalo ISC/IMC/IHC. */
void datalog_tick_1s(void);

uint16_t datalog_total(void);       /* registros validos */
uint16_t datalog_capacidade(void);
uint16_t datalog_proximo_indice(void);
bool datalog_ok(void);

/* Le bloco logico (0 = mais antigo). Preenche 9 bytes. */
bool datalog_le_bloco(uint16_t indice_logico, uint8_t bloco[DATALOG_RECORD_SIZE]);

/* Download serial: fixa o snapshot do anel e retorna o total a enviar. */
uint16_t datalog_download_inicia(void);
/* Le bloco logico (0 = mais antigo) usando o snapshot de datalog_download_inicia. */
bool datalog_download_le(uint16_t indice_logico,
                         uint8_t bloco[DATALOG_RECORD_SIZE]);

#endif /* INC_DATALOG_H_ */
