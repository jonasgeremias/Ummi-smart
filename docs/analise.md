# Relatório de Análise — Migração para "Ummi Control" sobre o hardware novo

> **Objetivo do documento:** comparar os requisitos de `REQUISITOS_STM32_Ummi_Control.md`
> (engenharia reversa do firmware PIC `011000.asm` — termo-higrômetro *Ummi Control*) com o
> **hardware novo** e com o **código STM32 atual** (`UMMI SECAGEM FUMO`), apontando divergências,
> problemas de migração e decisões adotadas.
>
> **Decisões do produto (confirmadas pelo cliente):**
> 1. O alvo é **reescrever para o termo-higrômetro Ummi Control** (REQUISITOS é o produto-alvo).
> 2. Protocolo serial: **reintroduzir o protocolo "Ummi Manager" (`*`, CRC XOR)**.
> 3. **Hardware novo:** RTC **interno** (não DS1307) e EEPROM **24AA512** (64 KB).
> 4. Manter **modo teste** com leitura de **5 V e 12 V**.
> 5. `GUR`/`ZUR` (e demais parâmetros) configuráveis por **tela** e por **serial**.
> 6. Código com **proteções industriais**.

---

## 1. Plataforma e ambiente

| Item | Original (PIC) | HW/SW atual STM32 | Decisão |
|---|---|---|---|
| MCU | PIC16F877A @4 MHz | **STM32F410RBT** (128 KB flash / 32 KB RAM), HSI+PLL → 96 MHz | Mantido STM32F410RB |
| Base de tempo | Timer1 ≈504 Hz | **SysTick 1 ms** (`mainIsr` em `stm32f4xx_it.c:191`) + multiplex de display em 1 ms | Mantido; contadores reexpressos em ms |
| RTC | DS1307 externo (I²C, BCD) | **RTC interno** com LSE 32.768 kHz (`.ioc`: `RCC_RTCCLKSOURCE_LSE`) | **RTC interno** (HW novo) |
| EEPROM datalogger | 24LC256 (32 KB) | Código atual dimensionado para 32 KB | **24AA512 (64 KB)** — exige re-dimensionar |
| EEPROM de config | EEPROM interna do PIC (0x00–0xCD) | **Emulação em Flash** (setor 4, `eeprom.c`) | Mantida emulação em Flash |
| Serial | USART 9600 8N1, protocolo `*` | USART1 PA9/PA10, hoje 115200 STX/CRC16 | USART1 + **protocolo `*`**; baud configurável (ver §6) |
| Toolchain | MPASM | STM32CubeIDE (GCC arm-none-eabi) | **Sem compilador no ambiente atual** → build/flash no CubeIDE |

> **[NOTA DE AMBIENTE]** Não há `arm-none-eabi-gcc`/`make` neste ambiente. A compilação e o flash
> devem ser feitos no STM32CubeIDE. Todo o código entregue foi revisado estaticamente, mas a
> validação de build e de bancada deve seguir os checkpoints de `aplicacao_ummi_control.md`.

---

## 2. Mapa de pinos — Original vs Hardware novo

O pinout do PIC **não tem correspondência 1:1** com o STM32. O termo-higrômetro foi remapeado sobre a
pinagem existente (definida em `Core/Inc/main.h` e `Core/Inc/defines.h`).

