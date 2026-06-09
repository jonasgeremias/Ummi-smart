# Aplicação — Ummi Control (termo-higrômetro) sobre STM32F410RB

> **Base:** este documento consolida os requisitos da aplicação a partir do **código STM32 existente**
> (`UMMI SECAGEM FUMO`) e dos **requisitos de `REQUISITOS_STM32_Ummi_Control.md`** (termo-higrômetro
> *Ummi Control*, engenharia reversa do PIC `011000.asm`).
>
> **Decisão:** o produto-alvo é o **Ummi Control** (termo-higrômetro industrial) executando no
> **hardware novo** (RTC interno, EEPROM 24AA512), com **protocolo serial Ummi Manager (`*`)**,
> mantendo o **modo teste de 5 V/12 V** e a configuração de **GUR/ZUR por tela e por serial**, com
> **proteções industriais**.
>
> As divergências e decisões de migração estão em [analise.md](analise.md).

---

## 1. Objetivo

Firmware STM32 para termo-higrômetro industrial que:
- mede **temperatura** (LM35 diferencial) e **umidade relativa** (HIH3051) e as exibe em **6 dígitos**;
- mantém **relógio/calendário** no **RTC interno** (24 h);
- registra **datalogger** em **EEPROM externa 24AA512** (memória circular + CRC + backup);
- comunica com o PC "Ummi Manager" pelo **protocolo serial `*`** (CRC XOR);
- controla **2 relés** e um **buzzer** por **setpoint+histerese** de temperatura/umidade e por **8 eventos horários** por fonte;
- persiste toda a configuração de forma não volátil;
- possui **modo teste** com leitura das tensões **5 V** e **12 V**;
- aplica **proteções industriais** (watchdog, brown-out, estado seguro, integridade de dados).

---

## 2. Hardware (mapeamento adotado)

| Recurso | Sinal lógico | Pino | Observação |
|---|---|---|---|
| Temperatura (LM35) | `EA_TEMPERATURA` − `EA_TEMP_REF_GND` | PA2 / PA3 | diferencial por software, 10 mV/°C |
| Umidade (HIH3051) | `EA_UMIDADE` | PA1 | 0,7575 V=0% … 3,9375 V=100% |
| Tensão 5 V | `EA_5V` | PA4 | modo teste (divisor 1k+1k8) |
| Tensão 12 V | `EA_12V` | PC0 | modo teste (divisor 10k+3k3) |
| Relé 1 | `SAIDA_01` | PC7 | ativo-alto |
| Relé 2 | `SAIDA_02` | PC9 | ativo-alto |
| Buzzer | `SD_BUZZER` | PB5 | ativo-alto, 4 padrões |
| Tecla T/R | `BOTAO_RELOGIO` | PC1 | ativo-baixo, pull-up |
| Tecla HORA | `BOTAO_MAX` | PC3 | ativo-baixo, pull-up |
| Tecla MINUTO | `BOTAO_MIN` | PC2 | ativo-baixo, pull-up |
| Display 7-seg | `SEG_A..G`,`SEG_DP` | PA5/6/7,PC4/5,PB0/1/2 | barramento comum |
| Mux 6 dígitos | `DISPLAY_*` | PC6,PB15/14/13/12/10 | multiplexado ~1 kHz/dígito |
| EEPROM 24AA512 | `I2C1` SCL/SDA | PB6/PB7 | 64 KB, página 128 B |
| Serial Ummi Manager | `USART1` TX/RX | PA9/PA10 | `*`/CRC XOR (baud `UMMI_BAUD`) |
| LEDs status | `LED_STATUS1..4` | PC10/11/12/13 | R1/R2/Buzzer/Alarme |
| Entradas digitais | `ENTRADA_01/02` | PB8/PB9 | reservadas |

---

## 3. Requisitos funcionais (resumo executável)

> Os critérios de aceitação detalhados estão no §17 de `REQUISITOS_STM32_Ummi_Control.md`.

