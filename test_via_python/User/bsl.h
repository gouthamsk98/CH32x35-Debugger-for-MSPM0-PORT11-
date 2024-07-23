#ifndef _BSL_H_
#define _BSL_H_

#include <stdio.h>
#include "stdint.h"

#define MAX_PAYLOAD_DATA_SIZE (128)
//MAX_PACKET_SIZE = MAX_PAYLOAD_DATA_SIZE + HDR_LEN_CMD_BYTES + CRC_BYTES = 128 + 8 = 136
#define MAX_PACKET_SIZE (136)

#define Hardware_Invoke

// uint8_t BSL_TX_buffer[MAX_PACKET_SIZE + 2];
// uint8_t BSL_RX_buffer[MAX_PACKET_SIZE + 2];
// ! Define BSL CORE commands
#define CMD_CONNECTION (0x12)
#define CMD_GET_ID (0x19)
#define CMD_RX_PASSWORD (0x21)
#define CMD_MASS_ERASE (0x15)
#define CMD_PROGRAMDATA (0x20)
#define CMD_START_APP (0x40)
#define CMD_READBACK_DATA (0x29)
#define CMD_VERIFY (0x26)
#define CMD_FLASH_RANGE_ERASE (0x23)


#define PACKET_HEADER (0x80)
#define CMD_BYTE (1)
#define HDR_LEN_CMD_BYTES (4)
#define CRC_BYTES (4)
#define PASSWORD_SIZE (uint8_t)(32)
#define ACK_BYTE (1)
#define ID_BACK (24)
#define ADDRS_BYTES (4)
#define LEN_BYTES (4)

//================================================================================
// ! Conversion MACROS
#define LSB(x) ((x) & 0x00FF)
#define MSB(x) (((x) & 0xFF00) >> 8)

enum {
    //! No Error Occurred! The operation was successful.
    eBSL_success = 0,

    //! Flash write check failed. After programming, a CRC is run on the programmed data
    //! If the CRC does not match the expected result, this error is returned.
    eBSL_flashWriteCheckFailed = 1,

    //! BSL locked.  The correct password has not yet been supplied to unlock the BSL.
    eBSL_locked = 4,

    //! BSL password error. An incorrect password was supplied to the BSL when attempting an unlock.
    eBSL_passwordError = 5,

    //! Unknown error.  The command given to the BSL was not recognized
    eBSL_unknownError = 7,

    eBSL_responseCommand = 0x3B

};
typedef uint8_t BSL_error_t;

enum {
    uart_noError   = 0,     //normal ACK
    header_Error   = 0x51,  //Header incorrect
    checksum_Error = 0x52,  //Checksum incorrect.
    //   packetsize0_Error = 0x53,   //Packet size zero.
    //   packetsizemax_Error = 0x54, //Packet size exceeds buffer.
    unknown_Error = 0x55,  //Unknown error
    //   baudrate_Error = 0x56,      //Unknown baud rate.
    packetsize_Error = 0x57,  //Packet Size Error.

};

typedef uint8_t uart_error_t;

enum {
    START      = 0x01,
    ERASE      = 0x02,
    PAGE_ERASE = 0x03,
    WRITE      = 0x04,
    READ       = 0x05,
    VERIFY     = 0x06,
    START_APP  = 0x07,
    EXIT       = 0x08,
    ACK        = 0x09,
    DATA       = 0x0A,
};


enum {
    FOR_WRITE = 0x11,
    FOR_READ = 0x22,
};


void Host_BSL_entry_sequence(void);

BSL_error_t Host_BSL_Connection(void);
BSL_error_t Host_BSL_GetID(uint16_t *max_buf_size);
BSL_error_t Host_BSL_loadPassword(uint8_t* pPassword);
BSL_error_t Host_BSL_MassErase(void);
BSL_error_t Host_BSL_writeMemory(
    uint32_t addr, const uint8_t* data, uint32_t len);
BSL_error_t Host_BSL_StartApp(void);

uint32_t softwareCRC(const uint8_t* data, uint8_t length);
BSL_error_t Host_BSL_getResponse(void);
BSL_error_t Host_BSL_ReadMemory(uint32_t addr , uint32_t len , uint8_t* readBackData);
BSL_error_t Host_BSL_Verify(uint32_t addr , uint32_t len , uint32_t *crc);
BSL_error_t Host_BSL_RangeErase(uint32_t startaddr , uint32_t endaddr);


#endif
