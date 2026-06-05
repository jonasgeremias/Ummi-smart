/***************************************************************************************
 * SISTEMA CONTADOR DE PECAS UMMI - PIC16F877A
 *
 * Versao: 1.0 (R1.0)
 * Data: 06/11/2025
 * Autor: Lucas Guessi Placido
 *
 * =====================================================================================
 * OBJETIVO DO SISTEMA:
 * =====================================================================================
 * Sistema de contagem de pecas com inspecao obrigatoria para controle de qualidade.
 * Monitora a producao atraves de sensor indutivo e controla alertas visuais e sonoros
 * quando os setpoints de inspecao sao atingidos, garantindo verificacao periodica.
 *
 * =====================================================================================
 * FUNCIONAMENTO DETALHADO:
 * =====================================================================================
 *
 * 1. CONTAGEM DE PECAS (Modo Operacao Normal)
 *    -----------------------------------------------------------------------------
 *    a) Deteccao:
 *       - Sensor indutivo (PIN_B0) detecta peca passando
 *       - Interrupcao externa (INT_EXT) na borda de descida
 *       - Incrementa variavel 'contador' (0 a 999.999)
 *       - Overflow: ao atingir 999.999, proximo valor = 0
 *       - Protecao: ignora pulsos menores que 250ms (anti-bounce)
 *
 *    b) Armazenamento:
 *       - Grava 'contador' na EEPROM a cada incremento (com timeout de 10 ciclos)
 *       - Sistema de redundancia tripla (3 enderecos por variavel)
 *       - Indicacao visual: ponto decimal no display da unidade pisca durante gravacao
 *       - Persistencia: mantem contagem apos queda de energia
 *
 *    c) Visualizacao:
 *       - Display mostra valor atual do 'contador' continuamente
 *       - Formato: 6 digitos (exemplo: 000123 = 123 pecas)
 *
 * 2. SISTEMA DE INSPECAO E CONTROLE DE QUALIDADE
 *    -----------------------------------------------------------------------------
 *    a) Registro de Inspecao (Sensor Gabarito):
 *       - Operador aciona sensor gabarito (PIN_A0) apos inspecionar pecas
 *       - Sistema captura valor atual do 'contador'
 *       - Salva em 'ultima_conferencia' na EEPROM
 *       - Esta marca serve como referencia para calcular proxima inspecao
 *
 *    b) Calculo de Diferenca:
 *       - Sistema calcula continuamente: diferenca = contador - ultima_conferencia
 *       - Tratamento de overflow: se contador < ultima_conferencia, considera ciclo
 *       - Formula overflow: diferenca = (999999 - ultima_conferencia) + contador
 *
 *    c) Acionamento de Alertas (baseado na diferenca):
 *
 *       CONDICAO 1: Se (diferenca > setpoint_inspecao)
 *       -> Liga SAIDA_1 (PIN_A2)
 *       -> Indica: "Atencao, inspecao recomendada em breve"
 *
 *       CONDICAO 2: Se (diferenca > setpoint_obriga_inspecao)
 *       -> Liga SAIDA_2 (PIN_A5)
 *       -> Liga BUZZER (PIN_B7) em modo ciclico:
 *          * 1 segundo ligado (20 ciclos x 50ms)
 *          * 9 segundos desligado (180 ciclos x 50ms)
 *          * Ciclo se repete continuamente
 *       -> Indica: "OBRIGATORIO realizar inspecao imediatamente"
 *
 *    d) Reset de Alertas:
 *       - Ao acionar sensor gabarito novamente (nova inspecao):
 *         * Atualiza 'ultima_conferencia' = contador atual
 *         * diferenca volta a zero
 *         * SAIDA_1 desliga
 *         * SAIDA_2 desliga
 *         * BUZZER desliga
 *         * Contador continua contando normalmente
 *
 * 3. CONFIGURACAO DE SETPOINTS (Navegacao por Teclas)
 *    -----------------------------------------------------------------------------
 *    TECLA RELOGIO (PIN_B3) - Navegacao entre modos:
 *
 *    a) No MODO 0 (Contagem normal):
 *       - Pressionar e segurar TAC_RELOGIO por 3 segundos (60 ciclos x 50ms)
 *       - Sistema entra no MODO 1 (Configurar Setpoint Inspecao)
 *       - Display exibe "SEt InS" por 1 segundo
 *       - Depois exibe valor atual do setpoint_inspecao
 *
 *    b) No MODO 1 (Configurar Setpoint Inspecao):
 *       - TAC_MAX: incrementa setpoint_inspecao (+1, +10, +20, +100 conforme tempo pressionado)
 *       - TAC_MIN: decrementa setpoint_inspecao (-1, -10, -20, -100 conforme tempo pressionado)
 *       - VALIDACAO: setpoint_inspecao NAO pode ser maior que setpoint_obriga_inspecao
 *       - Pressionar TAC_RELOGIO brevemente (>250ms): salva e vai para MODO 2
 *       - Display exibe "SEt Obr" por 1 segundo
 *
 *    c) No MODO 2 (Configurar Setpoint Obriga Inspecao):
 *       - TAC_MAX: incrementa setpoint_obriga_inspecao (+1, +10, +20, +100)
 *       - TAC_MIN: decrementa setpoint_obriga_inspecao (-1, -10, -20, -100)
 *       - VALIDACAO: setpoint_obriga_inspecao NAO pode ser menor que setpoint_inspecao
 *       - Pressionar TAC_RELOGIO brevemente: salva e volta para MODO 0
 *       - Display exibe "  Ok  " por 1.5 segundos
 *       - Sistema volta ao modo de contagem normal
 *
 * 4. FUNCOES ESPECIAIS DA INTERFACE
 *    -----------------------------------------------------------------------------
 *    a) CONSULTA ULTIMA INSPECAO (apenas no MODO 0):
 *       - Condicao: Pressionar e segurar TAC_MAX + TAC_MIN solto
 *       - Display mostra valor de 'ultima_conferencia' ao inves do contador
 *       - Permite operador verificar quando foi feita ultima inspecao
 *       - Ao soltar TAC_MAX: display volta a mostrar contador atual
 *
 *    b) RESET COMPLETO DO CONTADOR (apenas no MODO 0):
 *       - Condicao: Pressionar TAC_MAX + TAC_MIN SIMULTANEAMENTE por 10 segundos
 *       - Sistema detecta 200 ciclos (200 x 50ms = 10s) com ambas teclas pressionadas
 *       - Executa:
 *         * contador = 0
 *         * ultima_conferencia = 0
 *         * Salva ambos na EEPROM
 *         * Display exibe "---rSt" por 1 segundo
 *         * Volta ao modo contagem normal exibindo 000000
 *       - Uso: Reset de producao, novo lote, novo turno, etc.
 *
 * 5. INDICADORES VISUAIS E FEEDBACK
 *    -----------------------------------------------------------------------------
 *    - Ponto decimal display unidade: pisca durante gravacao EEPROM
 *    - Splash inicial: "bE1001" exibido por 2s ao ligar equipamento
 *    - Mensagens de configuracao: "SEt InS", "SEt Obr", "Ok"
 *    - Mensagem de reset: "---rSt"
 *    - Display multiplexado a 1ms garante visualizacao estavel
 *
 * 6. PROTECOES E VALIDACOES
 *    -----------------------------------------------------------------------------
 *    - Debounce de teclas: 5ms (ignora ruidos e bouncing)
 *    - Debounce sensor gabarito: 5 ciclos de varredura
 *    - Protecao pulso contagem: ignora pulsos < 250ms (4 Hz max)
 *    - Validacao setpoints: setpoint_inspecao <= setpoint_obriga_inspecao (sempre)
 *    - Limites de contagem: 0 a 999.999 com overflow para 0
 *    - EEPROM redundante: 3 copias por dado, validacao por votacao
 *    - Watchdog timer: 2.3s (previne travamento do sistema)
 *    - Protecao modo invalido: auto-correcao para modo 0
 *
 * =====================================================================================
 * HARDWARE:
 * =====================================================================================
 * - Microcontrolador: PIC16F877A @ 4MHz
 * - Display: 6x 7-segmentos multiplexados (PORTD + controle PORTC/PORTE)
 * - Entradas:
 *   * PIN_B0: Sensor indutivo (interrupcao externa)
 *   * PIN_A0: Sensor de gabarito (inspecao)
 *   * PIN_B3: TAC_RELOGIO
 *   * PIN_B4: TAC_MIN
 *   * PIN_B5: TAC_MAX
 * - Saidas:
 *   * PIN_A2: SAIDA_1 (Alerta nivel 1)
 *   * PIN_A5: SAIDA_2 (Alerta nivel 2)
 *   * PIN_B7: BUZZER (Alarme sonoro)
 * - Memoria: EEPROM com redundancia tripla
 *
 * =====================================================================================
 * TIMERS E INTERRUPCOES:
 * =====================================================================================
 * - INT_EXT: Deteccao de pecas (borda de descida)
 * - TIMER1: Base de tempo 50ms (controle de timeouts)
 * - TIMER2: Varredura de displays 1ms (multiplexacao)
 * - Prioridade: INT_EXT > INT_TIMER1 > INT_TIMER2
 *
 * =====================================================================================
 * NOTAS DE VERSAO:
 * =====================================================================================
 * v1.0 (001):
 * - Implementacao inicial
 * - Sistema de contagem com persistencia
 * - Controle de inspecoes e alertas
 * - Interface de configuracao de setpoints
 * - Sistema de buzzer ciclico
 * - Reset de contador via teclas
 * - Consulta de ultima inspecao
 * - Validacoes de limites entre setpoints
 *
 ***************************************************************************************/
