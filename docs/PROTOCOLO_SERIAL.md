# Protocolo Serial — Ummi Control (BE1 1.0)

Protocolo do enlace serial com o software "Ummi Manager", implementado em
[protocol.c](../Core/Src/protocol.c). Reintroduz o protocolo do firmware PIC
original sobre a USART1 do STM32F410.

> ⚠️ **Mudança de unidades:** nesta versão **temperatura** trafega em **graus
> inteiros** e **umidade** em **% inteiro** (o original usava ×10). Veja §8.

---

## 1. Camada física

| Parâmetro | Valor |
|---|---|
| Periférico | USART1 (PA9 TX / PA10 RX) |
| Velocidade | **9600 bps** |
| Formato | **8N1** (8 bits, sem paridade, 1 stop) |
| Controle de fluxo | nenhum |
| Nível | 3,3 V TTL no MCU (RS-232 só via transceptor externo) |

RX por interrupção + FIFO de 64 bytes. Erros de paridade/framing/overrun são
tratados (limpa e descarta o frame em montagem).

---

## 2. Formato do frame

```
+------+--------+------------------+-------+
| '*'  |  CMD   |   DADOS (0..6)   |  CRC  |
| 0x2A | 1 byte |   N bytes        | 1 byte|
+------+--------+------------------+-------+
```

- **Início:** sempre `'*'` (0x2A).
- **CMD:** 1 byte ASCII (define a ação e o tamanho do frame — ver §3).
- **DADOS:** 0 a 6 bytes úteis (mais *filler* em alguns comandos — ver §3).
- **CRC:** XOR de **todos** os bytes do frame. Frame válido quando
  `XOR(todos os bytes, inclusive '*' e CRC) == 0`.
  Ou seja, o transmissor calcula `CRC = '*' ^ CMD ^ dados...` e envia esse valor.
- **Timeout de sincronismo:** ~**504 ms**. Se o frame não completar nesse tempo,
  é descartado e a recepção volta a esperar `'*'`.

---

## 3. Tamanho do frame (NBCR)

O tamanho **total** do frame é determinado pela **faixa ASCII do comando**
(`nbcr_do_comando`):

| Faixa do CMD | Tamanho total | Bytes de dados úteis |
|---|---|---|
| `>= 'y'` (0x79) | 5 | 2 |
| `>= 'a'` (0x61) | 4 | 1 |
| `>= 'A'` (0x41) | 3 | 0 |
| `>= ':'` (0x3A) | 9 | 6 |
| demais (`< ':'`) | 10 | 7 (5 úteis + 2 *filler*) |

> Os comandos `0 1 2 3 4` caem na última faixa (frame de **10 bytes**): usam 5
> bytes de dados e mais **2 bytes de *filler*** antes do CRC (qualquer valor,
> mas entram no CRC). Os comandos `: ; < =` usam frame de **9 bytes** com 6
> bytes de dados (sem filler).

---

## 4. Respostas e ACK

- **Comandos de escrita** respondem com **ACK = `'F'`** (0x46) — 1 byte, sem CRC.
  - Exceção: comandos de RTC só dão ACK se o valor for **válido** (data/hora
    fora de faixa **não** recebe `'F'`).
- **Comandos de leitura** respondem com um frame `letra + dados + CRC(XOR)`.
- **Comando desconhecido:** ignorado (sem resposta).
- A gravação da config na Flash é **adiada** (~800 ms após a última escrita);
  o `'F'` é enviado **na hora**.

---

## 5. Comandos de ESCRITA (configuração)

Todos respondem **`'F'`** (ACK). Tamanho = NBCR da faixa (§3).

### 5.1 Exibição / escala

| CMD | Frame total | Ação |
|---|---|---|
| `J` | 3 | escala = **°C** |
| `H` | 3 | escala = **°F** |
| `F` | 3 | exibição alternada = on *(inerte nesta versão)* |
| `G` | 3 | exibição alternada = off *(inerte)* |

### 5.2 RTC (1 byte de dado)

| CMD | Frame | Dado (d0) | Ação |
|---|---|---|---|
| `d` | 4 | dia (1–31) | ajusta dia |
| `e` | 4 | mês (1–12) | ajusta mês |
| `a` | 4 | ano (0–99) | ajusta ano (20xx) |
| `h` | 4 | hora (0–23) | ajusta hora |
| `i` | 4 | min (0–59) | ajusta minuto (preserva segundos) |

ACK só se o valor resultar em data/hora **válida**.

### 5.3 Calibração da umidade (2 bytes, big-endian)

| CMD | Frame | Dados | Ação |
|---|---|---|---|
| `z` | 5 | ZUR (d0=MSB, d1=LSB) | zero da UR |
| `y` | 5 | GUR (d0=MSB, d1=LSB) | ganho da UR |

