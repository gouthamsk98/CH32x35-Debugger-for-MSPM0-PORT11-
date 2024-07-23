#include <uart_msp.h>
#include "bsl.h"
#include "ch32x035_usbfs_device.h"

#include "stdio.h"
#include "string.h"
#include "debug.h"

#define BSL_GPIO       GPIO_Pin_0
#define NRST_GPIO      GPIO_Pin_4

#define ERROR_LED      GPIO_Pin_1

#define GPIO_BSL_PORT  GPIOA

#define print_endp     DEF_UEP6
//*****************************************************************************
//
// ! BSL Entry Sequence
// ! Forces target to enter BSL mode
//
//*****************************************************************************

void Host_BSL_entry_sequence()
{
	GPIO_InitTypeDef GPIO_InitStructure = {0};

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = BSL_GPIO;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = NRST_GPIO;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = ERROR_LED;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_WriteBit(GPIOA, BSL_GPIO , 0);
    GPIO_WriteBit(GPIOA, NRST_GPIO, 0);
    Delay_Ms(100);

    GPIO_WriteBit(GPIOA, BSL_GPIO, 1);
    Delay_Ms(3400);

    GPIO_WriteBit(GPIOA, NRST_GPIO, 1);
    Delay_Ms(100);

    GPIO_WriteBit(GPIOA, BSL_GPIO, 0);

}

/*
 * Turn on the error LED
 */
void TurnOnErrorLED(void)
{
    GPIO_WriteBit(GPIOA, ERROR_LED, 1);
}

//*****************************************************************************
//
// ! Host_BSL_Connection
// ! Need to send first to build connection with target
//
//*****************************************************************************

BSL_error_t Host_BSL_Connection(void)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];  
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;
    uint32_t ui32CRC;

    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_CONNECTION;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    *(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;

    uart_ack = UART_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);

    if (uart_ack != uart_noError) {
        TurnOnErrorLED();
        bsl_err = eBSL_unknownError;
    }
    return (bsl_err);
}

//*****************************************************************************
// ! Host_BSL_GetID
// ! Need to send when build connection to get RAM BSL_RX_buffer size and other information
//
//*****************************************************************************
BSL_error_t Host_BSL_GetID(uint16_t *max_buf_size)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2]; 
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;
    uint32_t ui32CRC;

    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_GET_ID;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    *(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;

    // Write the packet to the target
    uart_ack = UART_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);
    if (uart_ack != uart_noError) {
        TurnOnErrorLED();
    }

    UART_readBuffer(BSL_RX_buffer, HDR_LEN_CMD_BYTES + ID_BACK + CRC_BYTES);
    uint16_t BSL_MAX_BUFFER_SIZE= 0 ;
    BSL_MAX_BUFFER_SIZE =
        *(uint16_t *) &BSL_RX_buffer[HDR_LEN_CMD_BYTES + ID_BACK - 14];

    *max_buf_size = BSL_MAX_BUFFER_SIZE;

    return (bsl_err);
}

//*****************************************************************************
// ! Unlock BSL for programming
// ! If first time, assume blank device.
// ! This will cause a mass erase and destroy previous password.
// ! When programming complete, issue BSL_readPassword to retrieve new one.
//
//*****************************************************************************
BSL_error_t Host_BSL_loadPassword(uint8_t *pPassword)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;
    uint32_t ui32CRC;

    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(PASSWORD_SIZE + CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_RX_PASSWORD;

    memcpy(&BSL_TX_buffer[4], pPassword, PASSWORD_SIZE);

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], PASSWORD_SIZE + CMD_BYTE);

    // Insert the CRC into the packet
    *(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES + PASSWORD_SIZE] = ui32CRC;

    // Write the packet to the target
    uart_ack = UART_writeBuffer(
        BSL_TX_buffer, HDR_LEN_CMD_BYTES + PASSWORD_SIZE + CRC_BYTES);
    if (uart_ack != uart_noError ) {
        TurnOnErrorLED();
    }
    bsl_err = Host_BSL_getResponse();

    return (bsl_err);
}

