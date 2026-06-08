# Aplicacao - Controlador de Secagem de Fumo

## Objetivo

Firmware STM32 para controle de temperatura e registro de dados em estufas de secagem de fumo. O sistema mede temperatura, umidade, tensoes de alimentacao, estado das saidas e status operacional; controla o soprador da fornalha por rele; exibe dados em display de 6 digitos; permite configuracao por menu local e Bluetooth HM-10; e grava historico periodico em EEPROM externa 24AA256.

Esta especificacao substitui a aplicacao de contador de pecas, mas deve preservar a infraestrutura funcional ja existente no projeto: varredura do display multiplexado, leitura dos botoes da membrana, turbo de incremento/decremento, temporizacao por tick e pinos de saida a rele `SD_SAIDA1`/`SD_SAIDA2` atualmente mapeados como `SAIDA_01` e `SAIDA_02`.

## Hardware

### Entradas Analogicas

| Sinal | Pino logico | Funcao | Conversao |
|---|---|---|---|
| Temperatura | `EA_TEMPERATURA` | Saida do LM35 | Temperatura calculada pela diferenca entre `EA_TEMPERATURA` e `EA_TEMP_REF_GND` |
| Referencia GND do LM35 | `EA_TEMP_REF_GND` | Compensacao de offset/GND | Subtrair da leitura do LM35 antes da conversao |
| Umidade | `EA_UMIDADE` | HIH3051 analogico | 0,7575 V = 0% UR; 3,9375 V = 100% UR |
| Tensao 5 V | `EA_5V` | Monitoramento da linha 5 V | Divisor 1k + 1k8 para GND |
| Tensao 12 V | `EA_12V` | Monitoramento da linha 12 V | Divisor 10k + 3k3 para GND |

Parametros de umidade:

- `ZUR = 6950`: zero/calibracao de offset do sensor.
- `GUR = 314`: ganho/multiplicador.
- Ambos devem ser configuraveis e persistidos para permitir troca de sensor HIH3051.

### Entradas Digitais E Botoes

| Sinal | Funcao |
|---|---|
| `BOTAO_RELOGIO` | Confirma/Enter e entrada no menu por pressionamento longo de 3 s |
| `BOTAO_MAX` | Incremento, navegacao para cima e gatilho do diagnostico oculto |
| `BOTAO_MIN` | Decremento e navegacao para baixo |
| `INPUT_01` | Reservado para status discreto futuro |
| `INPUT_02` | Reservado para status discreto futuro |

### Saidas

| Sinal | Funcao |
|---|---|
| `SAIDA_01` / `SD_SAIDA1` | Rele do soprador da fornalha |
| `SAIDA_02` / `SD_SAIDA2` | Rele auxiliar/reserva para alarme ou expansao futura |
| `SD_BUZZER` | Alarme sonoro de temperatura alta/baixa e alertas futuros |
| Display 7 segmentos | Interface local de leitura e configuracao |

### Comunicacao E Armazenamento

| Recurso | Funcao |
|---|---|
| USART1 | HM-10 Bluetooth, 115200 8N1 |
| RTC interno | Data/hora do datalogger usando cristal de 32,768 kHz |
| I2C1 + 24AA256 | Datalogger em EEPROM externa |
| Flash/EEPROM de configuracao | Parametros configuraveis e calibracoes |

## Medicoes

### Aquisicao Analogica

As entradas analogicas devem ser amostradas lentamente, a cada 20 ms. Cada canal usa media circular de 8 amostras, resultando em um valor completo a cada 160 ms.

Para poupar memoria e processamento, a media deve usar:

- buffer circular de 8 amostras por canal;
- soma acumulada por canal;
- ao inserir uma nova amostra, subtrair a amostra antiga da soma e somar a nova;
- valor tratado por deslocamento de 3 bits (`soma >> 3`), equivalente a divisao por 8.

O firmware so deve alterar o estado exibido ou marcar sensor desconectado apos mais de 5 leituras validas no filtro.

