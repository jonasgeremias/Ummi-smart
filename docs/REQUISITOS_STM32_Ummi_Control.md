# Documento de Requisitos para Reimplementação em C/STM32
## Firmware "Ummi Control SN" — Engenharia reversa de `011000.asm`

> **Origem:** `011000.asm` (8317 linhas, MPASM) — Betha Eletrônica Ltda — PIC16F877A
> **Programa:** *Ummi Control (Sensor sem Negativo)* — Termo-higrômetro industrial com relógio, datalogger, comunicação serial e controle de 2 relés + buzzer.
> **Destino:** STM32 em linguagem C (HAL/LL).
> **Método:** análise literal e rastreável do Assembly; cada afirmação cita linha(s). Marcações: **[FATO]** confirmado no código · **[INFERÊNCIA]** técnica · **[RECOMENDAÇÃO]**.

---

## 1. Resumo Executivo

O Ummi Control é um **termo-higrômetro industrial** baseado em PIC16F877A @ **4 MHz (cristal XT)** que:

- Mede **temperatura** (sensor LM35 em AN0) e **umidade relativa** (sensor Honeywell HIH em AN1) via ADC interno de 10 bits, Vref = Vdd = 5 V.
- Exibe os valores em **6 displays de 7 segmentos multiplexados** (3 vermelhos + 3 verdes) a ~84 Hz de refresh.
- Mantém **relógio/calendário** via RTC externo **DS1307** (I²C).
- Registra dados em **datalogger** numa EEPROM externa **24LC256** (32 KB, I²C), com registros de 7 bytes, memória circular, CRC e backup do ponteiro.
- Comunica com o software de PC "Ummi Manager" por **RS-232 (USART, 9600 8N1)** com protocolo proprietário (frames iniciados por `*`, CRC = XOR).
- Controla **2 relés** (RA2, RA5) e um **buzzer** (RB7) por **setpoint + histerese** de temperatura/umidade e por **8 eventos horários** cada.
- Armazena toda a configuração na **EEPROM interna** do PIC (mapa de 0x00–0xCD).

**Riscos mais relevantes para a migração** (detalhe na §15):
1. **Ausência de tratamento de overrun/framing da USART** (OERR/FERR) → risco de travamento da recepção serial. **Severidade: Alta.**
2. **Discrepância no mapeamento dos tipos de toque do buzzer** (tabela diz 0=Contínuo; código trata 0 como Pulso Lento e ignora código 3). **Severidade: Média.**
3. **Escrita da EEPROM externa byte-a-byte com `delay` bloqueante de 20 ms** → datalogger bloqueia o laço principal. **Severidade: Média.**
4. **Caminho diferencial AN0–AN3 para temperatura negativa está comentado/inativo** — só mede temperatura positiva (LM35 aterrado). **Severidade: Baixa** (decisão de projeto, mas precisa ser uma decisão consciente no STM32).
5. Dependências de **temporização via Timer1** (display, debounce, timeout serial, cadência do buzzer) acopladas à frequência exata de interrupção. **Severidade: Média** na portabilidade.

---

## 2. Identificação do Firmware e Plataforma

| Item | Valor | Evidência |
|---|---|---|
| MCU | PIC16F877A | linha 53 (`list p=16F877A`), 55 |
| Clock | **4 MHz**, cristal XT → Tcy = 1 µs | inferido: SPBRG=25→9600 bps (966), SSPADD=9→I²C 100 kHz "@4MHz" (970) |
| Config bits | ver tabela abaixo | linha 57 |

**Configuration bits (`__CONFIG`, linha 57):**

| Bit | Estado | Significado | Implicação STM32 |
|---|---|---|---|
| `_CP_ALL` | Ativo | Proteção total de leitura da Flash | RDP/option bytes no STM32 |
| `_CPD_ON` | Ativo | Proteção da Data EEPROM | proteção da área de config |
| `_DEBUG_OFF` | — | Debug in-circuit desligado | RB6/RB7 livres → RB7=Buzzer |
| `_LVP_OFF` | — | Low-Voltage Prog. desligado | RB3 livre como entrada (tecla T/R) |
| `_WRT_HALF` | — | Metade superior da Flash protegida contra auto-escrita | n/a |
| `_BODEN_ON` | Ligado | **Brown-Out Reset** | habilitar BOR/PVD no STM32 |
| `_PWRTE_ON` | Ligado | Power-Up Timer (~72 ms) | atraso de power-up |
| `_WDT_ON` | Ligado | **Watchdog** (prescaler 1:128 via OPTION_REG=0x5F) | habilitar IWDG/WWDG |
| `_XT_OSC` | — | Oscilador a cristal faixa XT (≈4 MHz) | HSE/cristal externo |

**[FATO]** Proteção de cópia total ativa, BOR + PWRT + WDT ligados. **[RECOMENDAÇÃO]** No STM32: habilitar IWDG, BOR (nível adequado), e RDP nível 1/2 se a proteção contra cópia for requisito de produto.

---

## 3. Visão Geral de Funcionalidades e Modos de Operação