//*****************************************************************************
// ! Host_BSL_MassErase
// ! Need to do mess erase before write new image
//
//*****************************************************************************
BSL_error_t Host_BSL_MassErase(void)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;
    uint32_t ui32CRC;

    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_MASS_ERASE;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    *(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;

    // Write the packet to the target
    uart_ack = UART_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);
    if (uart_ack != uart_noError) {
        TurnOnErrorLED();
    }

    bsl_err = Host_BSL_getResponse();
    return (bsl_err);
}

//*****************************************************************************
//
// ! Host_BSL_writeMemory
// ! Writes memory section to target
//
//*****************************************************************************
BSL_error_t Host_BSL_writeMemory(
    uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;
    uint16_t ui16PayloadSize;
    uint16_t ui16PacketSize;
    uint32_t ui32CRC;
    uint32_t TargetAddress    = addr;
    uint16_t ui16DataLength   = len;

    // Add (1byte) command + (4 bytes)ADDRS = 5 bytes to the payload
    ui16PayloadSize = (CMD_BYTE + ADDRS_BYTES + ui16DataLength);

    BSL_TX_buffer[0] = PACKET_HEADER;
    BSL_TX_buffer[1] =
        LSB(ui16PayloadSize);  // typically 4 + MAX_PAYLOAD SIZE
    BSL_TX_buffer[2] = MSB(ui16PayloadSize);
    BSL_TX_buffer[3] = (uint8_t) CMD_PROGRAMDATA;
    *(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = TargetAddress;

    // Copy the data into the BSL_RX_buffer
    memcpy(&BSL_TX_buffer[HDR_LEN_CMD_BYTES + ADDRS_BYTES], data,
        ui16DataLength);

    // Calculate CRC on the PAYLOAD
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], ui16PayloadSize);

    // Calculate the packet length
    ui16PacketSize = HDR_LEN_CMD_BYTES + ADDRS_BYTES + ui16DataLength;

    // Insert the CRC into the packet at the end
    *(uint32_t *) &BSL_TX_buffer[ui16PacketSize] = ui32CRC;

    // Write the packet to the target
    uart_ack = UART_writeBuffer(BSL_TX_buffer, ui16PacketSize + CRC_BYTES);
    if (uart_ack != uart_noError) {
        TurnOnErrorLED();
    }
    // Check operation was complete
    bsl_err = Host_BSL_getResponse();
  
    return (bsl_err);
}


//*****************************************************************************
// ! Host_BSL_StartApp
// ! Start the new application
//
//*****************************************************************************
BSL_error_t Host_BSL_StartApp(void)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;
    uint32_t ui32CRC;

    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = LSB(CMD_BYTE);
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_START_APP;

    // Calculate CRC on the PAYLOAD (CMD + Data)
    ui32CRC = softwareCRC(&BSL_TX_buffer[3], CMD_BYTE);
    // Insert the CRC into the packet
    *(uint32_t *) &BSL_TX_buffer[HDR_LEN_CMD_BYTES] = ui32CRC;

    // Write the packet to the target
    uart_ack = UART_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + CRC_BYTES);
    if (uart_ack != uart_noError) {
        TurnOnErrorLED();
    }
    return (bsl_err);
}

//*****************************************************************************
//
// ! softwareCRC
// ! Can be used on MSP430 and non-MSP platforms
// ! This functions computes the 16-bit CRC (same as BSL on MSP target)
//
//*****************************************************************************
#define CRC32_POLY 0xEDB88320
uint32_t softwareCRC(const uint8_t *data, uint8_t length)
{
    uint32_t ii, jj, byte, crc, mask;
    ;

    crc = 0xFFFFFFFF;

    for (ii = 0; ii < length; ii++) {
        byte = data[ii];
        crc  = crc ^ byte;

        for (jj = 0; jj < 8; jj++) {
            mask = -(crc & 1);
            crc  = (crc >> 1) ^ (CRC32_POLY & mask);
        }
    }

    return crc;
}

//*****************************************************************************
//
// ! Host_BSL_getResponse
// ! For those function calls that don't return specific data.
// ! Returns errors.
//
//*****************************************************************************

BSL_error_t Host_BSL_getResponse(void)
{
    uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2];

    BSL_error_t bsl_err = eBSL_success;

    UART_readBuffer(BSL_RX_buffer, (HDR_LEN_CMD_BYTES + ACK_BYTE + CRC_BYTES));
    //   Get ACK value
    bsl_err = BSL_RX_buffer[HDR_LEN_CMD_BYTES + ACK_BYTE - 1];

    if(bsl_err != eBSL_success){
        TurnOnErrorLED();
    }
    //   Return ACK value
    return (bsl_err);
}


