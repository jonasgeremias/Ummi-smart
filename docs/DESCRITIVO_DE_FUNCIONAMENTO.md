# Descritivo de Funcionamento — Ummi Control (BE1 1.0)

Firmware do termo-higrômetro/controlador **Ummi Control** sobre **STM32F410RB**.
Reescrita em C/HAL do firmware PIC16F877A original (`011000.asm`), mantendo o
protocolo serial e o conceito de operação, com migrações de hardware (RTC e
EEPROM) e os ajustes de produto solicitados (temperatura inteira, marca BE1).

> Versão de firmware: **1.0** (defines `FW_VERSAO_MAJOR/MINOR` em `config.h`).
> Build validado com o GCC do STM32CubeIDE: `Ummi-smart.elf` (≈39,5 KB flash,
> 2,4 KB RAM), sem erros nem *warnings* (`-Wall`).

---

## 1. Visão geral

O equipamento mede **temperatura** (LM35) e **umidade relativa** (Honeywell HIH),
exibe em um display de **6 dígitos 7-segmentos multiplexados** (3 + 3), aciona
**2 relés** e um **buzzer** por setpoint/histerese, por umidade e por eventos
horários, registra amostras em um **datalogger** em EEPROM I²C e conversa com o
software "Ummi Manager" por **serial 9600 8N1**.

### Periféricos e mapeamento

| Função | Recurso STM32 | Observação |
|---|---|---|
| Temperatura | ADC1 IN2 (PA2) − IN3 (PA3) | LM35 diferencial (10 mV/°C) |
| Umidade | ADC1 IN1 (PA1) | HIH, compensado por temperatura |
| Tensões 5 V / 12 V | ADC1 IN4 (PA4) / IN10 (PC0) | só no modo teste |
| Display segmentos | PA5–7, PB0–2 | A..G + ponto |
| Display dígitos (mux) | PC6, PB15/14/13/12/10 | 6 comuns |
| Teclas | PC1 (T/R), PC2 (Min), PC3 (Hora) | pull-up, ativo em nível baixo |
| Relé 1 / Relé 2 | PC7 / PC9 | saídas digitais |
| Buzzer | PB? (`SD_BUZZER`) | — |
| LEDs status | PC (LED1..4) | R1, R2, alarme, falha |
| EEPROM datalogger | I²C1 (PB6 SCL / PB7 SDA) @0x50 | 24AA512 ou 24LC256 |
| RTC | RTC interno + LSE 32,768 kHz | substitui o DS1307 |
| Serial Ummi Manager | USART1 (PA9 TX / PA10 RX) | 9600 8N1 |
| Watchdog | IWDG (~2,5 s) | independente, por registradores |

### Base de tempo

`SysTick` a 1 ms chama `mainIsr()` ([timer.c](../Core/Src/timer.c)), que gera as
bases de 1/3/5/10/50/100/500/1000 ms. Tarefas:

- **1 ms:** `serial_tick_1ms()` (timeouts da serial) + `display_scan()` (1 dígito por varredura → ~166 Hz/dígito).
- **3 ms:** debounce das teclas.
- **10 ms → /2 = 20 ms:** `sensors_update()` (1 amostra por canal).
- **50 ms:** `ui_tick_50ms()` + `control_tick_50ms()` (relés/alarmes).
- **1000 ms:** `datalog_tick_1s()`.
- **Laço principal:** `protocol_process()` + `ui_tick()` (render), com `iwdg_refresh()` a cada volta.

---

## 2. Inicialização

1. `HAL_Init`, clock (HSI+PLL, 96 MHz), GPIO; **saídas em estado seguro** (relés/buzzer desligados) logo após o GPIO.
2. I²C, RTC, USART; depois `sensors/config/rtc_app/datalog/control/protocol/ui`.
3. **Splash "BE1 1.0"** seguido de data e hora (3 × 1,2 s, bloqueante e curto).
4. **IWDG é ligado após o splash** (para o delay do splash não disparar o watchdog).

O RTC só é reinicializado se o domínio *backup* perdeu a hora (sem VBAT): assume
01/01/26 00:00. Com bateria, a hora é preservada entre power-cycles.

---

## 3. Aquisição de sinais ([sensors.c](../Core/Src/sensors.c))