| Função (Ummi Control) | PIC original | STM32 novo (sinal lógico) | Pino | Observação |
|---|---|---|---|---|
| ADC Temperatura | AN0 (LM35 single-ended) | `EA_TEMPERATURA` | PA2 (ADC_IN2) | **HW novo é diferencial por software** (ver §3) |
| ADC referência GND | AN3 (diferencial, **comentado**) | `EA_TEMP_REF_GND` | PA3 (ADC_IN3) | **No HW novo o caminho diferencial está ATIVO** |
| ADC Umidade | AN1 (HIH) | `EA_UMIDADE` | PA1 (ADC_IN1) | HIH3051 |
| Monitor 5 V | — (não existe) | `EA_5V` | PA4 (ADC_IN4) | **Novo — usado no modo teste** |
| Monitor 12 V | — (não existe) | `EA_12V` | PC0 (ADC_IN10) | **Novo — usado no modo teste** |
| Relé 1 | RA2 (ativo-alto) | `SAIDA_01` | PC7 | ativo-alto |
| Relé 2 | RA5 (ativo-alto) | `SAIDA_02` | PC9 | ativo-alto |
| Buzzer | RB7 (ativo-alto) | `SD_BUZZER` | PB5 | ativo-alto |
| Tecla T/R | RB3 (ativo-baixo) | `BOTAO_RELOGIO` | PC1 | pull-up interno |
| Tecla HORA | RB5 | `BOTAO_MAX` | PC3 | pull-up interno |
| Tecla MINUTO | RB4 | `BOTAO_MIN` | PC2 | pull-up interno |
| Display segmentos a–g+DP | RD0–RD7 (barramento) | `SEG_A..G`, `SEG_DP` | PA5/6/7, PC4/5, PB0/1/2 | mesma ideia, pinos espalhados |
| Mux 6 dígitos | RC0-2 (verm.) + RE0-2 (verde) | `DISPLAY_*` (6 comuns) | PC6, PB15/14/13/12/10 | **6 dígitos únicos** (não 3+3 fisicamente separados) |
| Comunicação I²C | RC3/RC4 (MSSP) | `I2C1` SCL/SDA | PB6/PB7 | 24AA512 |
| USART TX/RX | RC6/RC7 | `USART1` TX/RX | PA9/PA10 | — |
| Entradas digitais extra | — | `ENTRADA_01/02` | PB8/PB9 | reservadas |
| LEDs de status | — | `LED_STATUS1..4` | PC10/11/12/13 | **Novos — usar para status (R/U/relés)** |

### Divergências de pinout (DPN)

| ID | Divergência | Impacto | Resolução adotada |
|---|---|---|---|
| DPN-01 | Display original é **3 vermelhos + 3 verdes** (cores separadas); o HW novo tem **6 dígitos de cor única** multiplexados | A separação "temperatura em vermelho / UR em verde" vira "trio à direita / trio à esquerda" | Tela Termômetro mostra `T T T  U U U` nos 6 dígitos (3 esq. = temperatura, 3 dir. = UR), conforme `atualiza_display_principal()` já faz |
| DPN-02 | Temperatura negativa: no PIC o caminho diferencial AN0–AN3 estava **comentado** (só media positivos). No HW novo `EA_TEMP_REF_GND` (PA3) **existe e é usado** | O HW novo **pode** medir abaixar o offset, mas LM35 aterrado ainda não dá negativo real | Mantida medição diferencial por software; T<0 continua **não suportada** (LM35 unipolar) — documentado |
| DPN-03 | Sinais 5 V/12 V não existem no Ummi Control original | Funcionalidade **adicional** (modo teste) | Mantida como **modo teste/diagnóstico** (requisito do cliente) |
| DPN-04 | LEDs de status não existem no original | — | Reaproveitados para indicação de Relé1/Relé2/Buzzer/alarme |
| DPN-05 | 9 teclas no HW do PIC (3 usadas); HW novo tem **3 teclas** + 2 entradas digitais | As 3 teclas T/R, HORA, MINUTO mapeiam 1:1 | OK |

---

## 3. Subsistema analógico — divergências de conversão

| ID | Tema | Original (PIC, Vref=5 V, 10 bits) | HW/SW novo (Vref=3,3 V, 12 bits) | Resolução |
|---|---|---|---|---|
| DAN-01 | Resolução/Vref do ADC | 10 bits, 5 V, LSB=4,88 mV | 12 bits, **3,3 V**, LSB≈0,806 mV | Todas as constantes recalculadas para 12 bits/3,3 V |
| DAN-02 | LM35 | single-ended AN0, fator ×49/100 | **diferencial** `EA_TEMPERATURA − EA_TEMP_REF_GND`, 10 mV/°C | Conversão em mV → décimos de °C (10 mV/°C ⇒ 1 °C = 10 mV) |
| DAN-03 | Umidade HIH | HIH genérico, `Z_UR=8000`, `G_UR=310` (escala 5 V) | **HIH3051**: 0,7575 V=0% e 3,9375 V=100%; `ZUR=6950`, `GUR=314` | Mantidos `ZUR`/`GUR` configuráveis (tela + serial); satura 0–100% |
| DAN-04 | Filtro damping | média deslizante de **6 amostras** | média circular de **8 amostras** (`>>3`) já no código | **Divergência de janela** — ver decisão DAN-06 |
| DAN-05 | Filtro anti-degrau "Anidro" | salto ≥5 unidades aguarda 1 ciclo e reinit | **ausente** no código atual | **Implementar** o anti-degrau (RF-03) |
| DAN-06 | Janela de média | 6 (PIC) vs 8 (STM) | — | Adotado **6 amostras** para fidelidade ao RF-03; cadência de leitura mantida (20 ms/amostra) |
| DAN-07 | Sensor desconectado | ADC=0 → exibe `--` | umidade < 0,3 V e temp dif < 5 AD → desconectado | Mantida a heurística do HW novo (mais robusta) e exibe `---` |