#include <16F877A.h>
#device ADC = 10
#device WRITE_EEPROM = NOINT
#zero_ram

#FUSES WDT   // Watch Dog Timer
#FUSES PUT   // Power Up Timer
#FUSES NOLVP // No low voltage prgming, B3(PIC16) or B5(PIC18) used for I/O

#use delay(crystal = 4MHz)

#priority INT_EXT, INT_TIMER1, INT_TIMER2

// Versao do firmware
#define VERSAO_FIRMWARE 001

byte const tabela[] =
    {
        //  87654321
        0b00111111, //  0
        0b00000110, //  1
        0b01011011, //  2
        0b01001111, //  3
        0b01100110, //  4
        0b01101101, //  5
        0b01111101, //  6
        0b00000111, //  7
        0b01111111, //  8
        0b01101111, //  9
        0b01110111, // 10 = A
        0b01111100, // 11 = b
        0b00111001, // 12 = C
        0b01011110, // 13 = d
        0b01111001, // 14 = E
        0b01110001, // 15 = F
        0b01101111, // 16 = G
        0b01110110, // 17 = H
        0b00000100, // 18 = I
        0b00011110, // 19 = J
        0b00111000, // 20 = L
        0b01010100, // 21 = N
        0b01110011, // 22 = P
        0b01100111, // 23 = q
        0b01010000, // 24 = r
        0b01101101, // 25 = S
        0b01111000, // 26 = t
        0b00111110, // 27 = U
        0b00000000, // 28 = desligado
        0b01000000, // 29 = "-"
        0b00111111, // 30 = k
        0b00011100  // 31 = U
};