### Temperatura

A temperatura deve ser obtida por ADC diferencial em software:

1. Ler `EA_TEMPERATURA`.
2. Ler `EA_TEMP_REF_GND`.
3. Calcular `adc_lm35 = adc_temperatura - adc_ref_gnd`, saturando em zero se a referencia for maior.
4. Converter para tensao usando Vref/ADC calibrado.
5. Converter LM35 considerando 10 mV por grau Celsius.

A aplicacao deve manter temperatura em decimos de grau Celsius internamente para evitar uso desnecessario de ponto flutuante no controle.

Se o AD diferencial de temperatura for menor que 5 AD apos o filtro minimo de leituras, o sensor deve ser marcado como desconectado. Nessa condicao, o controle do soprador deve ir para estado seguro e a tela deve indicar desconexao em vez do valor.

### Umidade

A umidade relativa deve usar a faixa eletrica informada:

- 0% UR em 0,7575 V.
- 100% UR em 3,9375 V.

O calculo de producao deve usar os parametros persistidos `ZUR` e `GUR`, mantendo saturacao em 0% e 100%. O documento de implementacao deve preservar compatibilidade para recalibrar o sensor via menu e serial.

Se a leitura analogica de umidade for menor que 0,3 V apos o filtro minimo de leituras, o sensor deve ser marcado como desconectado. Nessa condicao, a tela deve indicar desconexao em vez do valor.

### Tensoes

- Tensao 5 V: compensar divisor 1k + 1k8.
- Tensao 12 V: compensar divisor 10k + 3k3.
- As tensoes devem ser usadas no datalogger, pacote de tempo real e tela oculta de diagnostico.

## Logica De Controle

### Controle Do Soprador

`SAIDA_01` aciona o soprador. Ao ligar o soprador, a ventilacao da fornalha aumenta e a temperatura tende a subir.

Controle basico:

- Se `temperatura < setpoint_temperatura - histerese`, ligar `SAIDA_01`.
- Se `temperatura >= setpoint_temperatura`, desligar `SAIDA_01`.
- Em erro de sensor de temperatura, desligar `SAIDA_01` e sinalizar alarme.
- Respeitar tempo minimo configuravel entre comutacoes para proteger rele e soprador.

### Alarmes

Alarmes iniciais:

- Temperatura alta: `temperatura >= limite_temperatura_alta`.
- Temperatura baixa: `temperatura <= limite_temperatura_baixa` apos tempo de estabilizacao configuravel.
- Falha de sensor de temperatura: leitura fora de faixa ou ADC invalido.
- Falha de umidade: leitura fora da faixa fisica esperada.
- Falha de alimentacao: 5 V ou 12 V fora dos limites configurados.

O buzzer deve operar por temporizacao cooperativa, sem bloquear o loop principal.

### Estados Do Sistema

| Estado | Descricao |
|---|---|
| Inicializando | Carrega configuracoes, inicializa EEPROM/RTC/serial e mostra splash |
| Operacao | Mede sensores, atualiza controle, display e datalogger |
| Menu | Saidas permanecem controladas, mas edicao local fica ativa |
| Diagnostico | Tela oculta mostra tensoes medidas |
| Falha | Saidas seguras e alarme ativo conforme severidade |

## Display E Menu

### Tela Principal

O display principal deve compactar umidade e temperatura:

- Formato normal: `UUUTTT`.
- Os tres primeiros digitos mostram umidade.
- Os tres ultimos digitos mostram temperatura.
- Se um sensor estiver desconectado ou invalido, seu bloco deve mostrar `---`.
- Exemplo com ambos desconectados: `------`.

### Splash Inicial

Ao energizar, a aplicacao deve exibir a sequencia:

- `BE1  1.0`, com ponto decimal entre `1` e `0`.
- Data do RTC em `DDMMAA`.
- Hora do RTC em `HHMMSS`.

### Entrada No Menu

Manter comportamento atual:

