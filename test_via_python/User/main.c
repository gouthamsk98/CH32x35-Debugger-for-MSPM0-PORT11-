// code for bsl_uart_to_msp (working code)

#include "debug.h"
#include "ch32x035_usbfs_device.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <uart_msp.h>
#include "bsl.h"


BSL_error_t bsl_err;
uint8_t status;
uint32_t start_addr = 0; 
uint32_t end_addr = 0;
/*=============================================================================
* Here is password of the boot code for update. The last two bytes if the start address of the boot code.
*/
const uint8_t BSL_PW_RESET[32] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF};


uint8_t main_data[1024];
uint8_t *read_buf = NULL;

/*Function for calculating checksum */
uint8_t CheckSum(uint8_t buf[], uint8_t len) 
{
  uint8_t i;
  uint16_t temp = 0;
  for (i = 2; i < len - 1; i++) {
    temp += buf[i];
  }

  temp = temp & 0xFF;
  temp = ~temp;
  i = temp;
  return i;
}

/*main function for performing operation based on the frame received from serial*/
void write_function(uint8_t *usb_data , uint32_t size)
{
  memcpy(main_data , usb_data , size);
  printf("MAIN_DATA size  : %lu" , size);
  
  /*determine operation based on the function received from frame 
  
  frame : [0xf9 , 0xff , L1, L2 , L3, L4 , cmd, function , parameter seq , CKSM ]*/
  switch(main_data[7])
  {
    case START:

    	Host_BSL_entry_sequence(); /*PLACE TARGET INTO BSL MODE by hardware invoke*/
      Delay_Ms(500);
      printf( "inside entry");

      bsl_err = Host_BSL_Connection();  /*ESTABLISH BSL connection*/ 
      Delay_Ms(1000);
      if(bsl_err == eBSL_success)
      {
        status = Status_check();  //Check the status of the target: BSL mode or application mode
        if (status == 0x51)  //BSL mode 0x51; application mode 0x22
        {
          Delay_Ms(500);
          uint16_t MAX_BUFFER_SIZE = 0;
          bsl_err = Host_BSL_GetID(&MAX_BUFFER_SIZE);    /*GET MAX BUFFER size*/
          
          /*if max buf size is suficient */
          if(MAX_BUFFER_SIZE >= MAX_PACKET_SIZE) {
            bsl_err = Host_BSL_loadPassword((uint8_t*) BSL_PW_RESET);  /* load password*/
            if(bsl_err == eBSL_success) {

              /*prepare data to send : data 0x01 indicates it is successful (OK)*/
              uint8_t data[] = {0x01};
              uint8_t tx_data[sizeof(data) + 9];        /*buf to put response frame */
              uint32_t ui32Length = sizeof(data) + 2;   /*ui32Length  : size of data + cmd + function*/

              /*reponse frame to send if it is successful
                
              RESPONSE FRAME : [0xf9 ,0xf5, L1,L2,L3,L4, cmd, function, 0x01(OK), CKSM]*/

              tx_data[0] = 0xF9;
              tx_data[1] = 0xF5;
              tx_data[2] = (ui32Length >> 24);
              tx_data[3] = (ui32Length >> 16);
              tx_data[4] = (ui32Length >> 8);
              tx_data[5] = ui32Length;
              tx_data[6] = FOR_WRITE;
              tx_data[7] = ACK;
              memcpy(tx_data + 8 , data , sizeof(data));
              tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

              /*send frame to serial through usb*/
              USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
              
              memset(main_data , '\0' , sizeof(main_data));   /*clear maindata buffer*/
            }
          }
        } else {    /*if status is not 0x51*/
          
          /*prepare data to send : data 0x00 indicates it is not successful (FAIL)*/
          uint8_t data[] = {0x00};
          uint8_t tx_data[sizeof(data) + 9];             /*buf to put response frame */
          uint32_t ui32Length = sizeof(data) + 2;        /*ui32Length  : size of data + cmd + function*/

          /*RESPONSE FRAME: [0xf9 , 0xf5, L1,L2,L3,L4, cmd , function , 0x00(FAIL), CKSM]*/

          tx_data[0] = 0xF9;
          tx_data[1] = 0xF5;
          tx_data[2] = (uint8_t)(ui32Length >> 24);
          tx_data[3] = (uint8_t)(ui32Length >> 16);
          tx_data[4] = (uint8_t)(ui32Length >> 8);
          tx_data[5] = (uint8_t)ui32Length;
          tx_data[6] = FOR_WRITE;
          tx_data[7] = ACK;
          memcpy(tx_data + 8 , data , sizeof(data));
          tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));
      
          USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
          memset(main_data , '\0' , sizeof(main_data));
        }
      }

    break;

    case ERASE:
      
      if(bsl_err == eBSL_success){    
        bsl_err = Host_BSL_MassErase();        /*call masserase function*/

        if(bsl_err == eBSL_success){
          /*prepare data to send : data 0x01 indicates it is successful (OK)*/
          uint8_t data[] = {0x01};
          uint8_t tx_data[sizeof(data) + 9];
          uint32_t ui32Length = sizeof(data) + 2;
          
          /*RESPONSE FRAME : [0xf9 ,0xf5, L1,L2,L3,L4, cmd, function, 0x01(OK), CKSM]*/
          tx_data[0] = 0xF9;
          tx_data[1] = 0xF5;
          tx_data[2] = (ui32Length >> 24);
          tx_data[3] = (ui32Length >> 16);
          tx_data[4] = (ui32Length >> 8);
          tx_data[5] = ui32Length;
          tx_data[6] = FOR_WRITE;
          tx_data[7] = ACK;
          memcpy(tx_data + 8 , data , sizeof(data));
          tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));
          
          USBFS_Endp_DataUp(tx_data, sizeof(tx_data));

        } else {
          /*prepare data to send : data 0x00 indicates it is not successful (FAIL)*/
          uint8_t data[] = {0x00};
          uint8_t tx_data[sizeof(data) + 9];
          uint32_t ui32Length = sizeof(data) + 2;
          
          /*RESPONSE FRAME: [0xf9 , 0xf5, L1,L2,L3,L4, cmd , function , 0x00(FAIL), CKSM]*/
          tx_data[0] = 0xF9;
          tx_data[1] = 0xF5;
          tx_data[2] = (ui32Length >> 24);
          tx_data[3] = (ui32Length >> 16);
          tx_data[4] = (ui32Length >> 8);
          tx_data[5] = ui32Length;
          tx_data[6] = FOR_WRITE;
          tx_data[7] = ACK;
          memcpy(tx_data + 8 , data , sizeof(data));
          tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

          USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
        }

        memset(main_data , '\0' , sizeof(main_data));
      }
    break;

    case PAGE_ERASE:
      
      /*for erasing address range*/

      /*frame from serial have start address and end address
      
      RCVD FRAME : [0xf9 , 0xff , L1,L2,L3,L4 , cmd , function , SA1,SA2,SA3,SA4,EA1,EA2,EA3,EA4 , CKSM] */

      start_addr = 0; 
      end_addr = 0;

      start_addr |= (main_data[8] << 24);
      start_addr |= (main_data[9] << 16);
      start_addr |= (main_data[10] << 8);
      start_addr |= main_data[11];

      end_addr |= (main_data[12] << 24);
      end_addr |= (main_data[13] << 16);
      end_addr |= (main_data[14] << 8);
      end_addr |= main_data[15];


      if(bsl_err == eBSL_success){
        bsl_err = Host_BSL_RangeErase(start_addr , end_addr);  /*call range erase fn*/

        if(bsl_err == eBSL_success){
          /*prepare data to send : data 0x01 indicates it is successful (OK)*/
          uint8_t data[] = {0x01};
          uint8_t tx_data[sizeof(data) + 9];
          uint32_t ui32Length = sizeof(data) + 2;

          /*RESPONSE FRAME : [0xf9 ,0xf5, L1,L2,L3,L4, cmd, function, 0x01(OK), CKSM]*/
          tx_data[0] = 0xF9;
          tx_data[1] = 0xF5;
          tx_data[2] = (ui32Length >> 24);
          tx_data[3] = (ui32Length >> 16);
          tx_data[4] = (ui32Length >> 8);
          tx_data[5] = ui32Length;
          tx_data[6] = FOR_WRITE;
          tx_data[7] = ACK;
          memcpy(tx_data + 8 , data , sizeof(data));
          tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

          USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
          
        } else {
          /*prepare data to send : data 0x00 indicates it is not successful (FAIL)*/
          uint8_t data[] = {0x00};
          uint8_t tx_data[sizeof(data) + 9];
          uint32_t ui32Length = sizeof(data) + 2;
          
          /*RESPONSE FRAME: [0xf9 , 0xf5, L1,L2,L3,L4, cmd , function , 0x00(FAIL), CKSM]*/
          tx_data[0] = 0xF9;
          tx_data[1] = 0xF5;
          tx_data[2] = (ui32Length >> 24);
          tx_data[3] = (ui32Length >> 16);
          tx_data[4] = (ui32Length >> 8);
          tx_data[5] = ui32Length;
          tx_data[6] = FOR_WRITE;
          tx_data[7] = ACK;
          memcpy(tx_data + 8 , data , sizeof(data));
          tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

          USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
        }

        memset(main_data , '\0' , sizeof(main_data));
      }

    break;

    case START_APP:
    {

      Delay_Ms(100);
      bsl_err = Host_BSL_StartApp();

      if(bsl_err == eBSL_success){
        uint8_t data[] = {0x01};
        uint8_t tx_data[sizeof(data) + 9];
        uint32_t ui32Length = sizeof(data) + 2;

        /*RESPONSE FRAME : [0xf9 ,0xf5, L1,L2,L3,L4, cmd, function, 0x01(OK), CKSM]*/
        tx_data[0] = 0xF9;
        tx_data[1] = 0xF5;
        tx_data[2] = (ui32Length >> 24);
        tx_data[3] = (ui32Length >> 16);
        tx_data[4] = (ui32Length >> 8);
        tx_data[5] = ui32Length;
        tx_data[6] = FOR_WRITE;
        tx_data[7] = ACK;
        memcpy(tx_data + 8 , data , sizeof(data));
        tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

        USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
          
      } else {

        uint8_t data[] = {0x00};
        uint8_t tx_data[sizeof(data) + 9];
        uint32_t ui32Length = sizeof(data) + 2;
        
        /*RESPONSE FRAME: [0xf9 , 0xf5, L1,L2,L3,L4, cmd , function , 0x00(FAIL), CKSM]*/
        tx_data[0] = 0xF9;
        tx_data[1] = 0xF5;
        tx_data[2] = (ui32Length >> 24);
        tx_data[3] = (ui32Length >> 16);
        tx_data[4] = (ui32Length >> 8);
        tx_data[5] = ui32Length;
        tx_data[6] = FOR_WRITE;
        tx_data[7] = ACK;
        memcpy(tx_data + 8 , data , sizeof(data));
        tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));
        
        USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
      }
 
      memset(main_data , '\0' , sizeof(main_data));
    }
    break;

    case WRITE:
    {

      printf("inside write to memory");
      uint32_t ui32WRAddress = 0;
      uint32_t ui32BytesToWrite = 0;
      
      bsl_err = eBSL_success;
      /*received frame contains address and length
      RCVD FRAME : [0xf9 , 0xff , L1,L2,L3,L4 , cmd , function , A1,A2,A3,A4,L1,L2,L3,L4 ,CKSM] 
      
      if this frame is received successfully , next while loop starts for reading firmware bytes*/ 
      ui32WRAddress |= (main_data[8] << 24);
      ui32WRAddress |= (main_data[9] << 16);
      ui32WRAddress |= (main_data[10] << 8);
      ui32WRAddress |= main_data[11];

      ui32BytesToWrite |= (main_data[12] << 24);
      ui32BytesToWrite |= (main_data[13] << 16);
      ui32BytesToWrite |= (main_data[14] << 8);
      ui32BytesToWrite |= main_data[15];

      printf("ui32WRAddress  %lu , ui32BytesToWrite : %lu" , ui32WRAddress ,ui32BytesToWrite );
      uint8_t write_data[64];
      uint8_t ui8DataLen = 0;
    
      while(ui32BytesToWrite > 0)      /*loop runs if total firmware length grtr than zero*/
      {
          
        USBFSD->UEP1_CTRL_H = (USBFSD->UEP1_CTRL_H & ~USBFS_UEP_R_RES_MASK) | USBFS_UEP_R_RES_ACK;
        ui8DataLen = USBFS_endp_dataRead(write_data);


        if(ui8DataLen > 0)
        { 
          bsl_err = Host_BSL_writeMemory(        
            ui32WRAddress, write_data, ui8DataLen);    /*write rcvd bytes to memory */
            
          ui32BytesToWrite -= ui8DataLen;              /*subtract rcvd length from total firmware length*/
          ui32WRAddress += ui8DataLen;                 /* add rcvd length to target address */

          printf("ui32DataLen  : %d  , ADDRESS : %lu , ui32BytesToWrite : %lu" 
            , ui8DataLen  , ui32WRAddress , ui32BytesToWrite);

          memset(write_data , '\0' , sizeof(write_data));
        } 
      }


      if(bsl_err == eBSL_success){   /*if write is success*/

        uint8_t data[] = {0x01};
        uint8_t tx_data[sizeof(data) + 9];
        uint32_t ui32Length = sizeof(data) + 2;

        /*RESPONSE FRAME : [0xf9 ,0xf5, L1,L2,L3,L4, cmd, function, 0x01(OK), CKSM]*/
        tx_data[0] = 0xF9;
        tx_data[1] = 0xF5;
        tx_data[2] = (ui32Length >> 24);
        tx_data[3] = (ui32Length >> 16);
        tx_data[4] = (ui32Length >> 8);
        tx_data[5] = ui32Length;
        tx_data[6] = FOR_WRITE;
        tx_data[7] = ACK;
        memcpy(tx_data + 8 , data , sizeof(data));
        tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

        USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
          
      } else {

        uint8_t data[] = {0x00};
        uint8_t tx_data[sizeof(data) + 9];
        uint32_t ui32Length = sizeof(data) + 2;

        /*RESPONSE FRAME: [0xf9 , 0xf5, L1,L2,L3,L4, cmd , function , 0x00(FAIL), CKSM]*/
        tx_data[0] = 0xF9;
        tx_data[1] = 0xF5;
        tx_data[2] = (ui32Length >> 24);
        tx_data[3] = (ui32Length >> 16);
        tx_data[4] = (ui32Length >> 8);
        tx_data[5] = ui32Length;
        tx_data[6] = FOR_WRITE;
        tx_data[7] = ACK;
        memcpy(tx_data + 8 , data , sizeof(data));
        tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

        USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
      }
      memset(main_data , '\0' , sizeof(main_data));
    } 
    break;


    case VERIFY:
  
      printf( "inside verify");  
      
      /*received frame contains localcrc , address and length
      RCVD FRAME : [0xf9 , 0xff , L1,L2,L3,L4 , cmd , function , C1,C2,C3,C4,A1,A2,A3,A4,L1,L2,L3,L4 , CKSM] */

      uint32_t target_crc = 0;
      uint32_t local_crc = 0;
      uint32_t ui32VRAddress = 0;
      uint32_t ui32VRDataLen = 0; 

      local_crc |= (main_data[8] << 24);
      local_crc |= (main_data[9] << 16);
      local_crc |= (main_data[10] << 8);
      local_crc |= main_data[11];

      ui32VRAddress |= (main_data[12] << 24);
      ui32VRAddress |= (main_data[13] << 16);
      ui32VRAddress |= (main_data[14] << 8);
      ui32VRAddress |= main_data[15];

      ui32VRDataLen |= (main_data[16] << 24);
      ui32VRDataLen |= (main_data[17] << 16);
      ui32VRDataLen |= (main_data[18] << 8);
      ui32VRDataLen |= main_data[19];
      

      printf("crc %lu" , local_crc);
      printf("ADDR: %lu , len : %lu" , ui32VRAddress ,ui32VRDataLen);
      /*fn for verify . targetcrc contains the crc from this function*/
      bsl_err = Host_BSL_Verify(ui32VRAddress , ui32VRDataLen , &target_crc);  
    
      if(local_crc == target_crc){   /*if two crcs are equal ,send OK*/     
        uint8_t data[] = {0x01};
        uint8_t tx_data[sizeof(data) + 9];
        uint32_t ui32Length = sizeof(data) + 2;

        /*RESPONSE FRAME : [0xf9 ,0xf5, L1,L2,L3,L4, cmd, function, 0x01(OK), CKSM]*/
        tx_data[0] = 0xF9;
        tx_data[1] = 0xF5;
        tx_data[2] = (ui32Length >> 24);
        tx_data[3] = (ui32Length >> 16);
        tx_data[4] = (ui32Length >> 8);
        tx_data[5] = ui32Length;
        tx_data[6] = FOR_WRITE;
        tx_data[7] = ACK;
        memcpy(tx_data + 8 , data , sizeof(data));
        tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));
        
        USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
          
      } else {

        uint8_t data[] = {0x00};
        uint8_t tx_data[sizeof(data) + 9];
        uint32_t ui32Length = sizeof(data) + 2;

        /*RESPONSE FRAME: [0xf9 , 0xf5, L1,L2,L3,L4, cmd , function , 0x00(FAIL), CKSM]*/
        tx_data[0] = 0xF9;
        tx_data[1] = 0xF5;
        tx_data[2] = (ui32Length >> 24);
        tx_data[3] = (ui32Length >> 16);
        tx_data[4] = (ui32Length >> 8);
        tx_data[5] = ui32Length;
        tx_data[6] = FOR_WRITE;
        tx_data[7] = ACK;
        memcpy(tx_data + 8 , data , sizeof(data));
        tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));

        USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
      }

      memset(main_data , '\0' , sizeof(main_data));
 
    break;

    case READ:
        
      printf("inside read");
      
      /*received frame contains address and length
      RCVD FRAME : [0xf9 , 0xff , L1,L2,L3,L4 , cmd , function , A1,A2,A3,A4,L1,L2,L3,L4 , CKSM] */

      uint32_t ui32RDAddress = 0; 
      uint32_t ui32bytesToRead = 0;
      
      ui32RDAddress |= (main_data[8] << 24);
      ui32RDAddress |= (main_data[9] << 16);
      ui32RDAddress |= (main_data[10] << 8);
      ui32RDAddress |= main_data[11];

      ui32bytesToRead |= (main_data[12] << 24);
      ui32bytesToRead |= (main_data[13] << 16);
      ui32bytesToRead |= (main_data[14] << 8);
      ui32bytesToRead |= main_data[15];

      uint32_t ui32DataLen;
      uint8_t MAX_DATA_SIZE = 55;          /*max data size set to 55 */
      read_buf = (uint8_t*)malloc(MAX_DATA_SIZE);
      printf( "malloc");
      if(bsl_err == eBSL_success){

        while(ui32bytesToRead > 0){        /* ui32bytesToRead :total length to read*/
          if(ui32bytesToRead > MAX_DATA_SIZE)  
            ui32DataLen = MAX_DATA_SIZE;
          else
            ui32DataLen = ui32bytesToRead;

          ui32bytesToRead -= ui32DataLen;      /*subtract the read length from total length */

          bsl_err = Host_BSL_ReadMemory(ui32RDAddress  ,ui32DataLen, read_buf);   /*read data */
          ui32RDAddress  += ui32DataLen;      /*add the address*/

          Delay_Ms(500);

          if(bsl_err == eBSL_success){  /*if read is success*/

            printf("bytestoread: %lu datalength : %lu "
              , ui32bytesToRead , ui32DataLen);

            // ESP_LOG_BUFFER_HEX(TAG ,read_buf, ui32DataLen);

            uint8_t tx_data[ui32DataLen + 9];
            uint32_t ui32Length = ui32DataLen + 2;
            
            /*response frame: 0xf9,0xf5, L1,L2,L3,L4 , cmd , function , readbuf(data) , CKSM]*/

            tx_data[0] = 0xF9;
            tx_data[1] = 0xF5;
            tx_data[2] = (ui32Length >> 24);
            tx_data[3] = (ui32Length >> 16);
            tx_data[4] = (ui32Length >> 8);
            tx_data[5] = ui32Length;
            tx_data[6] = FOR_WRITE;
            tx_data[7] = DATA;
            memcpy(tx_data + 8 , read_buf , ui32DataLen);
            tx_data[(ui32DataLen + 9) - 1] = CheckSum(tx_data , sizeof(tx_data));
            
            USBFS_Endp_DataUp(tx_data, sizeof(tx_data));
 
          }
        }

      } else {

        uint8_t data[] = {0x00};
        uint8_t tx_data[sizeof(data) + 9];
        uint32_t ui32Length = sizeof(data) + 2;
        
        /*RESPONSE FRAME: [0xf9 , 0xf5, L1,L2,L3,L4, cmd , function , 0x00(FAIL), CKSM]*/
        tx_data[0] = 0xF9;
        tx_data[1] = 0xF5;
        tx_data[2] = (ui32Length >> 24);
        tx_data[3] = (ui32Length >> 16);
        tx_data[4] = (ui32Length >> 8);
        tx_data[5] = ui32Length;
        tx_data[6] = FOR_WRITE;
        tx_data[7] = ACK;
        memcpy(tx_data + 8 , data , sizeof(data));
        tx_data[(sizeof(data) + 9) - 1] = CheckSum(tx_data , sizeof(tx_data)); 

        USBFS_Endp_DataUp(tx_data, sizeof(tx_data));

      } 

      if(read_buf) {
        free(read_buf);
        printf( "read_buf to NULL");
        read_buf = NULL;
      }

      memset(main_data , '\0' , sizeof(main_data));

    break;

    // case EXIT:   /*restart */
    //   esp_restart();
  }
}


