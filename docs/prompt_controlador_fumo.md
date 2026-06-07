# Prompt — Controlador de Secagem de Fumo (STM32)

Você é um engenheiro de firmware STM32 experiente. Siga rigorosamente o `PERFIL_AGENT_PROGRAMADOR_STM32.md` para organização e estilo de código.

---

## CONTEXTO DO PROJETO

Com base nos defines de pinos existentes no projeto:

1. Leia o arquivo `docs/resumo_funcional.md` para entender o produto que utiliza este hardware
2. Mantenha no código a lógica existente de **varredura de display**, **leitura de botões**, e os **pinos de saída a relé `SD_SAIDA1` e `SD_SAIDA2`**

---

## HARDWARE — MAPEAMENTO FUNCIONAL

| Recurso | Detalhe |
|---|---|
| `EA_TEMPERATURA` + `EA_TEMP_REF_GND` | Sensor LM35. Temperatura calculada pela diferença ADC entre os dois pinos |
| `EA_UMIDADE` | Sensor HIH3051 analógico — 0,7575V a 3,9375V = 0% a 100% UR. Parâmetros `ZUR = 6950` (zero) e `GUR = 314` (ganho/multiplicador) salvos em Flash/EEPROM para permitir troca de sensor |
| Analógica 5V | Divisor resistivo 1kΩ + 1k8Ω para GND |
| Analógica 12V | Divisor resistivo 10kΩ + 3k3Ω para GND |
| USART1 | Módulo Bluetooth HM-10 para configuração e leitura de dados |
| RTC | Cristal interno 32.768 kHz — usar RTC interno do STM32 |
| EEPROM externa | 24AA256 — armazenamento do datalogger |
| BUZZER | Alarme de temperatura alta/baixa e outros alertas futuros |

---

## FUNCIONAMENTO DO PRODUTO

Sistema controlador de temperatura para **secagem de fumo (tabaco)**:

- `SD_SAIDA1` → saída do **soprador**: ao acionar, aumenta a ventilação da fornalha, elevando a temperatura
- **Datalogger**: registra temperatura, umidade, status de energia e status do sistema a cada período configurável
- Todos os parâmetros de configuração são ajustáveis via **menu no display** e via **serial Bluetooth**
- O datalogger pode ser resetado via menu e via serial

---

## PROTOCOLO SERIAL (ASCII — padrão industrial)

Estrutura de cada pacote:
```
[STX] [CMD_ID] [TAM] [DADO1,DADO2,...,DADON] [CRC16]
```

Funcionalidades obrigatórias do protocolo:

- Configurar todas as variáveis de configuração da aplicação
- Pacote de **dados em tempo real** (temperatura, umidade, saídas, tensões)
- Leitura do datalogger de forma **assíncrona**: pacotes informam índice atual e tamanho total
- Busca de registros **a partir de índice X** recebido via serial
- Protocolo deve ser **completamente documentado** no `docs/aplicacao.md`

---

## INTERFACE — MENU E BOTÕES

- Entrar no menu: **segurar o botão RELÓGIO (CONFIRMA/ENTER) por 3 segundos** — igual ao comportamento atual
- **Turbo no incremento** de valores ao segurar botão — manter comportamento já existente
- Menu de diagnóstico oculto: **pressionar o botão "+" 5 vezes seguidas** → exibe tela com tensões 5V e 12V medidas
- Tela principal exibe: **temperatura** e **umidade**

---

## TAREFA

1. **Primeiro**, crie o arquivo `docs/aplicacao.md` com toda a especificação do que o sistema irá fazer: entradas, saídas, lógica de controle, protocolo serial (com tabela de comandos), estrutura do datalogger, parâmetros configuráveis e fluxo do menu

2. **Depois**, aguarde aprovação para iniciar a implementação em **etapas particionadas com checkpoints** de teste, começando pelo checkpoint inicial:

   - **Checkpoint 1**: tela principal mostrando temperatura e umidade; menu de diagnóstico oculto (5× botão "+") exibindo tensões 5V e 12V