- **ADC1 por registradores** (sem HAL ADC), leitura sequencial dos 5 canais, tempo de amostragem máximo (480 ciclos), Vref = 3,3 V, 12 bits.
- **Sensores LM35 e HIH5031 alimentados em 5 V**; as entradas de temperatura e umidade têm um **divisor 1k/1k8 para GND** que escala o sinal para o ADC de 3,3 V. O firmware **compensa o divisor** (`Vsensor = Vadc·2,8/1,8`, `DIVISOR_NUM/DEN`) antes de converter — sem isso, a temperatura ficava ~36% baixa e a UR/limiares ficavam errados.
- **Filtro de média móvel de 16 amostras** por canal (`MEDIA_N`, ~320 ms a 20 ms/amostra) + **filtro anti-degrau** (salto ≥ 0,5 °C aguarda 1 ciclo). O **número no display é re-amostrado só a cada ~500 ms** (rate-limit, para não "piscar"); os relés/controle usam o valor rápido.
- **Temperatura:** diferencial `IN2 − IN3` → mV → °C (10 mV/°C). Internamente em décimos de grau (precisão do filtro); exposta em **graus inteiros** por `sensors_temp_C()` / `sensors_temp_F()`.
- **Umidade (HIH5031 @5V):** `GUR/ZUR` calibrados direto na tensão **depois do divisor** (o que o ADC vê: ~0,49 V em 0% … ~2,53 V em 100%) — `dUR = (mV_ADC·GUR − ZUR)/100`, defaults `GUR=49`, `ZUR=23863` (ajuste fino no menu P28/P29). Calibrar absorve a tolerância real dos resistores; há ainda **compensação por temperatura** e saturação 0–100%.
- **Detecção de desconexão (trilho do ADC + 5 leituras):** ignora a conversão e considera o sensor "aberto" quando o **ADC bruto (já em média) encosta num trilho** — `0..10 LSB` (curto/p/ GND) ou `(fundo−10)..fundo` (entrada puxada p/ VCC). O estado só muda após **5 leituras seguidas** do mesmo lado (anti-ruído), eliminando o "piscar". Um sensor conectado nunca fica no trilho → leitura normal. Exibe `--`, não controla, não registra. Constantes `DESC_RAIL_LO_AD/HI_AD`, `DESC_CONSEC`.
- **Falha de ADC:** timeout de conversão sinaliza `sensors_adc_erro()`.

---

## 4. Temperatura inteira (000–999) — ajuste de produto

A pedido, **a temperatura passou a ser tratada em graus inteiros (000 a 999),
sem casa decimal**, em °C ou °F. Isso vale para **exibição, edição no menu,
controle, alarmes, datalogger e serial**. O filtro interno continua em décimos
para não perder resolução; o arredondamento para inteiro é feito apenas na saída
(`sensors_temp_C/F`).

**A umidade é tratada em valor inteiro (0–100%) de ponta a ponta** — exibição,
menu, controle, alarmes, datalogger e serial — sem o "99.9": 100% mostra "100".
O filtro interno mantém décimos (`dUR`) só para qualidade; o arredondamento para
% inteiro é feito na saída (`sensors_umidade_pct()`). O sensor de umidade abaixo
de **0,3 V** continua sendo identificado como **desconectado** (exibe `--`, não
controla nem registra).

> ⚠️ Desvio consciente do RF-01/RF-02 (original: 1 casa decimal e UR ×10) e da
> codificação serial do PIC. Temperatura **e** umidade agora trafegam em valor
> inteiro também na serial. Ver §11 (compatibilidade Ummi Manager).

---

## 5. Display e interface ([display.c](../Core/Src/display.c) / [ui.c](../Core/Src/ui.c))

### Modos
- **Termômetro:** trio esquerdo = **umidade** (% inteiro); trio direito = **temperatura** (grau inteiro). `--` por sensor desconectado.
- **Relógio:** `HH.MM.SS` (apenas por troca **manual** com T/R — não há mais alternância automática).
- **Teste:** entra com **5× tecla Hora**; **tecla Minuto avança a página**: (0) tensões 12V/5V → (1) `u`+ADC bruto umidade → (2) `t`+ADC bruto temperatura → (3) `r`+ADC bruto referência. Valores em contagem do ADC (0–4095) para diagnóstico. Sai por T/R ou após ~30 s.
- **Menu de configuração:** entra segurando **T/R por ~3 s**.