| # | Funcionalidade | Resumo |
|---|---|---|
| F1 | Medição de temperatura | LM35 em AN0; °C e °F; filtro de média (damping de 6 amostras) + filtro anti-degrau "Anidro" |
| F2 | Medição de umidade | Honeywell HIH em AN1; compensação por temperatura; saturação 0–100% |
| F3 | Display multiplexado | 6 dígitos 7-seg (3 vermelhos + 3 verdes); modos Termômetro / Relógio; piscar; exibição momentânea de Data/Máx/Mín |
| F4 | Relógio/calendário | DS1307 via I²C; modo 24h forçado; ajuste por serial e por teclas |
| F5 | Teclado | 3 teclas (T/R, Hora, Minuto) com debounce ~60 ms; ajustes e navegação |
| F6 | Datalogger | EEPROM 24LC256; registro de 7 bytes; intervalos ISC/IMC/IHC; memória circular + CRC + backup |
| F7 | Comunicação serial | RS-232 9600 8N1; ~36 comandos; status, config, download do logger, leituras instantâneas |
| F8 | Controle de Relé 1 e Relé 2 | por temperatura e/ou umidade (setpoint+histerese, lógica aquecer/refrigerar) e por 8 eventos horários |
| F9 | Alarme sonoro (buzzer) | por temp alta/baixa, umidade alta/baixa e 8 eventos horários; 4 padrões de toque |
| F10 | Persistência de config | EEPROM interna do PIC (mapa 0x00–0xCD) |
| F11 | Máx/Mín | registro e exibição de máximos e mínimos de temp e UR; zeramento por combo de teclas |

**Modos de exibição:** Termômetro (mostra Temp + UR) ↔ Relógio (mostra hora). Alternância manual (tecla T/R) ou **automática** (exibição alternada, `EE_CONFIG` bit0). Linhas 2288–2314, 95–100.

---

## 4. Mapa de Pinos (Pinout)

Direções extraídas dos TRIS carregados na init (linhas 935–949). `0`=saída, `1`=entrada.

| Pino | Porta.Bit | Direção | Função no firmware | Sinal/conexão | Nível ativo | Sugestão STM32 | Evidência |
|---|---|---|---|---|---|---|---|
| RA0/AN0 | A.0 | Entrada An. | ADC – Temperatura | Sensor LM35 | analógico | ADC_INx | 936, 985, 1264 |
| RA1/AN1 | A.1 | Entrada An. | ADC – Umidade | Sensor HIH | analógico | ADC_INx | 936, 1563 |
| RA2 | A.2 | Saída | **Relé 1** | Bobina relé 1 | **ativo-alto** | GPIO PP | 937, 8162/8167 |
| RA3/AN3 | A.3 | Entrada An. | ADC – par diferencial Temp (negativo) | ref. diferencial | analógico | ADC_INx | 936; código **comentado** 1299–1353 |
| RA4 | A.4 | Entrada | Não usado (open-drain) | — | — | — | 935 |
| RA5 | A.5 | Saída | **Relé 2** | Bobina relé 2 | **ativo-alto** | GPIO PP | 937, 8181/8186 |
| RB0–RB2 | B.0–2 | Entrada | Não referenciados | — | — | — | 939 |
| RB3 | B.3 | Entrada | Tecla **T/R** (Termômetro/Relógio) | botão | **ativo-baixo** | GPIO IN + pull-up | 666 |
| RB4 | B.4 | Entrada | Tecla **MINUTO** | botão | **ativo-baixo** | GPIO IN + pull-up | 674 |
| RB5 | B.5 | Entrada | Tecla **HORA** | botão | **ativo-baixo** | GPIO IN + pull-up | 670 |
| RB6 | B.6 | Entrada | Não usado | — | — | — | 939 |
| RB7 | B.7 | Saída | **Buzzer** | buzzer | **ativo-alto** | GPIO PP | 940, 8058/8148 |
| RC0 | C.0 | Saída | Mux dígito vermelho 1 | transistor | ativo-alto | GPIO PP | 943, 868 |
| RC1 | C.1 | Saída | Mux dígito vermelho 2 | transistor | ativo-alto | GPIO PP | 943, 868 |
| RC2 | C.2 | Saída | Mux dígito vermelho 3 | transistor | ativo-alto | GPIO PP | 943, 868 |
| RC3/SCL | C.3 | Saída | **SCL** I²C | EEPROM ext + DS1307 | — | I2C_SCL (AF, OD) | 606, 979 |
| RC4/SDA | C.4 | Entrada* | **SDA** I²C (bidirecional) | EEPROM ext + DS1307 | — | I2C_SDA (AF, OD) | 603 |
| RC5 | C.5 | Entrada | Não usado | — | — | — | 943 |
| RC6/TX | C.6 | Saída | **TX** USART | RS-232 TX | — | USART_TX (AF) | 943, 963 |
| RC7/RX | C.7 | Entrada | **RX** USART | RS-232 RX | — | USART_RX (AF) | 943, 981 |
| RD0–RD6 | D.0–6 | Saída | **Segmentos a–g** (barramento comum) | displays | — | GPIO PP ×7 | 946, 880 |
| RD7 | D.7 | Saída | Segmento **ponto decimal** | display DP | ativo-alto | GPIO PP | 946, 888 |
| RE0 | E.0 | Saída | Mux dígito verde 1 | transistor | ativo-alto | GPIO PP | 949, 896 |
| RE1 | E.1 | Saída | Mux dígito verde 2 | transistor | ativo-alto | GPIO PP | 949, 896 |
| RE2 | E.2 | Saída | Mux dígito verde 3 | transistor | ativo-alto | GPIO PP | 949, 896 |

