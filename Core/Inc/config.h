/*
 * config.h
 *
 * Mapa de configuracao persistente do Ummi Control (termo-higrometro).
 * Substitui o mapa da EEPROM interna do PIC (0x00-0xCD). Persistido em Flash
 * (setor dedicado) com assinatura + CRC e gravacao log-structured (desgaste).
 *
 * Temperatura (medida, setpoints, histereses, alarmes) em GRAUS INTEIROS,
 * faixa 0..999, em C ou F conforme escala_celsius (sem casa decimal).
 * Umidade (medida, setpoints, histereses, alarmes) em % INTEIRO 0..100.
 * Horarios em minutos do dia (0-1439).
 */
#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

/* Assinatura muda quando o layout/semantica de ummi_config_t muda, para
 * invalidar registros antigos na Flash (ex.: deci-grau -> grau inteiro,
 * inclusao de eeprom_tipo). */
#define CONFIG_ASSINATURA 0x554D4335U /* "UMC5" */
#define CONFIG_VERSAO 5U

/* Versao / marca do firmware (exibida no splash e no comando 'V'). */
#define FW_MARCA_BE1 1
#define FW_VERSAO_MAJOR 1U
#define FW_VERSAO_MINOR 0U

/* Faixa de temperatura inteira exibida/configurada. */
#define TEMP_MIN_GRAU 0
#define TEMP_MAX_GRAU 999

/* Limite ABSOLUTO de seguranca (grau inteiro, na escala em uso). Corta o
 * aquecimento de forma incondicional, independente do alarme configuravel
 * (que vem desabilitado de fabrica) e mesmo no modo trava da histerese.
 * Ultima barreira de software contra runaway termico com sensor coerente. */
#define TEMP_CORTE_ABS_C 110

/* Tipo de EEPROM externa do datalogger (g_config.eeprom_tipo). */
#define EEPROM_24LC256 0U /* 32 KB, pagina 64 B (hardware legado) */
#define EEPROM_24AA512 1U /* 64 KB, pagina 128 B (hardware atual soldado) */
/* Default de fabrica: hardware atual usa a 24AA512. */
#define EEPROM_TIPO_DEFAULT EEPROM_24AA512

#define EVENTOS_POR_FONTE 8U

/* Fonte de controle de um rele (bitfield em rele_cfg_t.fontes) */
#define CTRL_POR_TEMPERATURA 0x01U
#define CTRL_POR_UMIDADE 0x02U

/* Logica de um rele */
#define RELE_LOGICA_AQUECER 0U   /* liga abaixo de SP-HN (umidificar p/ UR) */
#define RELE_LOGICA_REFRIGERAR 1U /* liga acima de SP+HP (desumidificar p/ UR) */

/* Tipo de alarme (temp ou UR): 0=Desab,1=Alta,2=Baixa,3=Ambos */
#define ALM_DESABILITADO 0U
#define ALM_ALTA 1U
#define ALM_BAIXA 2U
#define ALM_AMBOS 3U

/* Padroes de toque do buzzer (R02 redefinido de forma clara) */
#define BUZZER_CONTINUO 0U
#define BUZZER_LENTO 1U
#define BUZZER_MEDIO 2U
#define BUZZER_RAPIDO 3U

/* Base de intervalo do datalogger */
#define LOG_BASE_SEGUNDOS 0U
#define LOG_BASE_MINUTOS 1U
#define LOG_BASE_HORAS 2U

/* Evento horario: liga em [liga_min, desliga_min). Suporta cruzar meia-noite. */
typedef struct {
  uint16_t liga_min;    /* 0-1439, 0xFFFF = desabilitado */
  uint16_t desliga_min; /* 0-1439 */
} evento_horario_t;

/* Configuracao de um rele por grandeza (temperatura ou umidade) */
typedef struct {
  uint8_t habilita; /* 0/1 */
  uint8_t logica;   /* RELE_LOGICA_AQUECER / REFRIGERAR */
  int32_t setpoint; /* grau inteiro (temp) ou dUR (umidade) */
  int32_t hn;       /* histerese negativa (mesma unidade do setpoint) */
  int32_t hp;       /* histerese positiva (mesma unidade do setpoint) */
} controle_grandeza_t;

typedef struct {
  controle_grandeza_t temperatura;
  controle_grandeza_t umidade;
  evento_horario_t eventos[EVENTOS_POR_FONTE];
} rele_cfg_t;

typedef struct {
  /* --- escala / exibicao --- */
  uint8_t escala_celsius;     /* 1=C, 0=F */
  uint8_t exibicao_alternada; /* 1=alterna Term/Relogio automaticamente */
  uint8_t eeprom_tipo;        /* EEPROM_24LC256 / EEPROM_24AA512 */

  /* --- calibracao da umidade (HIH3051) --- */
  uint32_t zur; /* zero  (default 6950) */
  uint32_t gur; /* ganho (default 314)  */

  /* --- datalogger --- */
  uint8_t log_habilitado;
  uint8_t log_base;        /* LOG_BASE_* */
  uint16_t log_intervalo;  /* valor (s/min/h conforme base) */

  /* --- alarme/buzzer --- */
  uint8_t alm_temp;   /* ALM_* */
  uint8_t alm_ur;     /* ALM_* */
  uint8_t alm_tipo;   /* BUZZER_* */
  int32_t alm_temp_alta_dC;  /* grau inteiro (0..999) */
  int32_t alm_temp_baixa_dC; /* grau inteiro (0..999) */
  int32_t alm_ur_alta_dUR;  /* % inteiro (0..100) */
  int32_t alm_ur_baixa_dUR; /* % inteiro (0..100) */
  evento_horario_t alm_eventos[EVENTOS_POR_FONTE];

  /* --- reles --- */
  rele_cfg_t rele1;
  rele_cfg_t rele2;

  /* --- limites de alimentacao (modo teste / alarme) --- */
  uint16_t v5_min_mV, v5_max_mV;
  uint16_t v12_min_mV, v12_max_mV;
} ummi_config_t;

extern ummi_config_t g_config;

/* Carrega da Flash (valida assinatura/CRC); aplica defaults se invalido. */
void config_init(void);
/* Grava o estado atual de g_config (com CRC) na Flash. Retorna true se ok. */
bool config_salva(void);
/* Reaplica defaults industriais em g_config (nao grava). */
void config_defaults(void);
/* Validacao/clamp de toda a estrutura (chamada apos load e antes de uso). */
void config_valida(ummi_config_t *c);

#endif /* INC_CONFIG_H_ */
