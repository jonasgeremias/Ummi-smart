# CBS_01 - Contador de Pecas STM32

Aplicacao para STM32F410RBTx que reproduz a logica do contador de pecas da JIGA UMMI. O sistema conta pulsos de sensor, controla os pontos de inspecao e inspecao obrigatoria, mostra os valores em display de 6 digitos de 7 segmentos e salva as configuracoes em memoria persistente.

## Hardware Esperado

- MCU: STM32F410RBTx.
- Display: 6 digitos de 7 segmentos multiplexados.
- Entrada de contagem: `PB8`.
- Entrada de inspecao/gabarito: `PB9`.
- Botoes de membrana:
  - Relogio/menu: `PC1`.
  - Menos: `PC2`.
  - Mais: `PC3`.
- Saida de inspecao: `PC7`.
- Saida de inspecao obrigatoria: `PC9`.
- EEPROM externa: 24x256 via I2C1, endereco `0x50`.
- Serial: USART1 em `PA9/PA10`, mantida habilitada para feature futura.

As entradas de sensores estao configuradas no estado atual como ativas em nivel alto:

```c
#define INPUT_01_ACTIVE_LEVEL HIGH
#define INPUT_02_ACTIVE_LEVEL HIGH
```

PB8 e PB9 usam `GPIO_PULLDOWN`. Em repouso devem ficar em `0 V`; ao acionar o sensor devem ir para nivel alto.

## Funcionamento Principal

Na inicializacao, o firmware:

1. Inicializa GPIO, I2C, RTC e USART1.
2. Le a configuracao salva na EEPROM.
3. Se nao houver dados validos, usa valores padrao:
   - Inspecao: `100`.
   - Inspecao obrigatoria: `200`.
   - Contador: `0`.
   - Ultima conferencia: `0`.
4. Mostra splash inicial `BE1001`.
5. Entra no modo contador.

## Modo Contador

O display mostra o contador atual.

Ao receber pulso valido em `PB8`, o contador incrementa. A captura do pulso ocorre no `SysTick`, por borda debounced, e o `main` consome os pulsos acumulados. Isso evita perder contagem durante rotinas mais lentas, como escrita em EEPROM.

O contador vai de `0` a `999999` e retorna para `0`.

Ao manter o botao `MAX` pressionado no modo contador, o display mostra `ultima_conferencia` em vez do contador atual.

## Sensor de Inspecao

Ao acionar o sensor de inspecao em `PB9`:

1. `ultima_conferencia` recebe o valor atual do contador.
2. O display mostra splash `InSPEC`.
3. Os dados sao salvos imediatamente.

A diferenca usada para acionar as saidas e:

```text
diferenca = contador - ultima_conferencia
```

Com tratamento circular quando o contador passa de `999999` para `0`.

## Saidas

- `PC7` liga quando `diferenca >= setpoint_inspecao`.
- `PC9` liga quando `diferenca >= setpoint_obriga_inspecao`.

Quando a conferencia e feita pelo sensor de inspecao, `ultima_conferencia` acompanha o contador atual, zerando a diferenca operacional.

## Configuracao de Setpoints

O botao relogio/menu troca os modos.

No modo contador:

- Segurar relogio por aproximadamente 3 segundos entra em `SEtInS`.

No modo de setpoint:

- Segurar relogio por aproximadamente 250 ms confirma o valor atual e avanca.
- `SEtInS`: configura o setpoint de inspecao.
- `SEtObr`: configura o setpoint de inspecao obrigatoria.
- Ao sair do ultimo setpoint, mostra `-0KUE-` e retorna ao contador.

Os botoes `MAX` e `MIN` alteram o valor. Mantendo pressionado, o incremento acelera.

Regras:

- `setpoint_inspecao` nao pode ser maior que `setpoint_obriga_inspecao`.
- `setpoint_obriga_inspecao` nao pode ser menor que `setpoint_inspecao`.
- Faixa geral: `0` a `999999`.

