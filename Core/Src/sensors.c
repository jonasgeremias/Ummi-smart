/*
 * sensors.c
 *
 * Implementacao da aquisicao analogica. ADC1 por registradores (sem HAL ADC),
 * leitura sequencial dos canais usados no hardware novo:
 *   EA_UMIDADE      = PA1 = ADC_IN1
 *   EA_TEMPERATURA  = PA2 = ADC_IN2
 *   EA_TEMP_REF_GND = PA3 = ADC_IN3
 *   EA_5V           = PA4 = ADC_IN4
 *   EA_12V          = PC0 = ADC_IN10
 */
#include "sensors.h"
#include "config.h"
#include "stm32f4xx_hal.h"

#define ADC_VREF_MV 3300U
#define ADC_FULL_SCALE 4095U

/* Divisor de tensao nas entradas de temperatura e umidade: 1k (serie) + 1k8
 * (p/ GND). Recupera a tensao do sensor: Vsensor = Vadc * (1000+1800)/1800.
 * Permite sensores alimentados em 5 V (LM35, HIH5031) no ADC de 3,3 V. */
#define DIVISOR_NUM 2800U
#define DIVISOR_DEN 1800U

/* Janela de media movel por canal. Original (RF-03) era 6; aumentado para
 * suavizar a leitura (a 20 ms/amostra, 16 amostras = ~320 ms). */
#define MEDIA_N 16U
#define MIN_AMOSTRAS_ESTAVEL MEDIA_N

/* Deteccao de sensor desconectado (decisao de projeto): IGNORA a conversao e
 * considera "aberto" quando o ADC bruto (ja em media) encosta num dos trilhos:
 *   0 .. DESC_RAIL_LO_AD            (curto/entrada p/ GND)
 *   DESC_RAIL_HI_AD .. fundo        (entrada aberta puxada p/ VCC)
 * O estado so muda apos DESC_CONSEC leituras SEGUIDAS no mesmo lado (anti-ruido).
 * Uma entrada valida (sensor conectado) nunca fica encostada no trilho. */
#define DESC_RAIL_LO_AD 10U
#define DESC_RAIL_HI_AD (ADC_FULL_SCALE - 10U)
#define DESC_CONSEC 5U

/* Faixa eletrica do HIH5031 @5V DEPOIS do divisor 1k/1k8 (o que o ADC ve):
 * 0%  -> 0,7575 * 1,8/2,8 = 487 mV
 * 100% -> 3,9375 * 1,8/2,8 = 2531 mV  */
#define UMIDADE_ZERO_mV 487U
#define UMIDADE_FUNDO_mV 2531U

/* Filtro anti-degrau: salto >= 5 dC aguarda 1 ciclo antes de aceitar. */
#define ANTIDEGRAU_LIMIAR_dC 5

typedef struct {
  uint16_t buffer[MEDIA_N];
  uint32_t soma;
  uint8_t idx;
  uint8_t cnt; /* amostras ja acumuladas (satura em MEDIA_N) */
} media_t;

static media_t m_umid, m_temp, m_ref, m_v5, m_v12;
static uint8_t amostras_validas;

static int32_t temperatura_dC;
static int32_t temperatura_dF;
static uint16_t umidade_dUR;
static uint16_t v5_mV;
static uint16_t v12_mV;

/* ADC bruto (medio, 0..4095) por canal — exposto no modo teste. */
static uint16_t ad_umid, ad_temp, ad_ref;

static bool f_temp_desc = true;
static bool f_umid_desc = true;
static bool f_umid_fora = false;
static bool f_adc_erro = false;

/* contadores de leituras seguidas dentro/fora do range (por sensor) */
static uint8_t temp_in_db, temp_out_db, umid_in_db, umid_out_db;

/* Estado do filtro anti-degrau da temperatura. */
static int32_t antidegrau_pendente;
static uint8_t antidegrau_armado;

static void adc1_init_regs(void) {
  __HAL_RCC_ADC1_CLK_ENABLE();
  ADC->CCR = ADC_CCR_ADCPRE_0; /* PCLK2/4 = 24 MHz (<= 36 MHz max F4) */
  ADC1->CR1 = 0;
  ADC1->CR2 = 0;
  /* tempo de amostragem maximo nos canais usados (480 ciclos) */
  ADC1->SMPR1 = ADC_SMPR1_SMP10;
  ADC1->SMPR2 = ADC_SMPR2_SMP1 | ADC_SMPR2_SMP2 | ADC_SMPR2_SMP3 | ADC_SMPR2_SMP4;
  ADC1->SQR1 = 0;
  ADC1->CR2 = ADC_CR2_ADON;
}