- Segurar `BOTAO_RELOGIO` por 3 segundos arma a entrada no menu; a entrada so acontece quando o botao for solto.
- Ao completar os 3 segundos ainda segurando o botao, o display mostra uma splash de entrada no menu. Nenhuma acao de menu e executada ate o botao ser solto.
- Em menu, `BOTAO_RELOGIO` so executa a acao quando for solto.
- `BOTAO_MAX` incrementa.
- `BOTAO_MIN` decrementa.
- Turbo de incremento deve preservar a logica existente de passos progressivos ao segurar o botao.

### Padrao De Menu Tipo Inversor

O menu deve seguir o estilo de inversores de frequencia:

- Primeiro nivel: lista de parametros.
- `BOTAO_MAX` e `BOTAO_MIN` navegam entre parametros.
- `BOTAO_RELOGIO` solto entra no parametro selecionado.
- Segundo nivel: edicao do valor.
- `BOTAO_MAX` e `BOTAO_MIN` alteram o valor com turbo.
- `BOTAO_RELOGIO` solto confirma, salva o valor e retorna para a lista.
- Para sair do menu, segurar `BOTAO_RELOGIO` por 3 segundos no primeiro nivel/lista. O display mostra splash de saida e a saida so acontece ao soltar o botao.

Parametros iniciais:

| Codigo | Tela | Parametro |
|---|---|---|
| `P01` | `P01  T` | Setpoint de temperatura |
| `P02` | `P02HIS` | Histerese |
| `P03` | `P03ALT` | Limite de temperatura alta |
| `P04` | `P04BAI` | Limite de temperatura baixa |
| `P05` | `P05ZUR` | Zero/calibracao da umidade |
| `P06` | `P06GUR` | Ganho da umidade |
| `P07` | `P07LOG` | Reset do datalogger; editar para `1` e confirmar |
| `P08` | `P08PER` | Periodo de gravacao do datalogger em segundos |
| `P09` | `P09DAT` | Data do RTC em `DDMMAA` |
| `P10` | `P10HOR` | Hora do RTC em `HHMM00`; segundos ficam em `00` pelo menu |

Em `P09DAT` e `P10HOR`, ao entrar com ENTER a edicao e feita por campo de dois digitos. O campo ativo pisca; ao pressionar MAIS ou MENOS o valor completo fica aceso por 1 s para facilitar a leitura. ENTER avanca para o proximo campo e, no ultimo campo, grava o RTC e mostra `-SAVE-`.

### Diagnostico Oculto

Na tela principal:

- Pressionar `BOTAO_MAX` 5 vezes seguidas abre a tela de diagnostico.
- A tela de diagnostico exibe as tensoes simultaneamente: tres digitos da esquerda para 12 V e tres digitos da direita para 5 V.
- Exemplo: `12.005.0`, com os pontos decimais ligados para indicar `12.0 V` e `05.0 V`.
- A saida do diagnostico deve ocorrer por `BOTAO_RELOGIO`, timeout ou retorno definido na implementacao.

### Fluxo Do Menu

Menu proposto:

| Item | Parametro | Unidade | Faixa inicial |
|---|---|---|---|
| `SP t` | Setpoint de temperatura | 0,1 C | 0,0 a 99,9 C |
| `Hi t` | Limite de temperatura alta | 0,1 C | 0,0 a 99,9 C |
| `Lo t` | Limite de temperatura baixa | 0,1 C | 0,0 a 99,9 C |
| `HiS` | Histerese de controle | 0,1 C | 0,1 a 20,0 C |
| `dLoG` | Periodo do datalogger | segundos/minutos | 10 s a 60 min |
| `ZUr` | Zero da umidade | inteiro | 0 a 65535 |
| `GUr` | Ganho da umidade | inteiro | 1 a 65535 |
| `rtc` | Ajuste de data/hora | calendario | valido RTC |
| `rStLG` | Reset do datalogger | confirmacao | nao/sim |

## Parametros Configuraveis