int8 unidade, dezena, centena, milhar, dezena_milhar, centena_milhar = 0;

// Macro para leituras de entradas
#define SENSOR_GABARITO input(PIN_A0)
#define SENSOR_INDUTIVO input(PIN_B0)

#define TAC_RELOGIO input(PIN_B3)
#define TAC_MIN input(PIN_B4)
#define TAC_MAX input(PIN_B5)

// Macros para leituras de Saidas
#define SAIDA_1 PIN_A2
#define SAIDA_2 PIN_A5

#define SEGMENTO_A PIN_D0
#define SEGMENTO_B PIN_D1
#define SEGMENTO_C PIN_D2
#define SEGMENTO_D PIN_D3
#define SEGMENTO_E PIN_D4
#define SEGMENTO_F PIN_D5
#define SEGMENTO_G PIN_D6
#define SEGMENTO_DP PIN_D7

#define LIGA_DISPLAY_UNIDADE PIN_C0
#define LIGA_DISPLAY_DEZENA PIN_C1
#define LIGA_DISPLAY_CENTENA PIN_C2
#define LIGA_DISPLAY_MILHAR PIN_E0
#define LIGA_DISPLAY_DEZENA_MILHAR PIN_E1
#define LIGA_DISPLAY_CENTENA_MILHAR PIN_E2

#define BUZZER PIN_B7

// outras definicoes
#define TEMPO_DEBOUNCE_CONTADOR 3
#define TAC_TEMPO_LONG_PRESS 60 // exemplo: 60 x 50ms = 3 segundos
#define TAC_TEMPO_PULSO_MIN 5   // exemplo: 15 x 50ms
#define TAC_TEMPO_PULSO_MAX 15  // exemplo: 15 x 50ms
#define TAC_TEMPO_RESET 200     // 200 x 50ms = 10 segundos

// biblioteca
#include <_eeprom_init.c>

// Prototipos de funcoes
void grava_eeprom_int8(int16 posicao, int8 valor);
int8 le_eeprom_int8(int16 posicao);
void grava_eeprom_int32(int16 posicao, int32 valor);
int32 le_eeprom_int32(int16 posicao);
void trata_tac_relogio_press();
void atualiza_display_int32(int32 x);
void splash_screen(int8 timeout, int8 d5, int8 d4, int8 d3, int8 d2, int8 d1, int8 d0);
void modo_contagem();
void modo_setpoint_inspecao();

// Variaveis globais
unsigned int32 contador = 0;
unsigned int32 ultima_contagem = 0;
int32 setpoint_inspecao = 0;
int32 setpoint_obriga_inspecao = 0;
unsigned int32 ultima_conferencia = 0;
int8 modo = 0;
int8 atualiza_display_timeout = 0;
//!int8 gravar_contador_timeout = 0;
int8 liga_ponto_timeout = 0;
int8 digito_ponto = 0;
int16 buzzer_periodo_timeout = 0; // Contador de 10 segundos (200 x 50ms)
int8 buzzer_ligado_timeout = 0;   // Contador de 1 segundo (20 x 50ms)
int16 reset_contador_timeout = 0; // Contador para reset (MAX + MIN simultaneos)
int8 contador_pulso_timeout = 0;  // Protecao contra pulsos rapidos (<250ms)