\* SDA fica como entrada no TRIS porque o periférico MSSP controla o pino; no STM32 será I2C em alternate function open-drain.

**Valores exatos carregados na init:** TRISA=`0xDB` (935), TRISB=`0x7F` (939), TRISC=`0xB0` (942), TRISD=`0x00` (945), TRISE=`0x00` (948).

---

## 5. Periféricos Internos e Configuração

| Periférico | Recurso PIC | Registro/valor (linha) | Função | Equivalente STM32 |
|---|---|---|---|---|
| ADC | 10 bits, Vref=Vdd | ADCON1=`0x84` (960, ADFM=1 right-just.), ADCON0=`0x41` (984, AN0, Tad=8·Tosc) | aquisição Temp/UR | ADC1 12 bits (right-aligned) |
| Timer 1 | 16 bits, prescaler 1:1, clk Fosc/4 | T1CON=`0x01` (987); recarga `0xF840` (642–645) | base de tempo / mux display | TIMx ~504 Hz |
| USART | assíncrona 8 bits | TXSTA=`0x24` (BRGH=1), SPBRG=25 (966), RCSTA=`0x90` (981) | RS-232 9600 8N1 | USARTx |
| MSSP (I²C) | Master | SSPCON=`0x28` (977), SSPADD=9 (969), SSPSTAT=`0x80` (972) | EEPROM ext + DS1307, 100 kHz | I2Cx |
| EEPROM interna | 256 B | EECON1/EECON2 unlock 0x55/0xAA (2696–2700) | persistência de config | Flash emulada / EEPROM |
| Interrupções | vetor único @4 | INTCON=`0x40` PEIE (954), PIE1=`0x21` (957) | TMR1+RX (TX dinâmico) | NVIC |
| OPTION_REG | pull-ups, WDT 1:128 | `0x5F` (951) | pull-ups RB, prescaler WDT | GPIO pull-ups, IWDG |

---

## 6. Protocolos de Comunicação

### 6.1 USART / RS-232
- **Baud 9600, 8N1, assíncrono** (TXSTA/RCSTA/SPBRG, linhas 963–982). BRGH=1.
- **[FATO]** Sem paridade por hardware.

### 6.2 Protocolo proprietário "Ummi Manager"
- **Início de frame:** caractere `*` (0x2A). Linhas 3334–3342.
- **Tamanho do frame (NBCR)** determinado pelo **2º byte** (código do comando), por faixas ASCII (`load_nbcr`, 3443–3488):

| 2º byte | Tamanho total (com `*` e CRC) |
|---|---|
| ≥ `'y'`(0x79) | 5 bytes |
| ≥ `'a'`(0x61) | 4 bytes |
| ≥ `'A'`(0x41) | 3 bytes |
| ≥ `':'`(0x3A) | 9 bytes |
| caso contrário | 10 bytes |

- **CRC = XOR de 8 bits** de todos os bytes do frame, inclusive o `*` (último byte do frame; verificado em `checa_crc_rc`, 3520–3544). Não é CRC polinomial.
- **Resposta de ACK** para comandos de escrita = caractere `'F'` (0x46) (`tx_ok`, 4418).
- **Handshake de download** = caractere `'#'` (0x23) enviado pelo PC após cada bloco.

#### Tabela de comandos (1º byte de dados = `BUF_RX1`)

| Cód | ASCII | Função | Resposta | Linha |
|---|---|---|---|---|
| 0x4A | J | Exibir °C | F | 3548/3789 |
| 0x48 | H | Exibir °F | F | 3554/3801 |
| 0x64 | d | Ajustar dia (RTC) | F | 3560/3805 |
| 0x65 | e | Ajustar mês | F | 3566/3821 |
| 0x61 | a | Ajustar ano | F | 3572/3837 |
| 0x68 | h | Ajustar hora | F | 3578/3853 |
| 0x69 | i | Ajustar minuto (zera seg) | F | 3584/3869 |
| 0x7A | z | Calibrar ZERO UR | F | 3590/3890 |
| 0x79 | y | Calibrar GANHO UR | F | 3596/3912 |
| 0x44 | D | Habilitar logger | F | 3602/3934 |
| 0x45 | E | Desabilitar logger | F | 3608/3946 |
| 0x5A | Z | Zerar logger | F | 3614/3950 |
| 0x73 | s | Intervalo logger – segundos (ISC) | F | 3620/3990 |
| 0x6D | m | Intervalo logger – minutos (IMC) | F | 3626/4003 |
| 0x63 | c | Intervalo logger – horas (IHC) | F | 3632/4016 |
| 0x54 | T | **Status completo** (24 bytes) | stream+CRC | 3638/4117 |
| 0x43 | C | **Iniciar download do logger** | cadeias | 3644/4359 |
| 0x56 | V | Versão firmware | 4 B+CRC | 3650/4029 |
| 0x58 | X | **Temperatura instantânea** | 4 B+CRC | 3656/4074 |
| 0x55 | U | **Umidade instantânea** | 3 B+CRC | 3662/4102 |
| 0x46 | F | Habilitar exibição alternada | F | 3669/5696 |
| 0x47 | G | Desabilitar exibição alternada | F | 3677/5711 |
| 0x62 | b | Tipo do alarme horário | F | 3685/5721 |
| 0x30 | 0 | Config alarme buzzer por Temp | F | 3693/5733 |
| 0x31 | 1 | Config alarme buzzer por UR | F | 3701/5742 |
| 0x3A | : | Config Relé 1 por Temp | F | 3709/5751 |
| 0x3B | ; | Config Relé 1 por UR | F | 3717/5760 |
| 0x3C | < | Config Relé 2 por Temp | F | 3725/5769 |
| 0x3D | = | Config Relé 2 por UR | F | 3733/5778 |
| 0x32 | 2 | Config evento alarme horário | F | 3741/5787 |
| 0x33 | 3 | Config evento Relé 1 horário | F | 3749/5792 |
| 0x34 | 4 | Config evento Relé 2 horário | F | 3757/5797 |
| 0x57 | W | **Solicitar todas as configs** (11 mensagens) | streams+CRC | 3765/5864 |
| 0x52 | R | **Status das saídas** (Buzzer/Relé1/Relé2) | 5 B+CRC | 3773/6177 |