| Nome interno | Padrao | Persistencia | Serial | Menu |
|---|---:|---|---|---|
| `setpoint_temperatura_dC` | 450 | Flash/EEPROM config | Sim | Sim |
| `histerese_temperatura_dC` | 10 | Flash/EEPROM config | Sim | Sim |
| `limite_temperatura_alta_dC` | 700 | Flash/EEPROM config | Sim | Sim |
| `limite_temperatura_baixa_dC` | 250 | Flash/EEPROM config | Sim | Sim |
| `tempo_estabilizacao_baixa_s` | 300 | Flash/EEPROM config | Sim | Sim |
| `periodo_datalogger_s` | 60 | Flash/EEPROM config | Sim | Sim |
| `zur_umidade` | 6950 | Flash/EEPROM config | Sim | Sim |
| `gur_umidade` | 314 | Flash/EEPROM config | Sim | Sim |
| `limite_5v_min_mV` | 4500 | Flash/EEPROM config | Sim | Futuro |
| `limite_5v_max_mV` | 5500 | Flash/EEPROM config | Sim | Futuro |
| `limite_12v_min_mV` | 10000 | Flash/EEPROM config | Sim | Futuro |
| `limite_12v_max_mV` | 15000 | Flash/EEPROM config | Sim | Futuro |

## Datalogger

### Meio Fisico

O datalogger deve ser armazenado na EEPROM externa 24AA256 via I2C1. Usar escrita paginada respeitando limites de pagina da memoria e rotina nao bloqueante sempre que possivel.

Implementacao inicial:

- area do datalogger inicia em `0x0200`, separada da area inicial usada pela persistencia existente;
- cabecalho em `0x0200`;
- registros a partir de `0x0240`;
- gravacao circular;
- periodo de gravacao configuravel pelo menu `P08PER`, padrao inicial de 60 s;
- reset local via menu `P07LOG`.

### Estrutura Geral

Area sugerida:

- Cabecalho fixo com assinatura, versao, tamanho do registro, capacidade total, proximo indice e quantidade valida.
- Registros circulares de tamanho fixo.
- CRC por registro para detectar corrupcao.

### Cabecalho

| Campo | Tipo | Descricao |
|---|---|---|
| `assinatura` | `uint32_t` | Identifica datalogger valido |
| `versao` | `uint16_t` | Versao da estrutura |
| `record_size` | `uint16_t` | Tamanho de cada registro |
| `capacidade` | `uint16_t` | Quantidade maxima de registros |
| `proximo_indice` | `uint16_t` | Posicao da proxima escrita |
| `quantidade_valida` | `uint16_t` | Quantidade de registros validos |
| `seq_global` | `uint32_t` | Sequencia monotona de registros |
| `crc16` | `uint16_t` | CRC do cabecalho |

### Registro

| Campo | Tipo | Unidade |
|---|---|---|
| `timestamp` | `uint32_t` | Unix time ou segundos desde epoca local definida |
| `temperatura_dC` | `int16_t` | 0,1 C |
| `umidade_dUR` | `uint16_t` | 0,1% UR |
| `flags` | `uint16_t` | bits de saidas, sensores e alarmes |
| `crc16` | `uint16_t` | CRC do registro |

Bits de `flags`:

| Bit | Significado |
|---|---|
| 0 | `SAIDA_01` soprador ligado |
| 1 | `SAIDA_02` ligada |
| 4 | temperatura desconectada |
| 5 | umidade desconectada |
| 6 | umidade fora da faixa eletrica esperada |
| 8 | alarme de temperatura alta |
| 9 | alarme de temperatura baixa |
| 10 | alarme/falha do sensor de temperatura |
| 11 | alarme/falha do sensor de umidade |
| 12 | alarme de alimentacao |

### Reset Do Datalogger

Deve estar disponivel:

- Via menu local.
- Via protocolo serial.

Reset deve recriar o cabecalho, zerar contadores e nao apagar configuracoes da aplicacao.

## Protocolo Serial Bluetooth

### Camada Fisica