//  TAC RELOGIO
int1 tac_relogio_puro = 0;
int16 tac_relogio_timeout = 0;

//  TAC MIN
int1 tac_min_puro = 0;
int1 tac_min_press = 0;
int16 tac_min_puro_timeout = 5;
int16 tac_min_timeout = 0;
int8 tac_min_turbo = 0;

//  TAC MAX
int1 tac_max_puro = 0;
int1 tac_max_press = 0;
int16 tac_max_puro_timeout = 5;
int16 tac_max_timeout = 0; // usando
int8 tac_max_turbo = 0;

int1 sensor_gabarito_puro = 0;
int1 sensor_gabarito_press = 0;
int1 trava_sensor_gabarito = 0;
int16 sensor_gabarito_puro_timeout = 5;
int8 indica_modo_alterado_timeout = 0;

// Splash screen
int8 splashscreen_timeout = 0;
int8 splash_digito[6];

#define TAC_TURBO_LEVEL1 350
#define TAC_TURBO_LEVEL2 200
#define TAC_TURBO_LEVEL3 100
int32 setpoint_temp = 0;

// EEMPROM
void grava_eeprom_int8(int16 posicao, int8 valor)
{
   write_eeprom(posicao + POSEE1, valor);
   write_eeprom(posicao + POSEE2, valor);
   write_eeprom(posicao + POSEE3, valor);
}

int8 le_eeprom_int8(int16 posicao)
{
   int8 p1, p2, p3;

   for (int8 i = 0; i < 3; i++)
   {
      p1 = read_eeprom(posicao + POSEE1);
      p2 = read_eeprom(posicao + POSEE2);
      p3 = read_eeprom(posicao + POSEE3);

      if ((p1 == p2) && (p1 == p3))
      {
         return p1;
      }
      else if (p2 == p3)
      {
         write_eeprom(posicao + POSEE1, p2);
         return p2;
      }
      else if (p1 == p3)
      {
         write_eeprom(posicao + POSEE2, p1);
         return p1;
      }
      else if (p1 == p2)
      {
         write_eeprom(posicao + POSEE3, p1);
         return p1;
      }

      delay_ms(5);
   }

   return 0;
}

void grava_eeprom_int32(int16 posicao, int32 valor)
{
   grava_eeprom_int8(posicao, (int8)(valor & 0xFF));
   grava_eeprom_int8(posicao + 1, (int8)((valor >> 8) & 0xFF));
   grava_eeprom_int8(posicao + 2, (int8)((valor >> 16) & 0xFF));
   // grava_eeprom_int8(posicao + 3, (int8)((valor >> 24) & 0xFF));
}

int32 le_eeprom_int32(int16 posicao)
{
   int32 resultado = 0;

   resultado = (int32)le_eeprom_int8(posicao);
   resultado |= ((int32)le_eeprom_int8(posicao + 1)) << 8;
   resultado |= ((int32)le_eeprom_int8(posicao + 2)) << 16;
   // resultado |= ((int32)le_eeprom_int8(posicao + 3)) << 24;

   return resultado;
}

void trata_tac_relogio_press()
{
   static int1 trava = 0;

   // modo = 0;
   // grava_eeprom_int8(POSEE_MODO, modo);
   // indica_modo_alterado_timeout = 10;

   if (modo == 0)
   {
      if (trava || !tac_relogio_puro || tac_relogio_timeout < TAC_TEMPO_LONG_PRESS)
      {
         if (!tac_relogio_puro)
            trava = 0;
         return;
      }
   }
   else
   {
      if (trava || !tac_relogio_puro || tac_relogio_timeout < TAC_TEMPO_PULSO_MIN)
      {
         if (!tac_relogio_puro)
            trava = 0;
         return;
      }
   }

   trava = 1;

   if (modo == 0)
   {
      // Carrega setpoint atual e vai para modo 1 (editar setpoint inspecao)
      setpoint_temp = setpoint_inspecao;
      modo = 1;

      // Exibe "SEt InS" (S=25, E=14, t=26, blank=28, 1=I, n=21, S=25)
      splash_screen(20, 25, 14, 26, 1, 21, 5);
   }
   else if (modo == 1) // sai do setpoint e vai pro obriga
   {
      // Salva setpoint_inspecao e vai para modo 2 (editar setpoint obriga)
      setpoint_inspecao = setpoint_temp;
      grava_eeprom_int32(POSEE_SETPOINT, setpoint_inspecao);

      setpoint_temp = setpoint_obriga_inspecao;
      modo = 2;

      // Exibe "SEt Obr" (S=25, E=14, t=26, blank=28, O=0, b=11, r=24)
      splash_screen(20, 25, 14, 26, 0, 11, 24);
   }
   else if (modo == 2)
   {
      // Salva setpoint_obriga_inspecao e volta para tela inicial
      setpoint_obriga_inspecao = setpoint_temp;
      grava_eeprom_int32(POSEE_SETPOINT_OBRIGA, setpoint_obriga_inspecao);

      // Volta pra tela inicial
      modo = 0;

      splash_screen(20, 29, 5, 10, 31, 14, 29);
   }

   indica_modo_alterado_timeout = 10;
}