#### Mensagens de configuração (comando `W`, `requer_config`)
11 mensagens (NUM_MSGN 0–0x0A), cada uma lida da EEPROM interna + CRC XOR (linhas 5864–6175). As mensagens 3, 7 e 0x0A (horários) são sub-laços de 8 eventos × 6 bytes. IDs (1º byte): `G,0,1,2,b,:,;,3,<,=,4`.

#### Download do logger
- Comando `C` define `REG_DOWN` (ponteiro de leitura) e `REG_LAST` (limite). Se sem virada: começa em 0x0000; se houve virada: começa no ponteiro atual. Linhas 4359–4416.
- Cada bloco = **9 bytes**: `Dia, Mês, Ano, Hora(bit7=°C/°F), Min(bit7=sinal), TempMSB, TempLSB, UR, CRC`. Linhas 4426–4562.
- PC envia `#` para pedir o próximo bloco; ao atingir REG_LAST, equipamento envia `"FIM"` + CRC. Linhas 3346–3399.

---

## 7. Dispositivos Externos (I²C)

| Dispositivo | Endereço I²C (W/R) | Uso | Evidência |
|---|---|---|---|
| EEPROM externa **24LC256** (32 KB) | 0xA0 / 0xA1 | Datalogger (0x0000–0x7FFF) | 2829/2855/2890; 589–593 |
| RTC **DS1307** | 0xD0 / 0xD1 | Relógio/calendário | 2947/2966/3001 |
| Sensor **LM35** | — (analógico AN0) | Temperatura | cabeçalho 34 |
| Sensor **Honeywell HIH** | — (analógico AN1) | Umidade relativa | cabeçalho/EEPROM 334–337 |

**[FATO]** I²C é **hardware MSSP Master**, não bit-bang (os `#define SDA/SCL` são vestigiais; só `bsf SCL` é usado na init, linha 979). Velocidade **100 kHz**.

**DS1307:** registradores 0x00 (seg, bit7=CH), 0x01 (min), 0x02 (hora, bit6=12/24h), 0x04 (dia), 0x05 (mês), 0x06 (ano), 0x07 (control). Formato **BCD**. Inicialização força **CH=0** (relógio rodando) e **modo 24h** (linhas 1234–1255). Escrita de registrador usa 1 byte de endereço; EEPROM usa 2 bytes.

---

## 8. Subsistema Analógico (ranges, Vref, conversões)

- **Resolução:** 10 bits, **right-justified** (ADFM=1). LSB = 5 V / 1024 = **4,8828 mV** (linha 1266).
- **Vref = Vdd = 5 V.**
- **Aquisição:** loop de espera ~512 µs antes de cada GO (1275, 1565).
- **Detecção de sensor desconectado:** leitura ADC = 0 (ambos os bytes zero) → exibe `--`. Temp: bit `SENSOR_DESC,0` (1320–1331); UR: bit `SENSOR_DESC,1` força UR=0 (1583–1596). Logger não grava se algum sensor desconectado (4600–4603).

| Grandeza | Canal | Vref | Faixa | Fórmula (do código) | Resultado |
|---|---|---|---|---|---|
| Temperatura | AN0 | 5 V | 0–5 V (só positivo, LM35 aterrado) | `TC×10 = (ADC×49)/100`, arred. resto−50 | °C com 1 casa decimal |
| Temp °F | derivado | — | — | `TF = ((TC×18)+3200)/100` (TC já ×10); const 3200=0x0C80 | °F |
| Umidade | AN1 | 5 V | 0–5 V | `RH = (ADC×49 − Z_UR)/G_UR`, compensada: `TRH = RH3/((10930 − 12·TF)/100)`; satura em 100 | 0–100% |

**Constantes:** fator LM35 = ×49/100 (≈0,49); `Z_UR`=8000 (×10000, zero=0,8), `G_UR`=310 (×1000, ganho=0,031); const compensação 10930 (=0x2AB2). Linhas 1357–1769.

**Filtros:**
- **Damping** (média móvel deslizante de 6 amostras): `SOMAT += VM; SOMAT −= DAMP; DAMP = SOMAT/6`. Linhas 1792–1975.
- **Filtro anti-degrau "Anidro"** (versão 01.07.01): se |nova − anterior| ≥ **5 unidades**, trata como degrau; aguarda 1 ciclo (~1 s) e, se persistir, reinicializa o filtro (salto direto). Tratamento de sinal por `DAMP_NEG` (contador 0–4) para evitar oscilação perto de 0 °C. Linhas 1807–1905.