`dUR = (mV_ADC·GUR − ZUR)/100` (mV no ADC, pós-divisor). Defaults `GUR=49`,
`ZUR=23863` para HIH5031 @5V com divisor 1k/1k8.

### 5.4 Datalogger

| CMD | Frame | Dado | Ação |
|---|---|---|---|
| `D` | 3 | — | habilita o log |
| `E` | 3 | — | desabilita o log |
| `Z` | 3 | — | **zera/formata** o datalogger (ACK) |
| `s` | 4 | intervalo (d0) | base = **segundos**, intervalo = d0 |
| `m` | 4 | intervalo (d0) | base = **minutos**, intervalo = d0 |
| `c` | 4 | intervalo (d0) | base = **horas**, intervalo = d0 |

### 5.5 Buzzer / alarmes

| CMD | Frame | Dados | Ação |
|---|---|---|---|
| `b` | 4 | tipo (d0) | tipo de toque: 0=Contínuo,1=Lento,2=Médio,3=Rápido |
| `0` | 10 | modo(d0), alta(d1,d2), baixa(d3,d4) + 2 filler | **alarme por temperatura**¹ |
| `1` | 10 | modo(d0), alta(d1,d2), baixa(d3,d4) + 2 filler | **alarme por umidade**¹ |

¹ modo: 0=Desabilitado, 1=Alta, 2=Baixa, 3=Ambos. Limites **alta/baixa** em
16 bits big-endian: temperatura em **grau inteiro**, umidade em **% inteiro**.

### 5.6 Relés por grandeza (6 bytes, frame de 9)

| CMD | Frame | Alvo |
|---|---|---|
| `:` | 9 | Relé 1 por **temperatura** |
| `;` | 9 | Relé 1 por **umidade** |
| `<` | 9 | Relé 2 por **temperatura** |
| `=` | 9 | Relé 2 por **umidade** |

Layout dos 6 bytes de dados:

| d0 | d1 | d2,d3 | d4 | d5 |
|---|---|---|---|---|
| habilita (0/1) | lógica (0=aquecer/umidif., 1=refrigerar/desumid.) | setpoint (16 bits BE) | HN | HP |

Setpoint/HN/HP: temperatura em **grau inteiro** (0–999); umidade em **% inteiro**
(0–100).

### 5.7 Eventos horários (frame de 10; 5 bytes úteis + 2 filler)

| CMD | Alvo |
|---|---|
| `2` | evento de **alarme** |
| `3` | evento do **Relé 1** |
| `4` | evento do **Relé 2** |

Layout dos 5 bytes úteis:

| d0 | d1 | d2 | d3 | d4 |
|---|---|---|---|---|
| índice (0–7) | liga Hora (0–23) | liga Min (0–59) | desliga Hora | desliga Min |

São **8 eventos por fonte** (índice 0–7). Janela `[liga, desliga)`, suporta
cruzar a meia-noite. Para **desabilitar** um evento, envie `liga == desliga`.

### 5.8 EEPROM externa

| CMD | Frame | Dado | Ação |
|---|---|---|---|
| `k` | 4 | tipo (d0) | 0 = 24LC256 (32 KB) · 1 = 24AA512 (64 KB). Reinicializa o datalogger. |

---

## 6. Comandos de LEITURA

Cada um responde com um frame `letra + dados + CRC(XOR)`. Frame de requisição =
3 bytes (`'*' + CMD + CRC`).

### 6.1 `V` — Versão
Resposta: `'V'` + major + minor + CRC → ex.: `56 01 00 57` = versão **1.0**.

### 6.2 `X` — Temperatura instantânea
Resposta (4 bytes + CRC):

| b0 | b1 | b2,b3 |
|---|---|---|
| `'X'` | flags | temp (16 bits BE, valor absoluto) |

`b1`: bit0 = sinal negativo; bit7 = escala (1=°C, 0=°F). Temperatura em **grau
inteiro**.

### 6.3 `U` — Umidade instantânea
Resposta (3 bytes + CRC): `'U'` + UR(16 bits BE) + CRC. UR em **% inteiro** (0–100).

### 6.4 `R` — Status das saídas
Resposta (5 bytes + CRC):

| b0 | b1 | b2 | b3 | b4 |
|---|---|---|---|---|
| `'R'` | buzzer (0/1) | relé1 (0/1) | relé2 (0/1) | flags (byte baixo — ver §7) |

### 6.5 `T` — Status completo
Resposta (24 bytes + CRC):