void trata_reset_contador()
{
   static int1 trava = 0;

   // Verifica se ambas as teclas estao pressionadas por tempo suficiente
   if (tac_min_press && tac_max_press && reset_contador_timeout >= TAC_TEMPO_RESET)
   {
      if (!trava)
      {
         trava = 1;

         // Reseta contador e ultima_conferencia
         contador = 0;
         ultima_conferencia = 0;

         // Exibe mensagem "--- rSt" (r=24, S=25, t=26, "-"=29)
         splash_screen(20, 29, 29, 29, 24, 25, 26);

         // Grava na EEPROM
         grava_eeprom_int32(POSEE_VALOR_CONTADOR, contador);
         grava_eeprom_int32(POSEE_VALOR_ULTIMA_CONFERENCIA, ultima_conferencia);

         // Desliga todas as saidas e buzzer
         output_low(SAIDA_1);
         output_low(SAIDA_2);
         output_low(BUZZER);
         buzzer_periodo_timeout = 0;
         buzzer_ligado_timeout = 0;

         // Forca atualizacao do display
         atualiza_display_timeout = 0;
      }
   }
   else
   {
      trava = 0;
   }
}

void atualiza_display_int32(int32 x)
{

   if (!atualiza_display_timeout)
   {
      atualiza_display_timeout = 2;

      centena_milhar = (x / 100000) % 10;
      dezena_milhar = (x / 10000) % 10;
      milhar = (x / 1000) % 10;
      centena = (x / 100) % 10;
      dezena = (x / 10) % 10;
      unidade = x % 10;
   }
}

void splash_screen(int8 timeout, int8 d5, int8 d4, int8 d3, int8 d2, int8 d1, int8 d0)
{
   splashscreen_timeout = timeout;
   splash_digito[0] = d0; // unidade
   splash_digito[1] = d1; // dezena
   splash_digito[2] = d2; // centena
   splash_digito[3] = d3; // milhar
   splash_digito[4] = d4; // dezena_milhar
   splash_digito[5] = d5; // centena_milhar
}

void modo_contagem()
{

   restart_wdt();
   // leitura_das_tacs();

   // Se MAX pressionado e MIN solto, mostra ultima_conferencia
   if (tac_max_press && !tac_min_press)
   {
      atualiza_display_int32(ultima_conferencia);
   }
   else
   {
      atualiza_display_int32(contador);
   }

   // Deteccao do SENSOR_GABARITO - verifica flag setada pela interrupcao
   // Protecao: so grava na borda de subida (transicao 0->1)
   if (sensor_gabarito_press && !trava_sensor_gabarito && ultima_conferencia != contador)
   {
      // Salva conferencia
      ultima_conferencia = contador;
      trava_sensor_gabarito = 1;
      // "InSPEC"
      splash_screen(20, 1, 21, 5, 22, 14, 12);
      grava_eeprom_int32(POSEE_VALOR_ULTIMA_CONFERENCIA, ultima_conferencia);
   }

   // Logica das saidas baseada na diferenca desde a ultima conferencia
   // Trata overflow: se contador < ultima_conferencia, houve overflow
   int32 diferenca;
   if (contador >= ultima_conferencia)
   {
      diferenca = contador - ultima_conferencia;
   }
   else
   {
      // Overflow ocorreu: calcula diferenca considerando o ciclo
      // Exemplo: ultima=999998, contador=2
      // Sequencia real: 999998 -> 999999 -> 0 -> 1 -> 2 = 5 pecas
      // Calculo: (999999 - 999998) + 2 + 2 = 1 + 2 + 2 = 5
      diferenca = (999999 - ultima_conferencia) + contador + 2;
   }

   if (diferenca >= setpoint_inspecao)
      output_high(SAIDA_1);
   else
      output_low(SAIDA_1);

   if (diferenca >= setpoint_obriga_inspecao)
   {
      output_high(SAIDA_2);

      // Controle do buzzer: aciona a cada 10 segundos por 1 segundo
      // Inicializa na primeira ativacao
      if (buzzer_periodo_timeout == 0 && buzzer_ligado_timeout == 0)
      {
         buzzer_ligado_timeout = 20;   // Toca imediatamente por 1 segundo
         buzzer_periodo_timeout = 200; // Depois espera 10 segundos
      }

      if (buzzer_ligado_timeout > 0)
      {
         output_high(BUZZER);
      }
      else
      {
         output_low(BUZZER);

         // Se o periodo de 10s terminou, religa o buzzer
         if (buzzer_periodo_timeout == 0)
         {
            buzzer_ligado_timeout = 20;   // 1 segundo (20 x 50ms)
            buzzer_periodo_timeout = 200; // 10 segundos (200 x 50ms)
         }
      }
   }
   else
   {
      output_low(SAIDA_2);
      output_low(BUZZER);
      buzzer_periodo_timeout = 0;
      buzzer_ligado_timeout = 0;
   }

   trata_tac_relogio_press();
   trata_reset_contador();
}