static void usb_read_task(void)
{
  uint8_t rx_data[sizeof(main_data)];
  uint8_t rx_len = 0; 
  
  printf("offfset to 0 {app_main}");
  while(1)
  { 
    /*read the usb data*/
  
    rx_len = USBFS_endp_dataRead(rx_data);

    if(rx_len > 0) {
      
      printf( " rx_len : %d" ,rx_len);
      uint8_t checksum = CheckSum(rx_data , rx_len - 1);
      printf( "checksum  : %d " , checksum);

      /*check the rx_data headers and checksum are correct .
      if these are correct , call the main fn*/

      if(rx_data[0] == 0xF9 && rx_data[1] == 0xFF 
        && rx_data[rx_len-1] == checksum){

        rx_data[rx_len] = '\0';
        write_function(rx_data , rx_len);

      }
    }
    memset(rx_data, '\0', sizeof(rx_data));
  } 
}

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
/*uart setup*/
  NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
  SystemCoreClockUpdate();
  Delay_Init();
  USART_Printf_Init(115200);
  printf("SystemClk:%d\r\n", SystemCoreClock);
  printf("ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
  uart_setup();

  /* Usb Init */
  USBFS_RCC_Init( );
  USBFS_Device_Init( ENABLE , PWR_VDD_SupplyVoltage());

  usb_read_task();
}