- **RF-01 Temperatura:** LM35 diferencial → décimos de °C; conversão °F derivada; 1 casa decimal.
- **RF-02 Umidade:** HIH3051 com `ZUR/GUR`, compensação por temperatura, saturação 0–100%.
- **RF-03 Filtros:** média móvel de **6 amostras** + filtro anti-degrau ("Anidro", salto ≥5).
- **RF-04 Sensor desconectado:** temp dif < 5 AD ou UR < 0,3 V → exibir `---` e estado seguro.
- **RF-05 Display:** 6 dígitos; modos **Termômetro** (T+UR) e **Relógio** (HH.MM.SS); **exibição alternada** opcional; ponto decimal conforme modo.
- **RF-06 RTC:** ajuste por serial (`d/e/a/h/i`) e por teclas; 24 h.
- **RF-07 Teclado:** 3 teclas com debounce ~60 ms; combos (HORA+MINUTO = zera máx/mín).
- **RF-08 Datalogger:** registro de **7 bytes**, intervalos **ISC/IMC/IHC**, memória circular, CRC + backup do ponteiro.
- **RF-09 Serial:** protocolo `*` (NBCR por faixa, CRC XOR, ACK `F`), ~36 comandos.
- **RF-10 Download:** blocos de **9 bytes** com handshake `#` e `"FIM"`.
- **RF-11 Relés por temperatura:** aquecer (liga ≤ SP−HN) / refrigerar (liga ≥ SP+HP), histerese de 2 pontos.
- **RF-12 Relés por umidade:** umidificar/desumidificar com histerese.
- **RF-13 Eventos horários:** 8 eventos por fonte (alarme, Relé 1, Relé 2), inclusive janela cruzando meia-noite.
- **RF-14 Combinação:** `saída = horário OR temp OR umidade`.
- **RF-15 Buzzer:** 4 padrões {Contínuo, Lento, Médio, Rápido} pela primeira fonte ativa.
- **RF-16 Máx/Mín:** registro e exibição de máximos/mínimos; zerável por combo.
- **RF-17 Persistência:** mapa de configuração completo, não volátil, com CRC + valores padrão.

### Requisitos adicionais (HW novo / cliente)
- **RA-01 Modo teste:** tela que exibe **5 V** (3 díg. dir.) e **12 V** (3 díg. esq.) medidos, com pontos decimais (`12.0 05.0`). Acesso oculto (5× tecla HORA/`+`).
- **RA-02 GUR/ZUR por tela e serial:** parâmetros de calibração da umidade editáveis no menu (`P05ZUR`, `P06GUR`) e via serial (`z`/`y` do Ummi Manager **e** alias de texto).
- **RA-03 Proteções industriais:** IWDG, BOR/PVD, I²C com recuperação, USART com tratamento de erro, estado seguro.

---

## 4. Modos de operação / máquina de estados

| Modo | Descrição | Saídas |
|---|---|---|
| Init | Carrega config, inicializa RTC/EEPROM/serial, splash | desligadas (estado seguro) |
| Termômetro | Exibe Temp+UR; controla relés/buzzer; loga | controladas |
| Relógio | Exibe hora; controla relés/buzzer; loga | controladas |
| Menu | Edição de parâmetros (inversor-style), inclui ZUR/GUR/RTC | controladas |
| Teste/Diagnóstico | Exibe 5 V/12 V | controladas |
| Falha | Estado seguro + alarme conforme severidade | desligadas |

A alternância Termômetro↔Relógio é manual (tecla T/R) ou automática (config `exibição alternada`).

---

## 5. Conversões analógicas (12 bits, Vref 3,3 V)

- **LSB** = 3300 mV / 4095 ≈ 0,806 mV.
- **Temperatura:** `mV = (ADC_temp − ADC_ref) × 3300 / 4095`; `T[dC] = mV / 10` (10 mV/°C ⇒ 1 °C = 10 mV; em décimos: `T_dC = mV`). °F = `(T_dC×18 + 3200)/10`.
- **Umidade:** `UR = ((mV × GUR) − ZUR)/100`, saturada 0–100,0%; compensação por temperatura conforme HIH3051.
- **5 V:** `mV_real = mV_adc × (1000+1800)/1800`.
- **12 V:** `mV_real = mV_adc × (10000+3300)/3300`.
- **Filtros:** buffer circular de 6 amostras (cadência 20 ms) + anti-degrau (salto ≥5 dC reinit).