void modo_setpoint_inspecao()
{
   restart_wdt();
   // leitura_das_tacs();

   /****************************************************************************
    * Leitura da tac max
    ***************************************************************************/
   if (!tac_max_timeout && tac_max_press)
   {
      if (tac_max_turbo >= 30)
         setpoint_temp += 100;
      else if (tac_max_turbo >= 20)
         setpoint_temp += 20;
      else if (tac_max_turbo >= 10)
         setpoint_temp += 10;

      else
         setpoint_temp += 1;

      if (setpoint_temp > 999999)
         setpoint_temp = 999999;

      // Validacao: se modo 1, nao pode ser maior que setpoint_obriga
      if (modo == 1 && setpoint_temp > setpoint_obriga_inspecao)
         setpoint_temp = setpoint_obriga_inspecao;

      // recarrega turbo
      if (tac_max_turbo < 30)
         tac_max_turbo++;

      if (tac_max_turbo >= 5)
         tac_max_timeout = TAC_TURBO_LEVEL2;
      else
         tac_max_timeout = TAC_TURBO_LEVEL1;

      // Forca atualizacao display
      atualiza_display_timeout = 0;
   }
   else if (!tac_max_press)
   {
      tac_max_timeout = TAC_TURBO_LEVEL3;
      tac_max_turbo = 0;
   }

   /****************************************************************************
    * Leitura da tac min
    ****************************************************************************/

   if (!tac_min_timeout && tac_min_press)
   {
      if (tac_min_turbo >= 30)
         setpoint_temp -= 100;
      else if (tac_min_turbo >= 20)
         setpoint_temp -= 20;
      else if (tac_min_turbo >= 10)
         setpoint_temp -= 10;

      else
         setpoint_temp -= 1;

      if (setpoint_temp > 999999)
         setpoint_temp = 0;

      // Validacao: se modo 2, nao pode ser menor que setpoint_inspecao
      if (modo == 2 && setpoint_temp < setpoint_inspecao)
         setpoint_temp = setpoint_inspecao;

      // recarrega turbo
      if (tac_min_turbo < 30)
         tac_min_turbo++;

      if (tac_min_turbo >= 5)
         tac_min_timeout = TAC_TURBO_LEVEL2;
      else
         tac_min_timeout = TAC_TURBO_LEVEL1;

      // Forca atualizacao display
      atualiza_display_timeout = 0;
   }
   else if (!tac_min_press)
   {
      tac_min_timeout = TAC_TURBO_LEVEL3;
      tac_min_turbo = 0;
   }

   atualiza_display_int32(setpoint_temp);
   trata_tac_relogio_press();
}

// Interrupcao externa - Sensor indutivo
#INT_EXT
void EXT_isr(void)
{
   // Protecao contra pulsos rapidos (menores que 250ms)
   // 250ms = 5 ciclos de 50ms
   if (contador_pulso_timeout > 0)
   {
      clear_interrupt(INT_EXT);
      return; // Ignora pulso se timeout ainda ativo
   }

   contador++;

   // Limita contador em 999999, overflow para 0
   if (contador > 999999)
      contador = 0;

   if (modo == 0)
   {
      atualiza_display_timeout = 0;
   }

   // gravar_contador_timeout = 0;
   contador_pulso_timeout = 5; // Bloqueia novos pulsos por 250ms (5 x 50ms)
   clear_interrupt(INT_EXT);
}

// Timer1 ISR - 50 ms tick