**[INFERÊNCIA]** O caminho diferencial AN0–AN3 (para temperatura negativa) está **comentado/inativo** (1299–1353): a medição efetiva é single-ended em AN0 e **não mede negativos** (decisão para evitar oscilação por EMI, cabeçalho linha 34).

---

## 9. Tabela de Limiares e Estados (Habilitado/Desabilitado)

| Item | Estado | Condição/valor no código | Polaridade | Linha |
|---|---|---|---|---|
| Relé 1 (RA2) | Liga / Desliga | `bsf` / `bcf PORTA,2` | ativo-alto | 8167 / 8162 |
| Relé 2 (RA5) | Liga / Desliga | `bsf` / `bcf PORTA,5` | ativo-alto | 8186 / 8181 |
| Buzzer (RB7) | Liga / Desliga | `bsf` / `bcf PORTB,7` | ativo-alto | 8148 / 8058 |
| Teclas (RB3/4/5) | Pressionada | pino = 0 | ativo-baixo | 666/670/674 |
| `EE_RELEx_TEMP` | Habilita controle por temp | bit0 = 1 (`0xFF`=não programado→desab) | — | 6320–6330 |
| `EE_RELEx_UR` | Habilita controle por umidade | bit0 = 1 | — | 6343/6380 |
| `EE_RELEx_TEMP_LOG` | 0 = aquecedor (liga abaixo) / 1 = refrigerador (liga acima) | `NUM_EVENTO,0` | — | 7217 |
| `EE_RELEx_UR_LOG` | 0 = umidificador / 1 = desumidificador | `NUM_EVENTO,0` | — | 7247 |
| `EE_ALM_TEMP` / `EE_ALM_UR` | 0=Desab,1=Alta,2=Baixa,3=Ambos | bits de `AUX_CONTROL` | — | 6400/6412/6625 |
| `EE_ALM_*_T*` / `EE_ALM_TIPO` | Tipo de toque (doc: 0=Contínuo,1=Lento,2=Médio,3=Rápido) | ⚠ ver §15 (despacho diverge) | — | 369, 8099–8107 |
| `ESCALA` bit0 | escala exibida 0=°F / 1=°C | — | — | 95 |
| `ESCALA` bit1 | 0=Termômetro / 1=Relógio | — | — | 96 |
| `ESCALA` bit5 | Logger habilitado (1=habilitado) | — | — | 100 |
| `EE_CONFIG` bit0 | modo exibição 0=Alternada / 1=Convencional | — | — | 353 |
| `FLAG_VIRADA` bit0 | logger em modo circular (já virou) | — | — | 226, 5148 |
| Histerese liga (aquecedor/umidif.) | MED ≤ (SP − HN) | — | — | 7458/7892 |
| Histerese desliga (aquecedor/umidif.) | MED ≥ (SP + HP) | — | — | 7499/7835 |
| Histerese liga (refrig./desumid.) | MED ≥ (SP + HP) | — | — | 7576/8027 |
| Histerese desliga (refrig./desumid.) | MED ≤ (SP − HN) | — | — | 7633/7977 |
| Proteção HP=0 & HN=0 | **não desliga o relé** | testa MINUTO_A(HP) e MINUTO_D(HN) = 0 | — | 7756–7764 |
| Combinação final do pino | **OR**(horário ∨ temp ∨ umidade) | sequência de `btfsc`/`goto` | — | 8152–8186 |

> **Setpoints e histereses são armazenados ×10** (uma casa decimal) na EEPROM interna. Ex.: `EE_RELE1_TEMP_MSB/LSB` (SP), `_HN`, `_HP`. Mapa completo nas linhas 362–580.

---

## 10. Entradas Digitais / Teclado / Debounce

- **3 teclas** ativas (hardware suporta até 9, não usado): **T/R** (RB3), **HORA** (RB5), **MINUTO** (RB4), ativo-baixo, pull-ups internos habilitados (OPTION_REG `RBPU=0`).
- **Leitura sincronizada com o display:** lida dentro da ISR do Timer1 quando `INDICE_DISP==5` (comuns do mux em zero), evitando leitura falsa. Linhas 662–676.
- **Debounce:** contador incrementa 1×/varredura (~11,9 ms); ação dispara em contagem **5** ⇒ ~**60 ms** de confirmação. Linhas 689–739.
- **Funções:** T/R alterna Termômetro/Relógio; em modo relógio + T/R: Hora/Minuto incrementam o RTC; senão exibem Data ou Máx/Mín momentaneamente; **Hora+Minuto** = zera Máx/Mín. Linhas 738–802.

---

## 11. Saídas Digitais / Relés / Buzzer / Intertravamentos

**Acionamento centralizado em `aciona_saidas` (8041–8189). Todas as saídas são ativo-alto.**

- **Controle por temperatura (relés):** histerese de dois pontos dependente do estado:
  - **Aquecedor** (LOG=0): liga quando MED ≤ (SP−HN), desliga quando MED ≥ (SP+HP).
  - **Refrigerador** (LOG=1): liga quando MED ≥ (SP+HP), desliga quando MED ≤ (SP−HN).
- **Controle por umidade (relés):** mesma lógica (umidificador / desumidificador). UR usa apenas 8 bits.
- **Combinação:** `estado_pino = HORÁRIO OR TEMP OR UMIDADE`. Sem prioridade; cada fonte mantém seu próprio flag (`FLAG_TEMP_UMID`, `FLAG_RELEx_HORA`). Linhas 8152–8186.
- **Proteção:** se HN=0 e HP=0, o relé **nunca desliga** (intertravamento contra config inválida). Linhas 7756–7764.

