
#include <stdio.h>
#include <string.h>
/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "NuMicro.h"
#include "vcom_and_hid_transfer.h"
unsigned char ReceiveBuf[64];
extern QueueHandle_t Message_Queue;
void task2_task(void *pvParameters)
{
	  int i=0;
    BaseType_t err;
    uint8_t *pu8Ptr;
    while (1)
    {
        if (Message_Queue != NULL)
        {
            err = xQueueReceive(Message_Queue, ReceiveBuf,(TickType_t)100 );
            if (err == pdTRUE)
            {
                printf("Receve:%s ", ReceiveBuf);
                memset(ReceiveBuf, 0, 64);				
                
							  for (i=0;i<64;i++)
							   ReceiveBuf[i]=64+i;
							pu8Ptr = (uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP5));
             USBD_MemCopy(pu8Ptr,ReceiveBuf , EP5_MAX_PKT_SIZE);
             USBD_SET_PAYLOAD_LEN(EP5, EP5_MAX_PKT_SIZE);
            }           
        }
				//if (ReceiveBuf[0]==0xa0) //i2c
					 
				
				//if (ReceiveBuf[0]==0xb0)
					  
         vTaskDelay(100);
    }
}