int8 display = 0;
#INT_TIMER2
void TIMER2_isr(void)
{
   int8 byte_display;
   /****************************************************************************
    * Leitura do SENSOR_GABARITO
    ****************************************************************************/
   sensor_gabarito_puro = SENSOR_GABARITO;

   if (sensor_gabarito_puro && !sensor_gabarito_press)
   {
      if (sensor_gabarito_puro_timeout)
         sensor_gabarito_puro_timeout--;
      else
         sensor_gabarito_press = 1;
   }
   else if (!sensor_gabarito_puro)
   {
      sensor_gabarito_puro_timeout = 5;
      trava_sensor_gabarito = 0;
      sensor_gabarito_press = 0;
   }

   /****************************************************************************
    * Leitura da tac max
    ****************************************************************************/
   // Leitura das tacs - bounce
   if (tac_max_puro)
   {
      if (tac_max_puro_timeout)
         tac_max_puro_timeout--;
      else
         tac_max_press = 1;
   }
   else
   {
      tac_max_puro_timeout = 5;
      tac_max_press = 0;
   }

   // Usamos par ao turbo no loop
   if (tac_max_timeout > 0)
      tac_max_timeout--;

   /****************************************************************************
    * Leitura da tac min
    ****************************************************************************/
   // Leitura das tacs - bounce
   if (tac_min_puro)
   {
      if (tac_min_puro_timeout)
         tac_min_puro_timeout--;
      else
         tac_min_press = 1;
   }
   else
   {
      tac_min_puro_timeout = 5;
      tac_min_press = 0;
   }

   if (tac_min_timeout > 0)
      tac_min_timeout--;

   // Varredura de display - verifica se ha splash screen ativo
   if (display == 0)
   {
      if (splashscreen_timeout > 0)
         byte_display = tabela[splash_digito[0]];
      else
         byte_display = tabela[unidade];

      // Liga ponto decimal se timeout ativo e bit correspondente setado
      if (liga_ponto_timeout && (digito_ponto & 0b00000001))
         byte_display |= 0b10000000;

      output_d(byte_display);
      output_high(LIGA_DISPLAY_UNIDADE);
   }
   else if (display == 1)
   {
      output_low(LIGA_DISPLAY_UNIDADE);

      if (splashscreen_timeout > 0)
         byte_display = tabela[splash_digito[1]];
      else
         byte_display = tabela[dezena];

      // Liga ponto decimal se timeout ativo e bit correspondente setado
      if (liga_ponto_timeout && (digito_ponto & 0b00000010))
         byte_display |= 0b10000000;

      output_d(byte_display);
      output_high(LIGA_DISPLAY_DEZENA);
   }
   else if (display == 2)
   {
      output_low(LIGA_DISPLAY_DEZENA);

      if (splashscreen_timeout > 0)
         byte_display = tabela[splash_digito[2]];
      else
         byte_display = tabela[centena];

      // Liga ponto decimal se timeout ativo e bit correspondente setado
      if (liga_ponto_timeout && (digito_ponto & 0b00000100))
         byte_display |= 0b10000000;

      output_d(byte_display);
      output_high(LIGA_DISPLAY_CENTENA);
   }
   else if (display == 3)
   {
      output_low(LIGA_DISPLAY_CENTENA);

      if (splashscreen_timeout > 0)
         byte_display = tabela[splash_digito[3]];
      else
         byte_display = tabela[milhar];

      // Liga ponto decimal se timeout ativo e bit correspondente setado
      if (liga_ponto_timeout && (digito_ponto & 0b00001000))
         byte_display |= 0b10000000;

      output_d(byte_display);
      output_high(LIGA_DISPLAY_MILHAR);
   }
   else if (display == 4)
   {
      output_low(LIGA_DISPLAY_MILHAR);

      if (splashscreen_timeout > 0)
         byte_display = tabela[splash_digito[4]];
      else
         byte_display = tabela[dezena_milhar];

      // Liga ponto decimal se timeout ativo e bit correspondente setado
      if (liga_ponto_timeout && (digito_ponto & 0b00010000))
         byte_display |= 0b10000000;

      output_d(byte_display);
      output_high(LIGA_DISPLAY_DEZENA_MILHAR);
   }
   else if (display == 5)
   {
      output_low(LIGA_DISPLAY_DEZENA_MILHAR);

      if (splashscreen_timeout > 0)
         byte_display = tabela[splash_digito[5]];
      else
         byte_display = tabela[centena_milhar];

      // Liga ponto decimal se timeout ativo e bit correspondente setado
      if (liga_ponto_timeout && (digito_ponto & 0b00100000))
         byte_display |= 0b10000000;

      output_d(byte_display);
      output_high(LIGA_DISPLAY_CENTENA_MILHAR);
   }
   else if (display == 6)
   {
      // Varredura tacs
      // Desliga todos os displays
      output_low(LIGA_DISPLAY_CENTENA_MILHAR);
      output_low(LIGA_DISPLAY_DEZENA_MILHAR);
      output_low(LIGA_DISPLAY_MILHAR);

      output_low(LIGA_DISPLAY_CENTENA);
      output_low(LIGA_DISPLAY_DEZENA);
      output_low(LIGA_DISPLAY_UNIDADE);
      output_d(0);

      tac_relogio_puro = !TAC_RELOGIO;
      tac_max_puro = !TAC_MAX;
      tac_min_puro = !TAC_MIN;
   }

   if (++display > 6)
      display = 0;
}