BSL_error_t Host_BSL_ReadMemory(uint32_t addr , uint32_t len , uint8_t *readBackData)
{ 

    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2];
    
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;
    uint32_t ui32CRC;
    uint32_t ui32DataLength = len;
    uint32_t TargetAddress = addr;

    BSL_TX_buffer[0] = (uint8_t) PACKET_HEADER;
    BSL_TX_buffer[1] = 0x09;
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = (uint8_t)CMD_READBACK_DATA;
    *(uint32_t *) &BSL_TX_buffer[4] = TargetAddress;
     

    *(uint32_t *) &BSL_TX_buffer[8] = ui32DataLength;

    ui32CRC = softwareCRC(&BSL_TX_buffer[3] , CMD_BYTE + ADDRS_BYTES + LEN_BYTES);
        
    *(uint32_t *) &BSL_TX_buffer[12] = ui32CRC;

    uart_ack = UART_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + 
        ADDRS_BYTES + LEN_BYTES + CRC_BYTES);    
    if (uart_ack != uart_noError) {
        TurnOnErrorLED();
    }

    UART_readBuffer(BSL_RX_buffer , HDR_LEN_CMD_BYTES + ui32DataLength + CRC_BYTES);
    memcpy(readBackData , BSL_RX_buffer + 4, ui32DataLength);

   return bsl_err;
}

BSL_error_t Host_BSL_Verify(uint32_t addr , uint32_t len , uint32_t *crc)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2];

    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;

    uint32_t ui32CRC;
    uint32_t ui32DataLength =  len;
    uint32_t TargetAddress =  addr;
    
    BSL_TX_buffer[0] = PACKET_HEADER;
    BSL_TX_buffer[1] = 0x09;
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_VERIFY;

    *(uint32_t *)&BSL_TX_buffer[4] = TargetAddress;
    *(uint32_t *)&BSL_TX_buffer[8] = ui32DataLength;

    ui32CRC = softwareCRC(&BSL_TX_buffer[3] , CMD_BYTE + ADDRS_BYTES + LEN_BYTES);

    *(uint32_t *)&BSL_TX_buffer[12] = ui32CRC;
    
    uart_ack = UART_writeBuffer(BSL_TX_buffer, HDR_LEN_CMD_BYTES + 
        ADDRS_BYTES + LEN_BYTES + CRC_BYTES);
    if (uart_ack != uart_noError) {
        TurnOnErrorLED();
    }

    UART_readBuffer(BSL_RX_buffer , HDR_LEN_CMD_BYTES + LEN_BYTES + CRC_BYTES);
    *crc = *(uint32_t *)&BSL_RX_buffer[4];

    return bsl_err;
}

BSL_error_t Host_BSL_RangeErase(uint32_t startaddr , uint32_t endaddr)
{
    uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
    BSL_error_t bsl_err = eBSL_success;
    uart_error_t uart_ack;

    uint32_t ui32StartAddress = startaddr;
    uint32_t ui32EndAddress = endaddr;
    uint32_t ui32CRC;

    BSL_TX_buffer[0] = PACKET_HEADER;
    BSL_TX_buffer[1] = 0x09;
    BSL_TX_buffer[2] = 0x00;
    BSL_TX_buffer[3] = CMD_FLASH_RANGE_ERASE;

    *(uint32_t *)&BSL_TX_buffer[4] = ui32StartAddress;
    *(uint32_t *)&BSL_TX_buffer[8] = ui32EndAddress;

    ui32CRC = softwareCRC(&BSL_TX_buffer[3] , CMD_BYTE + ADDRS_BYTES + ADDRS_BYTES);
    *(uint32_t *)&BSL_TX_buffer[12] = ui32CRC;

    uart_ack = UART_writeBuffer(BSL_TX_buffer , HDR_LEN_CMD_BYTES + ADDRS_BYTES
        + ADDRS_BYTES + CRC_BYTES);
    if (uart_ack != uart_noError) 
        TurnOnErrorLED();
    
    bsl_err = Host_BSL_getResponse();
    
    return (bsl_err);
}


