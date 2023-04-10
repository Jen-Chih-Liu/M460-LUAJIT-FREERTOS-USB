/*
 * FreeRTOS Kernel V10.0.0
 * Copyright (C) 2017 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software. If you wish to use our Amazon
 * FreeRTOS name, please do so in a fair use way that does not cause confusion.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://www.FreeRTOS.org
 * http://aws.amazon.com/freertos
 *
 * 1 tab == 4 spaces!
 */

#include <stdio.h>
#include <string.h>
/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "queue.h"
/* Hardware and starter kit includes. */

#include "NuMicro.h"
QueueHandle_t Message_Queue;

#define MESSAGE_Q_NUM 1
#define USART_REC_LEN 64

// 任務優先級
#define START_TASK_PRIO 1
// 任務堆棧大小
#define START_STK_SIZE 256
// 任務句柄
TaskHandle_t StartTask_Handler;
// 任務函數
void start_task(void *pvParameters);

// 任務優先級
#define VCOM_TASK_PRIO 2
// 任務堆棧大小
#define VCOM_STK_SIZE 256
// 任務句柄
TaskHandle_t VCOMTask_Handler;
// 任務函數
void VCOM_task(void *pvParameters);

// 任務優先級
#define TASK2_TASK_PRIO 3
// 任務堆棧大小
#define TASK2_STK_SIZE 256
// 任務句柄
TaskHandle_t Task2Task_Handler;
// 任務函數
extern void task2_task(void *pvParameters);

void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();
    Message_Queue = xQueueCreate(MESSAGE_Q_NUM, USART_REC_LEN);

    if (Message_Queue == NULL)
    {
        printf("Create Message_Queue Failed!\r\n");
    }
    xTaskCreate((TaskFunction_t)VCOM_task,
                (const char *)"vcom_task",
                (uint16_t)VCOM_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)VCOM_TASK_PRIO,
                (TaskHandle_t *)&VCOMTask_Handler);

    xTaskCreate((TaskFunction_t)task2_task,
                (const char *)"task2_task",
                (uint16_t)TASK2_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)TASK2_TASK_PRIO,
                (TaskHandle_t *)&Task2Task_Handler);

    vTaskDelete(StartTask_Handler);
    taskEXIT_CRITICAL();
}
#include "vcom_and_hid_transfer.h"
#define TRIM_INIT (SYS_BASE + 0x10C)
/*--------------------------------------------------------------------------*/
STR_VCOM_LINE_CODING g_LineCoding = {115200, 0, 0, 8}; /* Baud rate : 115200    */
/* Stop bit     */
/* parity       */
/* data bits    */
uint16_t g_u16CtrlSignal = 0; /* BIT0: DTR(Data Terminal Ready) , BIT1: RTS(Request To Send) */

/*--------------------------------------------------------------------------*/
#define RXBUFSIZE 512 /* RX buffer size */
#define TXBUFSIZE 512 /* RX buffer size */

/*---------------------------------------------------------------------------------------------------------*/
/* Global variables                                                                                        */
/*---------------------------------------------------------------------------------------------------------*/
/* UART0 */
static volatile uint8_t s_au8ComRbuf[RXBUFSIZE];
volatile uint16_t g_u16ComRbytes = 0;
volatile uint16_t g_u16ComRhead = 0;
volatile uint16_t g_u16ComRtail = 0;

static uint8_t s_au8RxBuf[64] = {0};
uint8_t *g_pu8RxBuf = 0;
uint32_t g_u32RxSize = 0;
uint32_t g_u32TxSize = 0;

volatile int8_t g_i8BulkOutReady = 0;