#INT_TIMER1
void TIMER1_isr(void)
{
   // Recarrega o timer para 50 ms
   set_timer1(65536 - 50000 + 6);

   // Controle do tempo da Tac relogio
   if (tac_relogio_puro && tac_relogio_timeout < 65535)
      tac_relogio_timeout++;
   else if (!tac_relogio_puro)
      tac_relogio_timeout = 0;

   // Decrementa timeout de indicacao de modo alterado
   if (indica_modo_alterado_timeout > 0)
      indica_modo_alterado_timeout--;

   // Decrementa timeout do splash screen
   if (splashscreen_timeout > 0)
      splashscreen_timeout--;

   if (atualiza_display_timeout > 0)
      atualiza_display_timeout--;

   // if (gravar_contador_timeout > 0)
   //    gravar_contador_timeout--;

   if (liga_ponto_timeout > 0)
      liga_ponto_timeout--;

   // Protecao contra pulsos rapidos na entrada de contagem
   if (contador_pulso_timeout > 0)
      contador_pulso_timeout--;

   // Controle dos timeouts do buzzer
   if (buzzer_periodo_timeout > 0)
      buzzer_periodo_timeout--;

   if (buzzer_ligado_timeout > 0)
      buzzer_ligado_timeout--;

   // Controle do timeout de reset (MAX + MIN simultaneo)
   if (tac_min_press && tac_max_press && reset_contador_timeout < 65535)
      reset_contador_timeout++;
   else
      reset_contador_timeout = 0;
}

// Programa principal
void main()
{
   set_tris_b(0b11111111);
   port_b_pullups(FALSE);
   set_tris_c(0b10000000);

   output_low(BUZZER);
   output_low(SAIDA_1);
   output_low(SAIDA_2);

   setup_wdt(WDT_2304MS);

   // Leitura das variaveis da EEPROM na inicializacao
   setpoint_inspecao = le_eeprom_int32(POSEE_SETPOINT);
   if (setpoint_inspecao > 999999)
      setpoint_inspecao = SETPOINT_DEFAULT;

   setpoint_obriga_inspecao = le_eeprom_int32(POSEE_SETPOINT_OBRIGA);
   if (setpoint_obriga_inspecao > 999999)
      setpoint_obriga_inspecao = SETPOINT_OBRIGA_DEFAULT;

   // Validacao critica: setpoint_inspecao deve ser <= setpoint_obriga_inspecao
   if (setpoint_inspecao > setpoint_obriga_inspecao)
   {
      // EEPROM corrompida ou configuracao invalida - restaura valores padrao
      setpoint_inspecao = SETPOINT_DEFAULT;
      setpoint_obriga_inspecao = SETPOINT_OBRIGA_DEFAULT;
   }

   contador = le_eeprom_int32(POSEE_VALOR_CONTADOR);
   if (contador > 999999)
      contador = VALOR_CONTADOR_DEFAULT;

   // contador = 100;

   ultima_conferencia = le_eeprom_int32(POSEE_VALOR_ULTIMA_CONFERENCIA);
   if (ultima_conferencia > 999999)
      ultima_conferencia = VALOR_ULTIMA_CONFERENCIA_DEFAULT;

   setup_timer_1(T1_INTERNAL | T1_DIV_BY_1); // Timer1: 65.536ms overflow max, reload 50ms na ISR

   // setup_timer_2(T2_DIV_BY_16, 20, 9); // Gera 3.024ms overflow, 1,5 ms interrupt
   // setup_timer_2(T2_DIV_BY_16, 20, 15);      // 336 us overflow, 5,0 ms interrupt
   setup_timer_2(T2_DIV_BY_16, 15, 1); // 1 ms interrupt @ 4 MHz
   ext_int_edge(H_TO_L);               // Interrupcao na borda de descida
   clear_interrupt(INT_EXT);
   enable_interrupts(INT_EXT);
   enable_interrupts(INT_TIMER1);
   enable_interrupts(INT_TIMER2);
   enable_interrupts(GLOBAL);

   // Splash screen inicial com versao do firmware "bE1001" (2 segundos)
   // Display: [b][E][1][0][0][1]
   // b=11, E=14, 1=1, 0=0, 0=0, 1=1
   // Parametros: timeout, d5, d4, d3, d2, d1, d0
   splash_screen(40, 11, 14, 1, 0, 0, 1);

   modo = 0;

   while (TRUE)
   {
      if (modo == 0)
      {
         modo_contagem();
      }
      else if (modo == 1)
      {
         modo_setpoint_inspecao();
      }
      else if (modo == 2)
      {
         modo_setpoint_inspecao();
      }
      else
      {
         // Protecao contra modo invalido - volta para modo de contagem
         modo = 0;
      }

      if ((/*!gravar_contador_timeout && */ (contador != ultima_contagem)) /*|| (contador - ultima_contagem >= 5)*/)
      {
         liga_ponto_timeout = 4;
         digito_ponto = 0b00000001;

         // Salva o contador na EEPROM a cada incremento
         grava_eeprom_int32(POSEE_VALOR_CONTADOR, contador);

         ultima_contagem = contador;
      }
   }
}