---

## 4. RTC — DS1307 → RTC interno

| Tema | DS1307 (original) | RTC interno STM32 | Impacto / resolução |
|---|---|---|---|
| Formato | BCD em registradores I²C | BCD nativo (HAL converte BIN↔BCD) | Rotinas BCD↔bin do PIC **não são necessárias**; usar `HAL_RTC_Get/SetTime/Date` |
| Bit CH / 24h | força CH=0 e modo 24h | já 24h (`RTC_HOURFORMAT_24`) | OK |
| Backup de hora | bateria no DS1307 | **VBAT + LSE** (domínio backup) | **Requer pino VBAT com bateria/supercap** no HW novo — confirmar no esquemático (LAC-01) |
| Perda de hora | flag CH | flag `RTC_FLAG_INITS` / backup register | Detectar "RTC não inicializado" e exibir hora inválida até ajuste |
| Ajuste | comandos `d/e/a/h/i` (serial) + teclas | mesmos comandos mapeados + teclas | Mantido |

> **[LACUNA LAC-01]** O RTC interno só mantém a hora sem alimentação principal se houver **VBAT**
> com bateria/supercapacitor e LSE no domínio backup. Confirmar no esquemático do HW novo.

---

## 5. Datalogger — 24LC256 → 24AA512

| Item | Original / código atual | 24AA512 (HW novo) | Resolução |
|---|---|---|---|
| Capacidade | 32 KB (`EXT_EEPROM_SIZE_BYTES=32768`, `main.c:102`) | **64 KB (65536 bytes)** | Atualizar para 65536 |
| Tamanho de página | 64 B (`EXT_EEPROM_PAGE_SIZE`, `main.c:106`) | **128 B** no 24AA512 | Atualizar para 128 |
| Endereçamento I²C | 16 bits (`I2C_MEMADD_SIZE_16BIT`) | 16 bits (0x0000–0xFFFF) | OK |
| Endereço de device | `0x50<<1` | 0xA0/0xA1 (A0=A1=A2=0) | OK |
| Registro | atual 16 B estruturado; **original 7 B** | — | **Reintroduzir registro de 7 bytes** + bloco de download de 9 bytes (compat. Ummi Manager) |
| Integridade | CRC FNV + cabeçalho | original: ponteiro + CRC (MSB⊕LSB) + **backup** | Implementar **ponteiro com CRC + cópia de backup** (RNF-03) |
| Escrita | byte-a-byte com delay 20 ms (PIC, R03) | page-write já presente no `ext_eeprom_write` atual | Manter **page-write**; tornar não bloqueante quanto possível (RNF-06) |
| Intervalos | ISC/IMC/IHC (s/min/h, frações exatas) | apenas `periodo_s` no atual | **Implementar ISC/IMC/IHC** (RF-08) |

### Divergências do datalogger (DLG)

| ID | Divergência | Resolução |
|---|---|---|
| DLG-01 | Formato de registro 7 B (original) vs 16 B (atual) | Adotar **7 bytes** para compatibilidade do download Ummi Manager (`Dia,Mês,Ano,Hora[b7=°C/°F],Min[b7=sinal],Temp,UR`) |
| DLG-02 | Bloco de download de **9 bytes** com CRC XOR e handshake `#`/`FIM` | Implementar conforme §6 do REQUISITOS |
| DLG-03 | Memória circular com `FLAG_VIRADA` | Implementar; reset por `Z` (serial) e por menu |
| DLG-04 | Anti-duplicidade (`tempo ≠ EE_TIME_L_LOG`) e disparo em frações exatas | Implementar a regra `tempo mod intervalo == 0` |

---

## 6. Comunicação serial — Ummi Manager (`*`) vs STX/CRC16

> **Decisão do cliente:** reintroduzir o protocolo **Ummi Manager** (frames iniciados por `*`,
> NBCR por faixa ASCII do 2º byte, CRC = XOR de 8 bits, ACK `'F'`, handshake `'#'`, `"FIM"` no fim
> do download). O protocolo STX/CRC16 (HM-10) do código atual será **substituído**.

