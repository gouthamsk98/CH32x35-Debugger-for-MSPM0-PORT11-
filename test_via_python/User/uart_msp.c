#include <uart_msp.h>
#include "ch32x035_usbfs_device.h"

#include "debug.h"
#define print_endp    DEF_UEP6

void uart_setup(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure = {0};
  USART_InitTypeDef USART_InitStructure = {0};
 
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

  /* USART2 TX-->A.2   RX-->A.3 */
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_Init(GPIOA, &GPIO_InitStructure);
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

  USART_InitStructure.USART_BaudRate = 9600;
  USART_InitStructure.USART_WordLength = USART_WordLength_8b;
  USART_InitStructure.USART_StopBits = USART_StopBits_1;
  USART_InitStructure.USART_Parity = USART_Parity_No;
  USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

  USART_Init(USART2, &USART_InitStructure);
  USART_Cmd(USART2, ENABLE);

}

uint8_t UART_writeBuffer(uint8_t *pData, uint8_t ui8Cnt)
{
  uint8_t res;

  while (ui8Cnt--) {
    USART_SendData(USART2, *pData++);
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) /* waiting for sending finish */
    {
    }
  }
  while(USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == RESET)
  {
  }
  res = USART_ReceiveData(USART2);
  return res;
}

void UART_writeBuf_only(uint8_t *pData, uint8_t ui8Cnt)
{
  while (ui8Cnt--) {
    USART_SendData(USART2, *pData++);
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) /* waiting for sending finish */
    {
    }
  }
}

void UART_readBuffer(uint8_t *pData, uint8_t ui8Cnt)
{
  while (ui8Cnt-- > 0) {
    while(USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == RESET)
    {
    }
    *pData = USART_ReceiveData(USART2);
    pData++;
  }
}

//uint8_t test_d;

uint8_t Status_check(void)
{
  uint8_t res = 0x99;
  /* Wait until all bytes have been transmitted and the TX FIFO is empty */
  USART_SendData(USART2, 0xBB);
  while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET) /* waiting for sending finish */
  {
  }
  /* Wait until all bytes have been transmitted and the TX FIFO is empty */
  while(USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == RESET)
  {
  }
  res = USART_ReceiveData(USART2);
  return res;
}