- USART1.
- 115200 bps.
- 8 bits, sem paridade, 1 stop bit.
- Recepcao por interrupcao em FIFO circular, no mesmo conceito dos exemplos `_Consulta/P0337-TX-G4` e `_Consulta/P0196-RX-STM32`.
- A ISR apenas le o byte, grava no FIFO, atualiza ultimo/penultimo byte e rearma timeout de 10 ms.
- Se receber `\r` ou `\n`, a ISR zera o timeout para o loop tratar o pacote imediatamente.
- Erros de UART `PE`, `FE`, `NE` e `ORE` sao limpos na ISR pela sequencia de leitura `SR`/`DR`; em erro critico o FIFO de recepcao e descartado.
- No loop principal, quando o timeout expira ou a quebra de linha e recebida, os bytes sao transferidos para um segundo buffer com interrupcao bloqueada apenas pelo tempo da copia; em seguida a recepcao e liberada e o pacote e tratado fora da interrupcao.

### Enquadramento

Pacote ASCII industrial:

```text
[STX][CMD_ID][TAM][DADO1,DADO2,...,DADON][CRC16]
```

Definicao proposta:

- `STX`: caractere `0x02`.
- `CMD_ID`: dois caracteres hexadecimais ASCII.
- `TAM`: dois caracteres hexadecimais ASCII indicando quantidade de bytes ASCII em `DADO`.
- `DADO`: campos ASCII separados por virgula, sem espacos.
- `CRC16`: quatro caracteres hexadecimais ASCII, CRC-16/IBM calculado de `CMD_ID` ate o ultimo byte de `DADO`.
- Terminacao opcional para debug: aceitar `\r`/`\n` apos CRC, mas nao exigir.

Resposta padrao:

```text
[STX][CMD_ID][TAM][DADO][CRC16]
```

Erros devem responder com comando de NACK (`7F`) e codigo de erro.

### Codigos De Erro

| Codigo | Significado |
|---|---|
| `E01` | CRC invalido |
| `E02` | Tamanho invalido |
| `E03` | Comando desconhecido |
| `E04` | Parametro fora de faixa |
| `E05` | Datalogger ocupado |
| `E06` | Indice inexistente |
| `E07` | Falha de EEPROM |
| `E08` | Falha de RTC |

### Tabela De Comandos

| CMD | Direcao | Funcao | Dados |
|---|---|---|---|
| `01` | App -> host | Identificacao | nome, versao, build, capacidade logger |
| `02` | Host -> app | Ler tempo real | vazio |
| `82` | App -> host | Tempo real | temp_dC, umid_dUR, v5_mV, v12_mV, saidas, status |
| `03` | Host -> app | Ler configuracao | vazio ou nome do parametro |
| `83` | App -> host | Configuracao | lista `nome=valor` |
| `04` | Host -> app | Gravar configuracao | lista `nome=valor` |
| `84` | App -> host | Configuracao gravada | `OK` |
| `05` | Host -> app | Ler status do datalogger | vazio |
| `85` | App -> host | Status do datalogger | total, capacidade, primeiro, ultimo, periodo |
| `06` | Host -> app | Ler datalogger a partir de indice | indice inicial, quantidade maxima |
| `86` | App -> host | Registro de datalogger | indice atual, total enviado/total disponivel, campos do registro |
| `07` | Host -> app | Resetar datalogger | `CONFIRMA` |
| `87` | App -> host | Reset datalogger | `OK` |
| `08` | Host -> app | Ajustar RTC | `DDMMAAHHMMSS` |
| `88` | App -> host | RTC ajustado | `OK` |
| `09` | Host -> app | Ler RTC | vazio |
| `89` | App -> host | RTC atual | `YYYY-MM-DD,hh:mm:ss` |
| `7F` | App -> host | NACK | codigo de erro |

Implementacao inicial aceita comandos terminados por `\r` ou `\n`. Alem dos pacotes com `CMD`, tambem aceita aliases ASCII para teste em terminal Bluetooth:

Pacotes completos iniciados por `STX` ja sao validados com `CMD`, `TAM` e `CRC16`. Em erro de tamanho responde `7F,E02`; em erro de CRC responde `7F,E01`.

- `02` ou `RT`: retorna tempo real em `82`.
- `01` ou `ID`: retorna identificacao em `01`.
- `03` ou `CFG?`: retorna configuracao em `83`.
- `04 SP=valor`, `04 HIS=valor`, `04 ALT=valor`, `04 BAI=valor`, `04 ZUR=valor`, `04 GUR=valor`, `04 PER=valor`: grava configuracao e responde `84`.
- Tambem aceita lista no mesmo comando: `04 SP=450,HIS=10,ALT=700,BAI=250,ZUR=6950,GUR=314,PER=60`.
- `05` ou `LOG?`: retorna status do logger em `85`.
- `06 indice,quantidade` ou `LOG indice,quantidade`: inicia envio assincrono de registros em pacotes `86`. Se a quantidade for omitida, envia 1 registro.
- `07 CONFIRMA` ou `LOGRST`: reseta o logger e responde `87`.
- `STOPLOG`: cancela envio assincrono do logger em andamento e responde `87,OK`.
- `08 DDMMAAHHMMSS`: ajusta RTC e responde `88`.
- `09` ou `RTC?`: retorna RTC em `89`.

Status do logger (`85`): `total,capacidade,proximo_indice,periodo_s,eeprom_ok,enviando`.

### Leitura Assincrona Do Datalogger

Fluxo:

1. Host envia `06` com indice inicial e quantidade maxima.
2. Firmware agenda envio e responde registros `86` em ciclos do loop principal.
3. Cada pacote `86` informa indice atual e tamanho total disponivel.
4. A transmissao pode ser interrompida por novo comando do host.
5. Em EEPROM ocupada ou indice invalido, responder `7F`.

## Checkpoints De Implementacao

### Checkpoint 1

Objetivo:

- Tela principal mostrando temperatura e umidade.
- Menu de diagnostico oculto por 5 pressionamentos consecutivos do `BOTAO_MAX`.
- Tela de diagnostico exibindo tensoes 5 V e 12 V medidas.

Escopo tecnico:

- Adicionar leitura ADC dos canais `EA_TEMPERATURA`, `EA_TEMP_REF_GND`, `EA_UMIDADE`, `EA_5V` e `EA_12V`.
- Criar conversoes iniciais de temperatura, umidade e tensoes.
- Preservar `display_scan()`, leitura de botoes em `timer.c`, entrada de menu por `BOTAO_RELOGIO` segurado e turbo existente.
- Nao alterar ainda datalogger nem protocolo serial alem do minimo necessario para nao quebrar build.

Teste esperado:

- Build sem erros.
- Display alterna temperatura/umidade.
- Cinco cliques em `BOTAO_MAX` entram no diagnostico.
- Diagnostico alterna/mostra 5 V e 12 V.
- `SAIDA_01` e `SAIDA_02` permanecem sem acionamento indevido neste checkpoint.

### Checkpoints Posteriores

| Checkpoint | Entrega |
|---|---|
| 2 | Controle do soprador por setpoint/histerese e alarmes basicos |
| 3 | Menu completo de configuracao e persistencia dos parametros |
| 4 | Datalogger em 24AA256 com cabecalho, registros e reset |
| 5 | Protocolo serial completo com tempo real, configuracao e leitura assincrona do logger |
| 6 | Validacao integrada, tratamento de falhas e documentacao final de testes |

## Criterios De Aprovacao Antes Da Implementacao

Antes de iniciar o Checkpoint 1, confirmar:

- Formato desejado da tela principal no display de 6 digitos.
- Faixas/padroes de temperatura adequadas ao processo real de secagem de fumo.
- Se `SAIDA_02` fica reservada ou ja deve ter funcao definida.
- Se o RTC deve usar data/hora real completa ou contador interno ate haver ajuste via Bluetooth.