### Teclas
- **T/R:** toque curto alterna Termômetro/Relógio; segurar ~3 s entra/sai do menu.
- **Hora (+) / Minuto (−):** navegação e ajuste; momentâneos mostram Máx-Temp / Data.
- **Hora + Minuto (combo):** zera Máx/Mín.

### Menu de configuração (P01–P38)
O menu cobre **todo o pacote de configuração** que antes só existia na serial,
navegável com as 3 teclas (Hora=próximo, Minuto=anterior, T/R=entra/confirma,
segurar T/R=sai). Cada item mostra `Pnn` + um mnemônico de 3 letras.

| Faixa | Itens | Conteúdo |
|---|---|---|
| P01–P05 | Relé 1 / Temperatura | habilita, lógica (Aq/rE), setpoint, HN, HP |
| P06–P10 | Relé 1 / Umidade | habilita, lógica, setpoint, HN, HP |
| P11–P15 | Relé 2 / Temperatura | idem |
| P16–P20 | Relé 2 / Umidade | idem |
| P21–P23 | Alarme temperatura | modo (oF/Alt/bAI/Anb), limite alto, limite baixo |
| P24–P26 | Alarme umidade | modo, limite alto, limite baixo |
| P27 | Buzzer | tipo de toque (Con/LEn/nED/rAP) |
| P28–P29 | Calibração UR | zero (ZUR), ganho (GUR) |
| P30–P33 | Datalogger | habilita, base (SEG/nIn/hor), intervalo, reset |
| P34 | Restaurar padrões | `dEF` — volta toda a config aos defaults de fábrica (limpa GUR/ZUR, setpoints, etc.) |
| P35 | Exibição | escala °C/°F |
| P36 | EEPROM | tipo (256 = 24LC256 / 512 = 24AA512) |
| P37–P38 | RTC | data, hora |

- **Segurança:** ao entrar no menu, **relés e buzzer são desligados** e ficam inibidos até a saída.
- **Itens numéricos** (setpoints, histereses, limites, intervalo): ajuste com
  auto-repetição acelerada ("turbo"). **Temperatura e umidade em valor inteiro.**
- **Itens enumerados** (habilita, lógica, modo, buzzer, base, escala, EEPROM):
  avançam/retrocedem com retorno (wrap) a cada toque.
- **Gravação adiada** ao confirmar o item; trocar o tipo de EEPROM reinicializa o
  datalogger.
- **Ainda só por serial:** os **eventos horários** (8 por fonte × 3 fontes) — são
  configuráveis pelo Ummi Manager, mas não foram colocados no menu (a edição de
  24 janelas H:M com 3 teclas fica para uma etapa seguinte).

### Máx/Mín (RF-16)
Registrados quando o sensor está estável e conectado; exibição momentânea;
zeráveis pelo combo de teclas.

---

## 6. Controle de relés e buzzer ([control.c](../Core/Src/control.c))

Executado a cada **50 ms**:

- **Histerese de 2 pontos** por grandeza: liga em `SP−HN`, desliga em `SP+HP`; lógica **Aquecer/Refrigerar** (ou Umidificar/Desumidificar). `HN=HP=0` ⇒ trava ligado (nunca desliga).
- **Eventos horários:** 8 por fonte, com suporte a **cruzar a meia-noite**.
- **Combinação OR** (RF-14): a saída liga se evento **ou** fonte-temperatura **ou** fonte-umidade pedir.
- **Anti-chattering:** mínimo de 5 s entre comutações do mesmo relé.
- **Falha de alimentação** (5 V/12 V fora de faixa) força os dois relés **desligados** imediatamente (segurança).
- **Estado seguro no menu:** ao entrar no **menu de configuração**, todas as saídas (relés e buzzer) são **forçadas desligadas** (`control_inibe`) e só voltam a operar ao sair do menu.
- **Corte por alarme de temperatura alta:** com o alarme de temp alta ativo, qualquer relé em modo **AQUECER** por temperatura é **inibido** (corte independente da histerese, mesmo no modo trava) — rede de segurança contra sobreaquecimento.
- **Histerese mínima 1** pelo menu (HN/HP ≥ 1) para evitar o modo "trava ligado" (HN=HP=0) por engano; o modo trava continua acessível deliberadamente pela serial.
- **Falhas** (`falha_t`, `falha_ur`, `falha_alim`) inibem o controle da grandeza afetada.
- **Buzzer:** dispara por temp alta/baixa, UR alta/baixa, qualquer falha e eventos horários de alarme; 4 padrões (Contínuo/Lento/Médio/Rápido). Alarme de **temperatura baixa** só após **5 min** contínuos abaixo do limite (estabilização).
- **LEDs:** R1, R2, alarme e falha.