**Buzzer:** dispara por temp alta/baixa, umidade alta/baixa, ou evento horário; a **primeira fonte ativa** define o **tipo de toque** (contínuo/pulso lento/médio/rápido). Padrões gerados por `CONT_BUZZER` (recarga 0x04 lento, 0x01 médio, 0 rápido) com reset periódico a 0xAA na rotina de temporização (2370–2377, 8093–8148).

**Flags de estado (`FLAG_TEMP_UMID` 0xe7):** bit0=Buzzer T.Alta, 1=Buzzer T.Baixa, 2=Buzzer UR Alta, 3=Buzzer UR Baixa, 4=Relé1 temp, 5=Relé1 umid, 6=Relé2 temp, 7=Relé2 umid. Linhas 283–291.

---

## 12. Datalogger e Memória Persistente

**Registro = 7 bytes** (EEPROM externa 24LC256, 32 KB ⇒ **4681 amostras**):

| Byte | Conteúdo | Observação | Linha |
|---|---|---|---|
| 0 | Dia | binário (de BCD) | 4904 |
| 1 | Mês | binário | 4938 |
| 2 | Ano | binário | 4974 |
| 3 | Hora | **bit7: 1=°C / 0=°F** | 5010/5030 |
| 4 | Minuto | **bit7: 1=Temp negativa** | 5048/5068 |
| 5 | Temperatura | TEMP_C_LSB ou TEMP_F_LSB | 5093 |
| 6 | Umidade (%) | `UR` | 5113 |

- **Disparo temporal:** ISC/IMC/IHC mutuamente exclusivos (segundos→minutos→horas). Loga somente quando `tempo mod intervalo == 0` (divisão exata) e `tempo ≠ EE_TIME_L_LOG` (anti-duplicidade). Linhas 4612–4770.
- **Intervalos válidos:** ISC/IMC ∈ {1,2,3,4,5,6,10,12,15,20,30}; IHC ∈ {1,2,3,4,6,8,12,24} (frações exatas do relógio). Cabeçalho 21–28.
- **Memória circular:** ao atingir 0x7FFE, seta `FLAG_VIRADA`, grava em EEPROM interna e zera o ponteiro. Linhas 5130–5158.
- **Integridade:** ponteiro `REG_DA` + **CRC = MSB XOR LSB** + **cópia de backup** completa na EEPROM interna. Na inicialização valida CRC primário → backup → reset total se ambos falharem. Linhas 4778–4890, 5160–5224.
- **[FATO/RISCO]** Escrita da EEPROM externa é **byte-a-byte com `delay_20ms` por byte** (4936) — sem page-write. ~7 bytes × 20 ms ≈ **140 ms por amostra** de bloqueio.

**EEPROM interna do PIC:** mapa de configuração 0x00–0xCD (calibração UR, intervalos logger, ponteiros, escala, setpoints de alarme/relé, 8 eventos horários × 3 fontes). Escrita com unlock 0x55/0xAA e GIE desabilitado durante a janela (2682–2708).

---

## 13. RTC, Base de Tempo, Timers e Interrupções

**Interrupções (vetor único @4):** ordem de polling TMR1 → RX → TX (sem prioridade de HW). Contexto salvo: W, STATUS (via swapf), FSR, PCLATH — restauração correta. Paginação tratada com PAGESEL. Linhas 613–920.

| Fonte | Habilitação | Rotina | Função |
|---|---|---|---|
| TMR1 | PIE1.0 + PEIE + GIE | `int_tmr1` (641) | recarga, mux display, teclado, contadores |
| USART RX | RCIE (PIE1.5) | `int_rc` (3334) | recepção/protocolo |
| USART TX | TXIE (dinâmico) | `int_tx` (4569) | transmissão do buffer |

**Timer 1 (base de tempo):** recarga `0xF840` ⇒ 1984 contagens × 1 µs ≈ **1,984 ms** ⇒ **≈504 Hz de interrupção**. Refresh do display completo (6 dígitos) ≈ **84 Hz**. Os comentários "80 Hz / 60 Hz" referem-se ao refresh do quadro completo.

**Contadores derivados:**

| Contador | Função | Período |
|---|---|---|
| INDICE_DISP | varredura de display (0–5) | 1 dígito/IRQ |
| DEBOUNCE | debounce do teclado | disparo em 5 ⇒ ~60 ms |
| CONT_REST | timeout de sincronismo serial (`*`) | 254 ⇒ ~504 ms (carga 3493) |
| CONT_PISCA | piscar display | ciclo 160 ⇒ ~317 ms (~3,15 Hz) |
| CONT_DATE | exibição momentânea Data/Máx/Mín | carga 20 (~1,5 s) |
| CONT_BUZZER | cadência do buzzer | recarga 0x04/0x01/0xAA |

**RTC DS1307:** leitura de hora/minuto/segundo em `evento_horario` (6740–6805), BCD→binário, para `HORA_ATUAL/MINUTO_ATUAL/SEGUNDO_ATUAL`.

---

## 14. Máquina de Estados e Fluxo Principal

