/*
 * protocol.h — Protocolo serial "Ummi Manager" (frames '*', CRC XOR).
 *
 * Reintroduz o protocolo do firmware PIC original sobre USART1.
 * Layout: '*' + comando + dados (NBCR por faixa do comando) + CRC(XOR8).
 * ACK de escrita = 'F'. Download por blocos de 9 bytes com handshake '#'
 * e terminador "FIM" + CRC.
 */
#ifndef INC_PROTOCOL_H_
#define INC_PROTOCOL_H_

#include <stdint.h>

/* Baud do enlace (legado Ummi Manager = 9600 8N1). */
#define UMMI_BAUD 9600U

void protocol_init(void);
void protocol_process(void); /* chamar no laco principal */

/* Ganchos chamados pela infraestrutura (stm32f4xx_it.c / SysTick). */
void serial_usart1_irq_handler(void);
void serial_tick_1ms(void);

#endif /* INC_PROTOCOL_H_ */