static uint16_t adc_le(uint32_t canal) {
  uint32_t timeout;
  uint16_t v = 0U;
  ADC1->SQR3 = canal & ADC_SQR3_SQ1;
  /* DUAS conversoes: a 1a "lava" o sample&hold com o canal selecionado (cobre
   * off-by-one e o bleed de carga do canal anterior) e e descartada; usa-se a
   * 2a. Cada leitura de DR limpa o EOC para a proxima. */
  for (uint8_t k = 0U; k < 2U; k++) {
    timeout = 200000U;
    ADC1->SR = 0;
    ADC1->CR2 |= ADC_CR2_SWSTART;
    while (((ADC1->SR & ADC_SR_EOC) == 0U) && (timeout > 0U)) {
      timeout--;
    }
    if (timeout == 0U) {
      f_adc_erro = true;
      return 0U;
    }
    v = (uint16_t)ADC1->DR;
  }
  return v;
}

static uint16_t media_passo(media_t *f, uint16_t nova) {
  f->soma -= f->buffer[f->idx];
  f->buffer[f->idx] = nova;
  f->soma += nova;
  if (++f->idx >= MEDIA_N) {
    f->idx = 0;
  }
  /* Durante o aquecimento (buffer ainda parcialmente preenchido) divide pelo
   * numero real de amostras, evitando subestimar a media (ex.: falha_alim
   * espuria no boot, pois v5/v12 nao sao protegidos por sensors_estavel). */
  if (f->cnt < MEDIA_N) {
    f->cnt++;
  }
  return (uint16_t)(f->soma / f->cnt);
}

/* Atualiza o estado de desconexao de um sensor pelo ADC bruto medio:
 * "fora" = encostado num trilho (<=LO ou >=HI). So muda apos DESC_CONSEC
 * leituras seguidas no mesmo lado (5 dentro => conectado, 5 fora => aberto). */
static void atualiza_desc(uint16_t md, uint8_t *c_in, uint8_t *c_out,
                          bool *desc) {
  bool fora = (md <= DESC_RAIL_LO_AD) || (md >= DESC_RAIL_HI_AD);
  if (fora) {
    *c_in = 0U;
    if (*c_out < DESC_CONSEC) (*c_out)++;
    if (*c_out >= DESC_CONSEC) *desc = true;
  } else {
    *c_out = 0U;
    if (*c_in < DESC_CONSEC) (*c_in)++;
    if (*c_in >= DESC_CONSEC) *desc = false;
  }
}

void sensors_init(void) {
  adc1_init_regs();
  amostras_validas = 0;
  antidegrau_armado = 0;
  f_temp_desc = true;
  f_umid_desc = true;
  temp_in_db = temp_out_db = umid_in_db = umid_out_db = 0U;
}

static int32_t calc_umidade_dUR(uint32_t umid_mV, int32_t temp_dF_local,
                                bool temp_ok) {
  int32_t rh;

  if (umid_mV <= UMIDADE_ZERO_mV) {
    return 0;
  }
  if (umid_mV >= UMIDADE_FUNDO_mV) {
    rh = 1000;
  } else {
    int32_t calc = ((int32_t)umid_mV * (int32_t)g_config.gur) -
                   (int32_t)g_config.zur;
    if (calc < 0) {
      calc = 0;
    }
    calc /= 100;
    if (calc > 1000) {
      calc = 1000;
    }
    rh = calc;
  }

  /* Compensacao por temperatura do HIH (forma do REQUISITOS, em F x10):
   * RH_true = RH / ((10930 - 12*TF)/10000). Guardas para evitar divisao
   * por zero/negativa. */
  if (temp_ok) {
    int32_t denom = 10930 - (12 * temp_dF_local) / 10; /* TF em F (de dF) */
    if (denom < 2000) {
      denom = 2000; /* guarda inferior */
    }
    rh = (rh * 10000) / denom;
    if (rh > 1000) rh = 1000;
    if (rh < 0) rh = 0;
  }
  return rh;
}