1. **Reset/Init** (`inicio`, 923–1040): limpa portas → configura TRIS/OPTION/INTCON/PIE1/ADCON1/USART/I²C → limpa RAM (3 bancos) → inicializa Timer1 → habilita GIE (último passo).
2. **Pós-init:** lê escala °C/°F e parâmetros do logger da EEPROM interna; inicializa RTC (CH=0, 24h).
3. **Laço principal:** lê ADC (Temp, UR) → aplica conversões e filtros → atualiza Máx/Mín → lê RTC → avalia controles (relés/buzzer por temp/umidade/horário em `verif_config2`) → aciona saídas (`aciona_saidas`) → executa logger se intervalo bateu → prepara buffer de display.
4. **Assíncrono (ISR):** Timer1 multiplexa display e lê teclado ~504 Hz; USART RX monta frames; USART TX esvazia buffer.

---

## 15. Bugs, Vulnerabilidades e Riscos

| ID | Risco / Bug | Sev. | Evidência | Impacto | Ação no STM32 |
|---|---|---|---|---|---|
| R01 | **Sem tratamento de OERR/FERR** na USART | **Alta** | ausência de teste em RCSTA (grep negativo) | overrun trava a recepção até reset | tratar erros do USART (flag ORE/FE), limpar e ressincronizar; usar DMA/IDLE |
| R02 | **Mapeamento de tipo de toque do buzzer divergente** | **Média** | doc 369 (0=Contínuo) vs despacho 8099–8107 (0=Lento, código 3 cai em fall-through) | tipo de alarme configurado ≠ tipo tocado | redefinir enum claro {Contínuo, Lento, Médio, Rápido} e validar |
| R03 | **Escrita EEPROM externa byte-a-byte + delay 20 ms bloqueante** | **Média** | 4936 | ~140 ms de bloqueio por amostra do logger | usar page-write + escrita não bloqueante (DMA/IT) |
| R04 | **Temperatura negativa não medida** (caminho diferencial AN3 comentado) | Baixa | 1299–1353, cabeçalho 34 | não há leitura de T<0 °C | decidir conscientemente: sensor com offset / ADC diferencial / sensor bipolar |
| R05 | **CRC = XOR de 8 bits** (protocolo e logger) | Baixa | 3533, 4529 | baixa detecção de erros (não pega erros pares) | manter por compatibilidade com Ummi Manager OU negociar CRC-16 |
| R06 | **Acoplamento forte às temporizações do Timer1** | Média | display/debounce/timeout/buzzer dependem de ~504 Hz | regressão se a base de tempo mudar | parametrizar tempos em ms, derivar de SysTick |
| R07 | **Proteção "HN=0 & HP=0 não desliga relé"** | Baixa (atenção) | 7756–7764 | config inválida deixa relé travado ligado | replicar deliberadamente; documentar para o operador |
| R08 | **GIE desabilitado durante escrita da EEPROM interna** | Baixa | 2694–2702 | janela sem display/serial (já mitigado no histórico) | escrita atômica curta; no STM32 não bloquear ISRs longas |
| R09 | **Comentários de hardware inconsistentes** (24LC02 vs 24LC256) | Info | 585, 4901 | confunde dimensionamento | confirmar 24LC256 (32 KB) — código usa 0x0000–0x7FFF |
| R10 | **Pinos sem função definida** (RA4, RB0-2, RB6, RC5) | Info | TRIS init | podem esconder recurso de hardware | confirmar no esquemático antes de reaproveitar |

---

## 16. Considerações de Migração PIC → STM32 (C)

1. **Banco de memória / paginação:** todo o acesso a SFR por banco e PCLATH desaparece no STM32 (Cortex-M é flat). Simplifica, mas exige reescrever a lógica, não traduzir instrução-a-instrução.
2. **BCD do RTC:** se usar DS1307 (mesma placa), manter rotinas BCD↔binário. Se migrar para RTC interno do STM32, o RTC já entrega BCD nativamente.
3. **Timing:** substituir a base do Timer1 por **SysTick (1 ms)** + timer de hardware para o multiplex do display; reexpressar todos os contadores (debounce 60 ms, timeout serial 504 ms, piscar 317 ms, exibição 1,5 s) em **milissegundos absolutos**.
4. **ADC:** STM32 tem 12 bits — recalcular as constantes de conversão (×49/100 do LM35 e Z_UR/G_UR do HIH) para a nova resolução e Vref real. Manter Vref bem definida (idealmente Vref+ dedicada, não Vdd ruidosa).
5. **Display multiplexado:** manter o esquema 3 vermelhos (RC0-2) + 3 verdes (RE0-2) + segmentos (PORTD) ou migrar para driver dedicado; respeitar a leitura de teclado sincronizada (índice 5).
6. **USART:** implementar tratamento de OERR/FERR (R01) e, de preferência, RX por DMA/IDLE-line. Manter o protocolo (frame `*`, NBCR por faixa, CRC XOR, ACK `F`, handshake `#`) para compatibilidade com o Ummi Manager.
7. **I²C:** EEPROM com **page-write** e escrita não bloqueante; manter endereços 0xA0 (EEPROM) e 0xD0 (DS1307).
8. **EEPROM interna:** STM32 não tem EEPROM real — usar **emulação em Flash** ou EEPROM externa para o mapa de config (0x00–0xCD) e para o ponteiro do logger com CRC+backup (manter a estratégia de integridade).
9. **Ordem de inicialização:** habilitar interrupções por último, como no original.
10. **Watchdog/BOR:** habilitar IWDG e BOR equivalentes.