---

## 6. Datalogger (24AA512)

- **Layout:** cabeçalho em `0x0200`, **backup do cabeçalho** em `0x0300`, registros a partir de `0x0400`.
- **Registro de 7 bytes:** `Dia, Mês, Ano, Hora[b7=°C/°F], Min[b7=sinal], Temp, UR`.
- **Capacidade:** (65536 − base) / 7 ≈ **9 k amostras**.
- **Intervalos:** ISC (s) ∈ {1,2,3,4,5,6,10,12,15,20,30}; IMC (min) idem; IHC (h) ∈ {1,2,3,4,6,8,12,24} — mutuamente exclusivos; loga quando `tempo mod intervalo == 0` e anti-duplicidade.
- **Integridade:** ponteiro + CRC (MSB⊕LSB) + cópia de backup; na init valida primário→backup→reset.
- **Reset:** por menu (`P07LOG`) e por serial (`Z`).
- **Download:** comando `C` define ponteiros; blocos de 9 bytes; PC envia `#`; fim = `"FIM"`+CRC.

---

## 7. Protocolo serial Ummi Manager

- **Físico:** USART1, `UMMI_BAUD` 8N1 (default 9600; ajustável p/ HM-10).
- **Frame:** inicia em `*` (0x2A); tamanho (NBCR) pela faixa ASCII do 2º byte; CRC = XOR de 8 bits de todos os bytes (inclui `*`).
- **ACK de escrita:** `'F'` (0x46). **Handshake download:** `'#'`.
- **Comandos:** os ~36 da tabela §6.2 do REQUISITOS (J,H,d,e,a,h,i,z,y,D,E,Z,s,m,c,T,C,V,X,U,F,G,b,0,1,:,;,<,=,2,3,4,W,R...).
- **Tratamento de erro:** OERR/FERR/PE/NE limpos na ISR; ressincroniza no próximo `*`; timeout de sincronismo.

---

## 8. Menu local (estilo inversor) — parâmetros com tela

Navegação: segurar `T/R` 3 s entra/sai; `HORA`(+) / `MINUTO`(−) navegam e editam (com turbo);
`T/R` solto confirma. Itens incluem (telas):

| Código | Tela | Parâmetro | Faixa |
|---|---|---|---|
| `P01` | `P01  T` | Setpoint temperatura | 0–999 dC |
| `P02` | `P02HIS` | Histerese | 1–200 dC |
| `P03` | `P03ALT` | Limite alta | ≥ baixa |
| `P04` | `P04BAI` | Limite baixa | ≤ alta |
| `P05` | `P05ZUR` | **Zero UR (ZUR)** | 0–65535 |
| `P06` | `P06GUR` | **Ganho UR (GUR)** | 1–65535 |
| `P07` | `P07LOG` | Reset datalogger | não/sim |
| `P08` | `P08PER` | Período/Intervalo logger | conforme ISC/IMC/IHC |
| `P09` | `P09DAT` | Data RTC (DDMMAA) | válido |
| `P10` | `P10HOR` | Hora RTC (HHMM00) | válido |
| `P11` | `P11rE1` | Config Relé 1 (temp/UR, aquecer/refrig.) | — |
| `P12` | `P12rE2` | Config Relé 2 | — |
| `P13` | `P13ALr` | Config alarme/buzzer (tipo + alta/baixa) | — |
| `P14` | `P14EuE` | Eventos horários (8 × fonte) | — |
| `P15` | `P15Esc` | Escala °C/°F e modo exibição | — |

> **ZUR/GUR** (RA-02): editáveis aqui **e** por serial (`z`/`y` Ummi Manager; aliases de texto para terminal).

---

## 9. Proteções industriais