void sensors_update(void) {
  f_adc_erro = false;

  uint16_t a_umid = adc_le(1U);
  uint16_t a_temp = adc_le(2U);
  uint16_t a_ref = adc_le(3U);
  uint16_t a_v5 = adc_le(4U);
  uint16_t a_v12 = adc_le(10U);

  uint16_t md_umid = media_passo(&m_umid, a_umid);
  uint16_t md_temp = media_passo(&m_temp, a_temp);
  uint16_t md_ref = media_passo(&m_ref, a_ref);
  uint16_t md_v5 = media_passo(&m_v5, a_v5);
  uint16_t md_v12 = media_passo(&m_v12, a_v12);

  ad_umid = md_umid;
  ad_temp = md_temp;
  ad_ref = md_ref;

  if (amostras_validas < MEDIA_N) {
    amostras_validas++;
  }

  /* --- Temperatura: diferencial LM35 --- */
  uint32_t temp_dif_ad = (md_temp > md_ref) ? (uint32_t)(md_temp - md_ref) : 0U;
  uint32_t temp_adc_mV = (temp_dif_ad * ADC_VREF_MV) / ADC_FULL_SCALE;
  /* Compensa o divisor de entrada 1k/1k8: Vsensor = Vadc*(1k+1k8)/1k8. */
  uint32_t temp_mV = (temp_adc_mV * DIVISOR_NUM) / DIVISOR_DEN;
  /* LM35 10 mV/C => 1 dC = 1 mV */
  int32_t nova_temp_dC = (int32_t)temp_mV;

  /* --- Umidade (HIH5031 @5V): gur/zur ja sao calibrados para a tensao DEPOIS
   * do divisor 1k/1k8 (faixa no ADC ~0,49..2,53 V), entao NAO recompensa o
   * divisor aqui. Calibrar gur/zur absorve a tolerancia real dos resistores. */
  uint32_t umid_mV = ((uint32_t)md_umid * ADC_VREF_MV) / ADC_FULL_SCALE;
  f_umid_fora = (umid_mV < (UMIDADE_ZERO_mV - 100U)) ||
                (umid_mV > (UMIDADE_FUNDO_mV + 100U));

  /* --- Deteccao de desconexao (apos filtro minimo) --- */
  if (amostras_validas >= MIN_AMOSTRAS_ESTAVEL) {
    /* Desconexao pelo ADC bruto encostado no trilho (ignora a conversao),
     * confirmada por 5 leituras seguidas. */
    atualiza_desc(md_temp, &temp_in_db, &temp_out_db, &f_temp_desc);
    atualiza_desc(md_umid, &umid_in_db, &umid_out_db, &f_umid_desc);
  }

  /* --- Filtro anti-degrau na temperatura --- */
  if (!f_temp_desc) {
    int32_t delta = nova_temp_dC - temperatura_dC;
    if (delta < 0) delta = -delta;
    if (delta >= ANTIDEGRAU_LIMIAR_dC && amostras_validas >= MIN_AMOSTRAS_ESTAVEL) {
      if (!antidegrau_armado) {
        /* primeiro ciclo do salto: segura o valor anterior, arma */
        antidegrau_armado = 1U;
        antidegrau_pendente = nova_temp_dC;
      } else {
        /* salto persistiu: aceita o novo valor (reinit do filtro) */
        temperatura_dC = nova_temp_dC;
        antidegrau_armado = 0U;
      }
    } else {
      temperatura_dC = nova_temp_dC;
      antidegrau_armado = 0U;
    }
  }

  temperatura_dF = (temperatura_dC * 18 + 3200) / 10;

  /* --- Umidade compensada --- */
  umidade_dUR = (uint16_t)calc_umidade_dUR(umid_mV, temperatura_dF, !f_temp_desc);

  /* --- Tensoes (modo teste) --- */
  v5_mV = (uint16_t)((((uint32_t)md_v5 * ADC_VREF_MV) / ADC_FULL_SCALE) *
                     2800U / 1800U); /* divisor 1k + 1k8 */
  v12_mV = (uint16_t)((((uint32_t)md_v12 * ADC_VREF_MV) / ADC_FULL_SCALE) *
                      13300U / 3300U); /* divisor 10k + 3k3 */
}

int32_t sensors_temp_dC(void) { return temperatura_dC; }
int32_t sensors_temp_dF(void) { return temperatura_dF; }

/* Temperatura em GRAUS INTEIROS (arredondada a partir do valor em decimos). */
int32_t sensors_temp_C(void) {
  int32_t v = temperatura_dC;
  return (v >= 0) ? ((v + 5) / 10) : -(((-v) + 5) / 10);
}
int32_t sensors_temp_F(void) {
  int32_t v = temperatura_dF;
  return (v >= 0) ? ((v + 5) / 10) : -(((-v) + 5) / 10);
}
uint16_t sensors_umidade_dUR(void) { return umidade_dUR; }
/* Umidade em % INTEIRO (0..100), arredondada a partir dos decimos internos. */
uint16_t sensors_umidade_pct(void) {
  uint32_t p = ((uint32_t)umidade_dUR + 5U) / 10U;
  return (p > 100U) ? 100U : (uint16_t)p;
}
uint16_t sensors_v5_mV(void) { return v5_mV; }
uint16_t sensors_v12_mV(void) { return v12_mV; }
bool sensors_temp_desconectada(void) { return f_temp_desc; }
bool sensors_umidade_desconectada(void) { return f_umid_desc; }
bool sensors_umidade_fora_faixa(void) { return f_umid_fora; }
bool sensors_adc_erro(void) { return f_adc_erro; }
bool sensors_estavel(void) { return amostras_validas >= MIN_AMOSTRAS_ESTAVEL; }

/* ADC bruto (medio, 0..4095) — diagnostico no modo teste. */
uint16_t sensors_ad_umidade(void) { return ad_umid; }
uint16_t sensors_ad_temperatura(void) { return ad_temp; }
uint16_t sensors_ad_referencia(void) { return ad_ref; }