void VCOM_TransferData(void)
{
    uint32_t i, u32Len;

    /* Check whether USB is ready for next packet or not */
    if (g_u32TxSize == 0)
    {
        /* Check whether we have new COM Rx data to send to USB or not */
        if (g_u16ComRbytes)
        {
            u32Len = g_u16ComRbytes;
            if (u32Len > EP2_MAX_PKT_SIZE)
                u32Len = EP2_MAX_PKT_SIZE;

            for (i = 0; i < u32Len; i++)
            {
                s_au8RxBuf[i] = s_au8ComRbuf[g_u16ComRhead++];
                if (g_u16ComRhead >= RXBUFSIZE)
                    g_u16ComRhead = 0;
            }

            __set_PRIMASK(1);
            g_u16ComRbytes -= u32Len;
            __set_PRIMASK(0);

            g_u32TxSize = u32Len;
            USBD_MemCopy((uint8_t *)(USBD_BUF_BASE + USBD_GET_EP_BUF_ADDR(EP2)), (uint8_t *)s_au8RxBuf, u32Len);
            USBD_SET_PAYLOAD_LEN(EP2, u32Len);
        }
        else
        {
            /* Prepare a zero packet if previous packet size is EP2_MAX_PKT_SIZE and
               no more data to send at this moment to note Host the transfer has been done */
            u32Len = USBD_GET_PAYLOAD_LEN(EP2);
            if (u32Len == EP2_MAX_PKT_SIZE)
                USBD_SET_PAYLOAD_LEN(EP2, 0);
        }
    }
}

void VCOM_task(void *pvParameters)
{
    uint32_t u32TrimInit;
    /* Backup default trim */
    u32TrimInit = M32(TRIM_INIT);

    while (1)
    {

        /* Start USB trim if it is not enabled. */
        if ((SYS->HIRCTCTL & SYS_HIRCTCTL_FREQSEL_Msk) != 1)
        {
            /* Start USB trim only when SOF */
            if (USBD->INTSTS & USBD_INTSTS_SOFIF_Msk)
            {
                /* Clear SOF */
                USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;

                /* Re-enable crystal-less */
                SYS->HIRCTCTL = 0x01;
                SYS->HIRCTCTL |= SYS_HIRCTCTL_REFCKSEL_Msk | SYS_HIRCTCTL_BOUNDEN_Msk | (8 << SYS_HIRCTCTL_BOUNDARY_Pos);
            }
        }

        /* Disable USB Trim when error */
        if (SYS->HIRCTISTS & (SYS_HIRCTISTS_CLKERRIF_Msk | SYS_HIRCTISTS_TFAILIF_Msk))
        {
            /* Init TRIM */
            M32(TRIM_INIT) = u32TrimInit;

            /* Disable crystal-less */
            SYS->HIRCTCTL = 0;

            /* Clear error flags */
            SYS->HIRCTISTS = SYS_HIRCTISTS_CLKERRIF_Msk | SYS_HIRCTISTS_TFAILIF_Msk;

            /* Clear SOF */
            USBD->INTSTS = USBD_INTSTS_SOFIF_Msk;
        }

        VCOM_TransferData();

        vTaskDelay(10);
    }
}

/*-----------------------------------------------------------*/

void vApplicationMallocFailedHook(void)
{
    /* vApplicationMallocFailedHook() will only be called if
    configUSE_MALLOC_FAILED_HOOK is set to 1 in FreeRTOSConfig.h.  It is a hook
    function that will get called if a call to pvPortMalloc() fails.
    pvPortMalloc() is called internally by the kernel whenever a task, queue,
    timer or semaphore is created.  It is also called by various parts of the
    demo application.  If heap_1.c or heap_2.c are used, then the size of the
    heap available to pvPortMalloc() is defined by configTOTAL_HEAP_SIZE in
    FreeRTOSConfig.h, and the xPortGetFreeHeapSize() API function can be used
    to query the size of free heap space that remains (although it does not
    provide information on how the remaining heap might be fragmented). */
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;
}
/*-----------------------------------------------------------*/

void vApplicationIdleHook(void)
{
    /* vApplicationIdleHook() will only be called if configUSE_IDLE_HOOK is set
    to 1 in FreeRTOSConfig.h.  It will be called on each iteration of the idle
    task.  It is essential that code added to this hook function never attempts
    to block in any way (for example, call xQueueReceive() with a block time
    specified, or call vTaskDelay()).  If the application makes use of the
    vTaskDelete() API function (as this demo application does) then it is also
    important that vApplicationIdleHook() is permitted to return to its calling
    function, because it is the responsibility of the idle task to clean up
    memory allocated by the kernel to any task that has since been deleted. */
}
/*-----------------------------------------------------------*/

void vApplicationStackOverflowHook(xTaskHandle pxTask, signed char *pcTaskName)
{
    (void)pcTaskName;
    (void)pxTask;

    /* Run time stack overflow checking is performed if
    configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2.  This hook
    function is called if a stack overflow is detected. */
    taskDISABLE_INTERRUPTS();
    for (;;)
        ;
}