| Tema | Ummi Manager (alvo) | STX/CRC16 (atual) | Resolução |
|---|---|---|---|
| Início de frame | `*` (0x2A) | `STX` (0x02) | Adotar `*` |
| Tamanho do frame | por faixa do 2º byte (`load_nbcr`) | TAM hex explícito | Adotar tabela NBCR do REQUISITOS §6.2 |
| CRC | **XOR 8 bits** (inclui o `*`) | CRC-16/IBM | Adotar XOR 8 bits (R05: detecção fraca, aceita por compatibilidade) |
| ACK escrita | `'F'` (0x46) | `84,OK` | Adotar `'F'` |
| Download | blocos 9 B + `#` + `"FIM"` | pacotes `86` assíncronos | Adotar handshake `#`/`FIM` |
| Comandos | ~36 comandos (tabela §6) | ~10 comandos | Implementar os 36 comandos |
| Baud | **9600 8N1** (legado) | 115200 | **Configurável** — default 9600 para compat.; ver DPS-01 |

### Divergências do protocolo (DPS)

| ID | Divergência | Severidade | Resolução |
|---|---|---|---|
| DPS-01 | Baud 9600 (Ummi Manager) vs 115200 (HM-10) | Média | Constante `UMMI_BAUD` (default 9600). Se o HM-10 estiver fixo em 115200, configurar o módulo ou usar 9600 |
| DPS-02 | Sem tratamento de OERR/FERR no original (R01) | **Alta** | Tratar PE/FE/NE/ORE na ISR (já existe esqueleto em `serial_usart1_irq_handler`) e ressincronizar |
| DPS-03 | Tipo de toque do buzzer divergente (R02) | Média | Definir enum claro {Contínuo, Lento, Médio, Rápido} e validar despacho |
| DPS-04 | Layout binário exato dos frames de status/config | Info | O REQUISITOS resume; o **layout byte-a-byte definitivo** depende do `011000.asm`/Ummi Manager. Implementado conforme documentado e **sinalizado para validação com o PC** (LAC-02) |

> **[LACUNA LAC-02]** A compatibilidade byte-a-byte com o software "Ummi Manager" depende dos
> layouts exatos dos frames de resposta (status 24 B, 11 mensagens de config, blocos do logger).
> O REQUISITOS descreve as estruturas; a validação final exige teste contra o PC real.

---

## 7. Controle de saídas — divergências funcionais

| ID | Tema | Original (Ummi Control) | Código atual (secagem) | Resolução |
|---|---|---|---|---|
| DCT-01 | Nº de relés controlados | **2 relés** por temp **e** umidade | 1 soprador (SAIDA_01) + SAIDA_02 reserva | **Implementar os 2 relés** com lógica completa |
| DCT-02 | Lógica de histerese | 2 pontos: aquecer/refrigerar e umidif/desumid | só "aquecer" (liga abaixo de SP−H) | **Implementar aquecer/refrigerar + umidif/desumid** |
| DCT-03 | Eventos horários | **8 eventos × 3 fontes** (alarme, R1, R2) | ausente | **Implementar** (RF-13), inclusive janela cruzando meia-noite |
| DCT-04 | Combinação de fontes | `OR`(horário ∨ temp ∨ umidade) | só temp | **Implementar OR** (RF-14) |
| DCT-05 | Proteção HP=0 & HN=0 | relé **nunca desliga** (R07) | tempo mínimo de comutação | Replicar a proteção **e** manter tempo mínimo de comutação (industrial) |
| DCT-06 | Buzzer | 4 padrões por fonte (R02) | ON/OFF temporizado | **Implementar 4 padrões** |
| DCT-07 | Setpoints/histereses | armazenados ×10 (1 casa decimal) | em décimos (dC) | Compatível (décimos) |

---

## 8. Proteções industriais — estado atual e lacunas