| Bytes | Campo |
|---|---|
| b0 | `'T'` |
| b1,b2 | temperatura °C (16 bits BE, valor absoluto, **grau inteiro**) |
| b3 | sinal (1 = negativa) |
| b4,b5 | temperatura °F (16 bits BE, **grau inteiro**) |
| b6,b7 | umidade (16 bits BE, **% inteiro**) |
| b8,b9 | tensão 5 V (mV, 16 bits BE) |
| b10,b11 | tensão 12 V (mV, 16 bits BE) |
| b12 | dia · b13 mês · b14 ano |
| b15 | hora · b16 min · b17 seg |
| b18,b19 | flags de status (16 bits BE — ver §7) |
| b20 | escala (1=°C) |
| b21,b22 | total de registros no log (16 bits BE) |
| b23 | log habilitado (0/1) |

### 6.6 `W` — Configuração (resumo)
Resposta (16 bytes + CRC):

| Bytes | Campo |
|---|---|
| b0 | `'W'` |
| b1 | escala (1=°C) |
| b2 | exibição alternada |
| b3,b4 | ZUR (16 bits BE) |
| b5,b6 | GUR (16 bits BE) |
| b7 | log habilitado |
| b8 | log base (0=s,1=min,2=h) |
| b9,b10 | log intervalo (16 bits BE) |
| b11 | modo alarme temp · b12 modo alarme UR · b13 tipo buzzer |
| b14 | Relé1 temp habilitado · b15 Relé2 temp habilitado |

---

## 7. Bits de status (`control_status_flags`)

Campo de 16 bits (em `R` o byte baixo; em `T` os 16 bits):

| Bit | Máscara | Significado |
|---|---|---|
| 0 | 0x0001 | Relé 1 ligado |
| 1 | 0x0002 | Relé 2 ligado |
| 2 | 0x0004 | Buzzer ativo |
| 4 | 0x0010 | Sensor de temperatura desconectado |
| 5 | 0x0020 | Sensor de umidade desconectado |
| 6 | 0x0040 | Umidade fora de faixa |
| 8 | 0x0100 | Alarme de temperatura alta |
| 9 | 0x0200 | Alarme de temperatura baixa |
| 10 | 0x0400 | Alarme de umidade alta |
| 11 | 0x0800 | Alarme de umidade baixa |
| 12 | 0x1000 | Falha de alimentação |

---

## 8. Download do datalogger (`C`)

1. Host envia `C` (frame de 3 bytes). Se houver registros, o equipamento envia
   **imediatamente o 1º bloco** (9 bytes).
2. Para cada bloco seguinte, o host envia um único byte **`'#'`** (0x23) —
   *handshake*, fora do formato de frame.
3. Quando acabam os registros (ou em erro de leitura), o equipamento envia o
   terminador **`"FIM"` + CRC** (`'F' 'I' 'M' CRC`).

Se não houver registros, responde direto com `"FIM"`+CRC.

### Formato do bloco / registro (9 bytes)

| Byte | Campo |
|---|---|
| 0 | Dia |
| 1 | Mês |
| 2 | Ano |
| 3 | Hora (bit7: 1=°C, 0=°F) |
| 4 | Minuto (bit7: 1 = temperatura negativa) |
| 5 | Temperatura MSB |
| 6 | Temperatura LSB |
| 7 | Umidade (% inteiro) |
| 8 | CRC = XOR dos bytes 0..7 |

Temperatura em **grau inteiro** na escala vigente (bit em b3).

---

## 9. Observações / compatibilidade

- **Unidades inteiras:** temperatura (grau) e umidade (%) trafegam **sem ×10**.
  Ajustar o Ummi Manager para o novo layout.
- **Comando `k`** (tipo de EEPROM) é novo (não existia no original).
- `'F'` é, ao mesmo tempo, o **ACK** e o **comando** "exibição alternada on" —
  herança do protocolo original (só relevante em montagem com eco/loopback).
- Setpoints/limites são **saturados** na recepção (`config_valida`): temperatura
  0–999, umidade 0–100, etc.

---

## 10. Exemplos (bytes em hexadecimal)

| Ação | Frame enviado | Resposta |
|---|---|---|
| Ler versão | `2A 56 7C` | `56 01 00 57` (v1.0) |
| Ler temperatura | `2A 58 72` | `58 fl HH LL crc` |
| Ler umidade | `2A 55 7F` | `55 HH LL crc` |
| Ler status (R) | `2A 52 78` | `52 bz r1 r2 fl crc` |
| Escala °C | `2A 4A 60` | `46` (ACK) |
| Escala °F | `2A 48 62` | `46` |
| ZUR = 23863 (0x5D37) | `2A 7A 5D 37 3A` | `46` |
| GUR = 49 (0x0031) | `2A 79 00 31 62` | `46` |
| Habilitar log | `2A 44 6E` | `46` |
| Zerar log | `2A 5A 70` | `46` |
| Iniciar download | `2A 43 69` | 1º bloco (9 bytes) |
| Próximo bloco | `23` (`'#'`) | bloco (9 bytes) ou `"FIM"`+CRC |

> CRC de exemplo: para `'*' 'V'` → `0x2A ^ 0x56 = 0x7C`.
