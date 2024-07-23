#ifndef _UART_H
#define _UART_H

#include "stdint.h"

#define TIMEOUT_COUNT 1500000

void uart_setup();
uint8_t Status_check(void);

uint8_t UART_writeBuffer(uint8_t *pData, uint8_t ui8Cnt);
void UART_readBuffer(uint8_t *pData, uint8_t ui8Cnt);
void UART_writeBuf_only(uint8_t *pData, uint8_t ui8Cnt);

#endif