---

## 7. Datalogger ([datalog.c](../Core/Src/datalog.c))

- **EEPROM externa I²C selecionável** (ver §9): cabeçalho com assinatura + CRC + **cópia de backup**, memória **circular**, registro de **9 bytes** (= bloco de download).
- Registro: `Dia, Mês, Ano, Hora(b7=°C), Min(b7=T negativa), TempMSB, TempLSB, UR%, CRC(XOR)`. **Temperatura em grau inteiro.**
- **Intervalos:** segundos / minutos / horas, com chave anti-duplicidade (não grava duas vezes na mesma marca).
- Não registra com sensor desconectado ou instável.
- **Recuperação:** na inicialização valida o cabeçalho primário → backup → reset total se ambos falham (RNF-03).
- **Escrita não bloqueante por página** (page-write + polling de pronto), em vez do `delay` de 20 ms/byte do original (RNF-06).
- **Recuperação de barramento I²C** travado: `DeInit/Init` do periférico (RNF-04).

---

## 8. Persistência da configuração ([config.c](../Core/Src/config.c))

- Toda a configuração (`g_config`) é gravada na **Flash interna** (setor 4, 64 KB), com **assinatura + CRC** e gravação **log-structured** (append) para distribuir desgaste; apaga o setor só quando enche.
- Na carga: lê o registro mais recente válido; se inválido, aplica **defaults de fábrica** e valida/satura todos os campos.
- **Gravação adiada (debounce ~800 ms):** os comandos seriais de escrita marcam a config como "suja" e dão **ACK imediato**; a Flash só é gravada quando a rajada de comandos cessa — evita erase/program a cada byte e reduz desgaste/bloqueio.
- **Watchdog protegido no erase:** antes do erase (que bloqueia ~1–2 s) chama-se `iwdg_refresh()` para não resetar no meio da gravação.

> A assinatura é versionada (atual **`UMC3`**): ao atualizar o firmware com
> mudança de layout/semântica (ex.: deci→inteiro de temperatura e UR), uma config
> antiga é descartada e os defaults assumem. O setor se auto-recupera (apaga
> quando necessário).

---

## 9. EEPROM do datalogger — parâmetro configurável

O hardware atual tem a **24AA512 (64 KB, página 128 B)** soldada, mas o projeto
legado usava a **24LC256 (32 KB, página 64 B)**. O tipo é um **parâmetro
configurável**:

- **Default de fábrica:** `EEPROM_TIPO_DEFAULT = EEPROM_24AA512` em [config.h](../Core/Inc/config.h) (basta trocar esse define para mudar o padrão de uma variante de placa).
- **Persistido** em `g_config.eeprom_tipo` (sobrevive a power-cycle).
- **Ajustável em runtime** por dois caminhos, ambos reinicializando o datalogger:
  - **Menu P36** (`EEP`): `256` = 24LC256 / `512` = 24AA512.
  - **Serial:** comando **`k`** + 1 byte (`0`=24LC256, `1`=24AA512).

Tamanho e página são aplicados em runtime (`aplica_tipo_eeprom()`); a capacidade
calculada entra no CRC do cabeçalho, então trocar de chip força um *reset* limpo
do log.

---

## 10. Comunicação serial ([protocol.c](../Core/Src/protocol.c))