---

## 17. Requisitos Funcionais e Não-Funcionais

### Funcionais

| ID | Requisito | Origem (linha) | Critério de aceitação |
|---|---|---|---|
| RF-01 | Medir temperatura via LM35 em AN0, 0–5 V, e converter para °C/°F com 1 casa decimal | 1357–1525 | leitura coincide com original ±0,1 °C |
| RF-02 | Medir UR via HIH em AN1 com compensação por temperatura, saturada 0–100% | 1562–1769 | UR coincide ±1% e satura em 100 |
| RF-03 | Aplicar damping de 6 amostras + filtro anti-degrau de 5 unidades | 1792–1905 | resposta de filtro equivalente |
| RF-04 | Detectar sensor desconectado (ADC=0) e exibir `--` | 1320–1596 | `--` quando entrada aberta |
| RF-05 | Exibir 6 dígitos multiplexados (3 vermelhos/3 verdes) com modos Termômetro/Relógio e exibição alternada | 838–908, 2288 | refresh sem flicker, ponto decimal no relógio |
| RF-06 | Ler/ajustar RTC DS1307 (24h), data/hora por serial e teclas | 1234–1255, 3805–3886 | hora correta após ajuste |
| RF-07 | Teclado de 3 teclas com debounce ~60 ms e funções definidas | 662–802 | sem repique; combos funcionam |
| RF-08 | Datalogger: registro de 7 bytes, intervalos ISC/IMC/IHC, memória circular, CRC + backup | 4600–5254 | amostragem em frações exatas; recuperação por CRC |
| RF-09 | Comunicação serial 9600 8N1 com protocolo proprietário (≈36 comandos, CRC XOR, ACK `F`) | 3334–6207 | Ummi Manager comunica sem alteração |
| RF-10 | Download do logger com handshake `#`/`FIM` e blocos de 9 bytes | 4359–4562 | download íntegro com CRC |
| RF-11 | Controle de Relé 1 e Relé 2 por temperatura (aquecer/refrigerar) com histerese SP±H | 7205–7668 | comutação nos pontos (SP−HN)/(SP+HP) |
| RF-12 | Controle de Relé 1/2 por umidade (umidificar/desumidificar) com histerese | 7785–8035 | idem para UR |
| RF-13 | Controle por 8 eventos horários por fonte (alarme, Relé 1, Relé 2) | 6731–7199 | liga/desliga nos horários, inclusive janela cruzando meia-noite |
| RF-14 | Combinação OR das fontes para cada saída | 8152–8186 | qualquer fonte ativa liga a saída |
| RF-15 | Buzzer com 4 padrões de toque por temp/UR alta/baixa e horário | 8043–8148 | padrão correto por configuração (corrigir R02) |
| RF-16 | Registrar e exibir Máx/Mín de temp e UR, zerável por combo de teclas | 767–797 | máx/mín corretos; zeram no combo |
| RF-17 | Persistir toda a configuração (mapa 0x00–0xCD) de forma não volátil | 332–580 | config sobrevive a power-cycle |

### Não-funcionais

| ID | Requisito | Origem |
|---|---|---|
| RNF-01 | Watchdog habilitado e alimentado periodicamente | _WDT_ON, 1007 |
| RNF-02 | Brown-out reset habilitado | _BODEN_ON |
| RNF-03 | Recuperação automática do ponteiro do logger por CRC + backup | 4778–4890 |
| RNF-04 | Tratamento de erro de barramento I²C (NACK → STOP) | 2928–2932 |
| RNF-05 | **(Novo)** Tratamento de overrun/framing da USART — *ausente no original (R01)* | §15 |
| RNF-06 | Datalogger não deve bloquear o laço por >X ms — *melhorar vs 140 ms atual (R03)* | §15 |
| RNF-07 | Compatibilidade total com o protocolo do Ummi Manager (frames/CRC/ACK) | §6 |
| RNF-08 | Proteção de leitura do firmware (RDP) se a cópia for requisito comercial | _CP_ALL |

---

## 18. Lacunas e Pontos a Confirmar

1. **Esquemático elétrico** — confirmar conexões de RA4, RB0-RB2, RB6, RC5 (sem uso no código), pull-ups dos relés/buzzer e drivers de display. **Sem evidência suficiente no código.**
2. **Vref real do ADC** — o código assume Vref=Vdd=5 V; confirmar se há referência dedicada (impacta precisão).
3. **Modelo exato do sensor de umidade** — cabeçalho cita Honeywell HIH; confirmar HIH-3610/4000/4030 para a fórmula correta no STM32.
4. **Tipo de toque do buzzer (R02)** — definir com o cliente o mapeamento correto {Contínuo/Lento/Médio/Rápido}.
5. **Temperatura negativa (R04)** — decidir se o produto STM32 deve medir T<0 °C (hardware diferente).
6. **Capacidade do logger** — confirmar 24LC256 (32 KB / 4681 amostras) vs comentários inconsistentes de 24LC02.
7. **Damping da UR** — bloco existe (`SOMAT_UR/DAMP_UR`) e é simétrico ao de temperatura, mas não foi lido integralmente; confirmar se há threshold de degrau para UR. **Sem evidência adicional.**

---

*Documento gerado por engenharia reversa rastreável de `011000.asm`. Números de linha referem-se ao arquivo fonte original.*