- **IWDG** habilitado; refresh no laço principal (timeout ~1–2 s); se travar → reset.
- **BOR**/**PVD** habilitados; em PVD → estado seguro.
- **Estado seguro**: relés e buzzer **OFF** no boot, em falha de sensor/alimentação e em fault handlers.
- **I²C robusto**: timeout, NACK→STOP, recuperação de bus travado (reset do periférico).
- **USART robusto**: tratamento PE/FE/NE/ORE; FIFO circular; ressincronização do frame.
- **Config validada**: clamp de faixa + default seguro para qualquer valor inválido; CRC + (futuro) duplo-banco.
- **Datalogger**: CRC por registro + backup do cabeçalho; nunca bloquear o laço por longos períodos.
- **Anti-chattering**: tempo mínimo de comutação dos relés + proteção HN=HP=0 (não desliga).

---

## 10. Plano de realização (checkpoints)

> Cada checkpoint termina com **build limpo no STM32CubeIDE** e teste de bancada. Como não há
> compilador neste ambiente, a verificação de build/HW é responsabilidade de cada checkpoint.

| CP | Entrega | Critério de aceitação |
|---|---|---|
| **CP0** | Documentos (`analise.md`, este) + proteções de base (IWDG/BOR/estado seguro) | Documentos revisados; saídas em estado seguro no boot |
| **CP1** | Sensores (ADC temp/UR/5V/12V) + filtros 6 amostras + anti-degrau + desconexão | Leituras estáveis; `---` ao desconectar; modo teste mostra 5V/12V |
| **CP2** | RTC interno (ajuste serial/teclas) + display modos Termômetro/Relógio + alternância | Hora correta após ajuste; alternância funciona |
| **CP3** | Datalogger 24AA512 (7 B, ISC/IMC/IHC, circular, CRC+backup) + reset menu/serial | Amostragem em frações exatas; recuperação por CRC |
| **CP4** | Controle 2 relés (temp/UR, aquecer/refrig./umidif/desumid) + 8 eventos + OR + buzzer 4 padrões | Comutações nos pontos SP±H; eventos horários; padrões corretos |
| **CP5** | Protocolo Ummi Manager completo (`*`, ~36 cmds, ACK `F`, download `#`/`FIM`) | Ummi Manager comunica (validar layout — LAC-02) |
| **CP6** | Máx/Mín + menu completo (ZUR/GUR/relés/eventos/escala) + persistência total | Config sobrevive a power-cycle; máx/mín corretos |
| **CP7** | Endurecimento industrial (I²C/USART recovery, validações, testes de falha) + doc de testes | Recuperação de falhas; watchdog comprovado |

### Estrutura de código proposta (módulos)

```
Core/Src
├── main.c            (orquestração, init, laço principal, estado seguro, IWDG)
├── sensors.c         (ADC, filtros, conversões, desconexão)          + Inc/sensors.h
├── rtc_app.c         (RTC interno, BCD/bin, ajuste, data/hora compacta) + Inc/rtc_app.h
├── datalog.c         (24AA512: 7 B, ISC/IMC/IHC, circular, CRC+backup, download) + Inc/datalog.h
├── control.c         (relés temp/UR, 8 eventos horários, OR, buzzer 4 padrões) + Inc/control.h
├── ui.c              (display modos, menu inversor, máx/mín, teste 5V/12V) + Inc/ui.h
├── protocol.c        (Ummi Manager: '*', NBCR, CRC XOR, comandos, download) + Inc/protocol.h
├── config.c          (mapa de config persistente, validação, defaults)   + Inc/config.h
├── display.c/timer.c (reuso da infra existente)
└── eeprom.c          (config em Flash — reuso/extensão)
```

---

## 11. Itens a confirmar antes da validação final

1. **VBAT/bateria do RTC** no HW novo (LAC-01).
2. **Layout binário exato** dos frames do Ummi Manager para compat. byte-a-byte (LAC-02).
3. **Baud** efetivo do enlace serial/HM-10 (LAC-04).
4. Necessidade de **temperatura negativa** (LAC-06).
5. Significado dos **LEDs de status** e **ENTRADA_01/02** (LAC-05).

---

*Especificação consolidada da aplicação Ummi Control para STM32F410RB. Ver `analise.md` para
divergências de migração e `REQUISITOS_STM32_Ummi_Control.md` para a fonte de requisitos.*