- **USART1 9600 8N1** assíncrona (reconfigurada por registradores sobre o init do CubeMX), RX por **interrupção + FIFO**, com **tratamento de overrun/framing/parity** (RNF-05).
- **Frame:** `'*'` + comando + dados + **CRC XOR** (XOR de todo o frame = 0). Tamanho do frame (NBCR) determinado pela faixa ASCII do comando. Timeout de sincronismo de ~0,5 s.
- **ACK de escrita** = `'F'`. Comando desconhecido é ignorado.
- **Leituras:** versão (`V`→1.0), temperatura (`X`), umidade (`U`), status de saídas (`R`), status completo (`T`), config (`W`).
- **Escritas:** escala °C/°F, exibição alternada (`F`/`G` — inerte: alternância automática removida), calibração UR (`z`/`y`), RTC (`d/e/a/h/i`), datalogger on/off/reset/base+intervalo, alarmes temp/UR, relés por temp/UR, eventos horários, tipo de toque, **tipo de EEPROM (`k`)**.
- **Download do logger:** comando `C` inicia; handshake `'#'` envia o próximo bloco de 9 bytes; termina com `"FIM"`+CRC (RF-10).

---

## 11. Validação contra os requisitos

| ID | Requisito | Situação | Observação |
|---|---|---|---|
| RF-01 | Temperatura LM35 °C/°F | ⚠️ **Alterado** | **inteiro 000–999** por decisão de produto (era 1 casa decimal) |
| RF-02 | UR com compensação, 0–100% | ⚠️ **Alterado** | satura 100%; **% inteiro de ponta a ponta** (era ×10) |
| RF-03 | Damping 6 + anti-degrau 5 | ✅ | `MEDIA_N=6`, salto ≥0,5 °C |
| RF-04 | Sensor desconectado → `--` | ✅ | limiares de mV/LSB |
| RF-05 | Display 6 díg., modos, ponto no relógio | ✅ | multiplexação 1 díg./ms |
| RF-06 | RTC 24h, ajuste serial/teclas | ✅ **migrado** | **RTC interno + LSE** (não DS1307) |
| RF-07 | 3 teclas com debounce | ⚠️ | debounce ~3 ms (ver §12); combos e hold OK |
| RF-08 | Logger circular + CRC + backup | ✅ | registro de **9 bytes** (= bloco de download) |
| RF-09 | Serial 9600 8N1, CRC XOR, ACK `F` | ✅/⚠️ | protocolo implementado; **validar layout vs Ummi Manager** |
| RF-10 | Download `#`/`FIM`, blocos 9 B | ✅ | — |
| RF-11 | Relé por temperatura (SP±H) | ✅ | comuta em `SP−HN`/`SP+HP` |
| RF-12 | Relé por umidade | ✅ | mesma histerese |
| RF-13 | 8 eventos horários por fonte | ✅ | inclusive cruzando meia-noite |
| RF-14 | OR das fontes | ✅ | — |
| RF-15 | Buzzer 4 padrões | ✅ | Contínuo/Lento/Médio/Rápido |
| RF-16 | Máx/Mín zerável por combo | ✅ | — |
| RF-17 | Persistir config não volátil | ✅ **migrado** | **Flash interna** (não EEPROM do PIC) |
| RNF-01 | Watchdog alimentado | ✅ | IWDG ~2,5 s |
| RNF-02 | Brown-out reset | ⚠️ | configurar via **option bytes (BOR)** — fora do código |
| RNF-03 | Recuperação do logger (CRC+backup) | ✅ | — |
| RNF-04 | I²C NACK/recuperação | ✅ | `i2c_recupera()` |
| RNF-05 | Overrun/framing USART | ✅ | tratado na ISR |
| RNF-06 | Logger não bloqueia o laço | ✅ | page-write + polling |
| RNF-07 | Compat. total Ummi Manager | ⚠️ | ver §11 abaixo |
| RNF-08 | Proteção de leitura (RDP) | ⚠️ | via option bytes, se requisito comercial |

### Compatibilidade com o Ummi Manager (RNF-07)
A migração para **valores inteiros** muda a semântica dos campos de **temperatura
(antes ×10) e umidade (antes ×10 / `dUR`)** na serial: setpoints, histereses,
limites de alarme e leituras agora trafegam em **inteiro** (grau ou % conforme a
grandeza). Além disso foi adicionado o comando **`k`** (tipo de EEPROM),
inexistente no original. **É necessário validar/atualizar o Ummi Manager** para o
novo layout (já sinalizado como LAC-02 em `protocol.c`).

---

## 12. Bugs corrigidos e pontos em aberto