Ao confirmar um setpoint, o salvamento e imediato.

## Reset do Contador

Segurar `MIN` e `MAX` juntos por aproximadamente 10 segundos:

1. Zera `contador`.
2. Zera `ultima_conferencia`.
3. Desliga `PC7` e `PC9`.
4. Mostra splash de reset.
5. Salva imediatamente.

## Persistencia

Os dados persistidos sao:

- Assinatura da aplicacao.
- Contador.
- Ultima conferencia.
- Setpoint de inspecao.
- Setpoint de inspecao obrigatoria.

O backend principal usa EEPROM externa 24x256 via I2C. A gravacao usa dois slots com sequencia e checksum para reduzir risco de perder dados se faltar energia durante escrita.

O fallback em flash interna esta desabilitado:

```c
#define EEPROM_ENABLE_FLASH_FALLBACK 0
```

Isso evita que o firmware grave na flash interna durante o boot ou operacao normal, o que poderia atrapalhar debug/grave via ST-LINK.

## Display

O display e varrido a cada 1 ms pelo `SysTick`.

A rotina de varredura desliga todos os displays antes de alterar os segmentos e so depois habilita o digito atual. Isso reduz ghosting e evita aparecer digito perdido durante splashes.

## Serial

USART1 esta inicializada em `115200 bps`, TX/RX habilitados por registradores CMSIS, sem HAL UART.

No boot e enviado:

```text
UMMI CONTADOR STM
```

A recepcao serial ja possui buffer circular, mas ainda nao ha protocolo de comandos implementado.

## Gravacao e Debug

Configuracao recomendada para ST-LINK:

```text
Interface: SWD
Mode: Connect under reset
Reset mode: Hardware reset
Frequency: 100 kHz
```

O projeto possui:

- `CBS_01-Contador_de_Pecas.cfg`
- `CBS_01-Contador_de_Pecas.launch`

Ambos foram ajustados para SWD em `100 kHz`.

Se o alvo falhar com `DEV_TARGET_NOT_HALTED` ou `Error finishing flash operation`, recuperar com STM32CubeProgrammer:

```text
Mode: Under Reset
Reset mode: Hardware reset
Frequency: 100 kHz
Full chip erase / Mass erase
```

CLI equivalente:

```powershell
STM32_Programmer_CLI.exe -c port=SWD mode=UR reset=HWrst freq=100 -e all
```

Depois do erase, gravar novamente pelo CubeIDE.

## Testes de Bancada Recomendados

1. Medir PB8 em repouso: deve estar em `0 V`.
2. Acionar sensor de contagem: PB8 deve ir para nivel alto e o contador deve incrementar.
3. Medir PB9 em repouso: deve estar em `0 V`.
4. Acionar sensor de inspecao: PB9 deve ir para nivel alto, mostrar `InSPEC` e atualizar `ultima_conferencia`.
5. Ajustar `SEtInS`, desligar e religar: valor deve permanecer.
6. Ajustar `SEtObr`, desligar e religar: valor deve permanecer.
7. Contar ate passar o setpoint de inspecao: `PC7` deve ligar.
8. Contar ate passar o setpoint obrigatorio: `PC9` deve ligar.
9. Acionar sensor de inspecao: saidas devem retornar conforme a diferenca volta a zero.

## Observacoes Importantes

- Se o sensor de contagem for ativo baixo, alterar `INPUT_01_ACTIVE_LEVEL` para `LOW` e configurar PB8 com `GPIO_PULLUP`.
- Se o sensor de inspecao for ativo baixo, alterar `INPUT_02_ACTIVE_LEVEL` para `LOW` e configurar PB9 com `GPIO_PULLUP`.
- Setpoint `0` hoje significa acionamento imediato, pois a condicao e `diferenca >= setpoint`.
- Nao usar funcoes antigas de `24x256.c` com `HAL_MAX_DELAY` em fluxo critico sem revisar, pois podem bloquear o loop principal.