| RNF | Requisito | Estado atual | Ação |
|---|---|---|---|
| RNF-01 | **Watchdog** alimentado | **ausente** | Habilitar **IWDG** e fazer refresh no laço principal |
| RNF-02 | **Brown-out reset** | **ausente** | Habilitar **BOR** (option bytes) e/ou **PVD** com tratamento |
| RNF-03 | Recuperação do ponteiro do logger (CRC+backup) | parcial (CRC sem backup) | Implementar **backup** completo |
| RNF-04 | Erro de barramento I²C (NACK→STOP) | `HAL_I2C` trata, mas sem recuperação de BUSY | Adicionar **recuperação de I²C travado** (reset do periférico/bus) |
| RNF-05 | Overrun/framing USART | **parcial** (`serial_usart1_irq_handler` limpa erros) | Manter e garantir ressincronização do frame `*` |
| RNF-06 | Logger não bloqueante (>X ms) | page-write bloqueante curto | Limitar fatia por iteração; nunca bloquear display/serial |
| RNF-08 | Proteção de leitura (RDP) | **ausente** | **Opcional** (requisito comercial) — documentar; não habilitar sem decisão |

Proteções adicionais recomendadas (boas práticas industriais), além do original:
- **Estado seguro na inicialização e em falha**: relés e buzzer desligados antes de qualquer lógica
  (já feito em `Error_Handler`; estender para reset/boot).
- **Validação de faixa** de todo parâmetro lido da config (clamp + default em valor inválido).
- **Tempo mínimo de comutação** dos relés (anti-chattering) — preservar do código atual.
- **`assert`/handlers de fault** que levam a estado seguro + reset por watchdog.

---

## 9. Matriz de rastreabilidade — Requisitos × Estado

| Req (REQUISITOS) | Descrição | Estado no código atual | Ação nesta entrega |
|---|---|---|---|
| RF-01 | Temp LM35 °C/°F | Parcial (°C dif, sem °F) | Adicionar °F e 1 casa decimal |
| RF-02 | UR HIH compensada | Parcial (sem compensação por temp) | Adicionar compensação por temperatura |
| RF-03 | Damping 6 + anti-degrau | Parcial (8, sem anti-degrau) | Ajustar para 6 + anti-degrau |
| RF-04 | Sensor desconectado `--` | OK | Manter (`---`) |
| RF-05 | Display 6 díg + modos Term/Relógio + alternado | Parcial (só termômetro) | Adicionar modo relógio + alternância |
| RF-06 | RTC ajuste serial/teclas | Parcial | Completar comandos `d/e/a/h/i` + teclas |
| RF-07 | Teclado 3 teclas + combos | OK (debounce) | Adicionar combos (zera máx/mín etc.) |
| RF-08 | Datalogger 7 B, ISC/IMC/IHC, circular, CRC+backup | Parcial (16 B, período único) | Reescrever conforme original |
| RF-09 | Serial protocolo `*` ~36 cmds | Não (STX/CRC16) | Reescrever protocolo |
| RF-10 | Download `#`/`FIM` 9 B | Não | Implementar |
| RF-11 | Relés por temperatura (aquecer/refrigerar) | Parcial (1 relé, só aquecer) | Implementar 2 relés + refrigerar |
| RF-12 | Relés por umidade | Não | Implementar |
| RF-13 | 8 eventos horários × 3 fontes | Não | Implementar |
| RF-14 | Combinação OR | Não | Implementar |
| RF-15 | Buzzer 4 padrões | Não (ON/OFF) | Implementar |
| RF-16 | Máx/Mín + zerar por combo | Não | Implementar |
| RF-17 | Persistência da config completa | Parcial (subconjunto) | Expandir mapa de config |
| RNF-01..08 | Não-funcionais | Ver §8 | Implementar IWDG/BOR/I²C/USART/backup |

---

## 10. Lacunas a confirmar (bloqueiam validação, não a implementação)

| ID | Lacuna | Como contornamos |
|---|---|---|
| LAC-01 | VBAT/bateria do RTC no HW novo | Implementado detectando RTC não inicializado; confirmar esquemático |
| LAC-02 | Layout binário exato dos frames Ummi Manager | Implementado conforme REQUISITOS; validar contra o PC |
| LAC-03 | Modelo exato do HIH (3051 confirmado em `aplicacao.md`) | Usadas as tensões 0,7575 V / 3,9375 V e `ZUR/GUR` |
| LAC-04 | Baud do HM-10/serial (9600 vs 115200) | Constante `UMMI_BAUD` (default 9600) |
| LAC-05 | Significado físico dos LEDs de status e ENTRADA_01/02 | LEDs usados para R1/R2/Buzzer/alarme; entradas reservadas |
| LAC-06 | Temperatura negativa | Não suportada (LM35 unipolar) — confirmar se há necessidade |

---

*Documento de análise de migração. Referências de linha do PIC remetem a `011000.asm`;
referências `arquivo:linha` remetem ao código STM32 atual.*