### Corrigidos nesta entrega
1. **EEPROM com tamanho/página errados** (assumia 24AA512 fixo) → parametrizado (24AA512 default / 24LC256), corrigindo *page-write* e cálculo de capacidade.
2. **Watchdog podia resetar no meio do erase** da Flash → `iwdg_refresh()` antes do erase + **gravação adiada** (não salva a cada comando).
3. **Saídas ficavam no último estado em caso de fault** → HardFault/MemManage/BusFault/UsageFault/NMI agora **desligam relés/buzzer**.
4. **`serial_tick_1ms` / IRQ chamados sem protótipo** → `#include "protocol.h"` em `timer.c` e `stm32f4xx_it.c`.
5. **Ajuste de minuto zerava os segundos** → preserva os segundos correntes.
6. **Watchdog por registradores espalhado** → isolado em `main.c` (dono do hardware); demais módulos chamam `iwdg_refresh()` (sem tocar registradores).
7. **Risco térmico (relé "trava ligado")** → menu agora exige histerese ≥ 1; **alarme de temperatura alta corta o aquecimento**.
8. **RTC dava ACK em valor inválido** → `'F'` só após gravação bem-sucedida.
9. **Config antiga na Flash após mudança de layout** → defaults são persistidos no boot quando não há registro válido, superando registros antigos.

### Em aberto / a confirmar
- **Serial sem autenticação** (inerente ao protocolo legado): quem tem acesso à linha pode alterar toda a config e baixar o log — aceitável em RS-232 de painel, registrado como decisão consciente.
- **`'F'` é byte de ACK e também comando** (exibição alternada): herança do protocolo original; mantido por compatibilidade com o Ummi Manager (só relevante em montagem com eco/loopback).
- **Eventos horários (8 × 3 fontes) só por serial** — ainda não estão no menu (edição de 24 janelas H:M com 3 teclas fica para uma próxima etapa). Todo o restante da config já está no menu (P01–P38).
- **Debounce das teclas ~3 ms** (1 amostra a cada 3 ms). Para membranas costuma bastar, mas o RF-07 sugere ~60 ms; recomenda-se aumentar `BTN_DEBOUNCE_TICKS` se houver repique.
- **Brown-out (BOR) e RDP**: configurar por *option bytes* (não há API no código).
- **Layout serial vs Ummi Manager** (RNF-07/LAC-02): validar byte-a-byte.
- **Detecção de sensor aberto:** depende de a entrada aberta encostar num trilho do ADC (0 ou VCC). Caso, em alguma placa, um pino solto estabilize **no meio da escala**, ajustar os limiares `DESC_RAIL_*_AD` ou prever um resistor de polarização (pull-up/down) que force o pino aberto para um trilho.
- **Calibração UR (ZUR/GUR):** defaults calculados para **HIH5031 @5V com divisor 1k/1k8** (`GUR=31`, `ZUR=22800`, ~±0,7% de quantização da inclinação). Verificar com higrômetro de referência e ajustar fino no menu (P28/P29). O limite superior de "aberto" (`UMIDADE_DESC_ALTA_mV = 4100 mV`, lado sensor) também pode ser afinado.
- **Faixa de temperatura**: o display mostra 000–999; o sensor LM35 a 3,3 V cobre até ~330 °C. Acima de 999 satura na exibição.

---

## 13. Arquivos principais

| Módulo | Arquivo | Responsabilidade |
|---|---|---|
| Orquestração | [main.c](../Core/Src/main.c) | init, laço, watchdog, estado seguro |
| Base de tempo/teclas | [timer.c](../Core/Src/timer.c) | SysTick, flags, debounce |
| Aquisição | [sensors.c](../Core/Src/sensors.c) | ADC, filtros, °C/°F inteiro |
| Controle | [control.c](../Core/Src/control.c) | relés, buzzer, alarmes, eventos |
| UI | [ui.c](../Core/Src/ui.c) | display, menu, máx/mín, teste |
| Display | [display.c](../Core/Src/display.c) | multiplexação 7-seg |
| Datalogger | [datalog.c](../Core/Src/datalog.c) | EEPROM I²C, registros, download |
| Config | [config.c](../Core/Src/config.c) | persistência em Flash, defaults |
| RTC | [rtc_app.c](../Core/Src/rtc_app.c) | RTC interno, data/hora |
| Serial | [protocol.c](../Core/Src/protocol.c) | protocolo Ummi Manager |
