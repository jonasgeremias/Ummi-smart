# Resumo funcional

Este firmware implementa o contador de pecas UMMI no STM32, mantendo a logica funcional do programa original do PIC16F877A.

## Visao geral

O sistema conta pecas por uma entrada digital, registra a ultima inspecao feita pelo operador e aciona alertas quando a quantidade produzida desde a ultima conferencia atinge os setpoints configurados.

A varredura do display, os botoes da membrana, as entradas digitais e a base de timer ja estao integrados ao projeto STM32.

## Entradas

- `INPUT_01`: sensor de contagem de pecas.
- `INPUT_02`: sensor de gabarito/conferencia.
- `BOTAO_RELOGIO`: navegacao entre modos.
- `BOTAO_MAX`: incremento/consulta.
- `BOTAO_MIN`: decremento/reset em conjunto com MAX.

## Saidas

- `SAIDA_01`: alerta de inspecao recomendada.
- `SAIDA_02`: alerta de inspecao obrigatoria.

No PIC original existia um buzzer dedicado. No hardware STM atual nao ha uma saida exclusiva configurada para buzzer, entao a logica de temporizacao do buzzer foi preservada em software para uso futuro.

## Display

O display multiplexado de 6 digitos mostra:

- contador atual no modo normal;
- ultima conferencia enquanto `BOTAO_MAX` estiver pressionado no modo normal;
- valor temporario do setpoint durante configuracao;
- mensagens temporarias de operacao.

Mensagens implementadas:

- `bE1001`: splash inicial;
- `SEt InS`: entrada no ajuste do setpoint de inspecao;
- `SEt Obr`: entrada no ajuste do setpoint obrigatorio;
- `InSPEC`: conferencia registrada pelo sensor de gabarito;
- `---rSt`: reset completo do contador;
- `-0KUE-`: confirmacao ao sair da configuracao.

O ponto decimal da unidade pisca por alguns ciclos quando o contador e salvo.

## Modo normal

No modo `0`, o sistema:

- mostra o contador atual;
- incrementa o contador quando `INPUT_01` detecta uma peca;
- ignora pulsos de contagem durante 250 ms apos cada incremento;
- registra a ultima conferencia quando `INPUT_02` e acionada;
- calcula `diferenca = contador - ultima_conferencia`;
- trata overflow do contador quando a contagem passa de `999999` para `0`.

Se `BOTAO_MAX` for mantido pressionado e `BOTAO_MIN` estiver solto, o display mostra `ultima_conferencia`. Ao soltar, volta a mostrar o contador.

## Alertas

A diferenca entre contador e ultima conferencia controla as saidas:

- se `diferenca >= setpoint_inspecao`, liga `SAIDA_01`;
- se `diferenca >= setpoint_obriga_inspecao`, liga `SAIDA_02`;
- ao registrar nova conferencia, a diferenca volta a zero e as saidas desligam conforme os limites deixam de ser atingidos.

## Configuracao dos setpoints

O botao relogio controla a navegacao.

No modo normal:

- segurar `BOTAO_RELOGIO` por 3 segundos entra no modo `1`;
- o display mostra `SEt InS`;
- o valor editado passa a ser `setpoint_inspecao`.

No modo `1`:

- `BOTAO_MAX` incrementa o valor;
- `BOTAO_MIN` decrementa o valor;
- o valor nao pode passar de `setpoint_obriga_inspecao`;
- pressionar `BOTAO_RELOGIO` salva e avanca para o modo `2`;
- o display mostra `SEt Obr`.

No modo `2`:

- `BOTAO_MAX` incrementa o valor;
- `BOTAO_MIN` decrementa o valor;
- o valor nao pode ser menor que `setpoint_inspecao`;
- pressionar `BOTAO_RELOGIO` salva e volta ao modo normal;
- o display mostra a mensagem de confirmacao.

Incremento/decremento acelerado:

- pressionamento inicial: passo de 1;
- com tempo pressionado: passos de 10, 20 e 100.

## Reset completo

No modo normal, manter `BOTAO_MAX` e `BOTAO_MIN` pressionados juntos por 10 segundos executa:

- `contador = 0`;
- `ultima_conferencia = 0`;
- desliga `SAIDA_01` e `SAIDA_02`;
- zera temporizadores de alarme;
- salva os dados;
- mostra `---rSt`.

## Persistencia

Os dados persistidos sao:

- contador;
- ultima conferencia;
- setpoint de inspecao;
- setpoint obrigatorio.

Na primeira inicializacao, ou se a assinatura gravada for invalida, o firmware cria valores padrao:

- contador: `0`;
- ultima conferencia: `0`;
- setpoint de inspecao: `100`;
- setpoint obrigatorio: `200`.

## Serial

A USART1 fica habilitada para uso futuro:

- TX em `PA9`;
- RX em `PA10`;
- baud rate `115200`;
- recepcao por interrupcao;
- buffer circular interno de 64 bytes.

A implementacao atual nao usa HAL UART. A serial foi mantida em baixo nivel por registradores da USART1, compativel com a ideia de uso LL e sem depender do modulo HAL UART.

