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

/* Kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "semphr.h"
#include "queue.h"
/* Hardware and starter kit includes. */

#include "NuMicro.h"
QueueHandle_t Key_Queue1, Key_Queue2;
QueueHandle_t Message_Queue;

#define KEYMSG_Q_NUM1 1
#define KEYMSG_Q_NUM2 1
#define MESSAGE_Q_NUM 4

static void prvSetupHardware(void);

// 任務優先級
#define START_TASK_PRIO 1
// 任務堆棧大小
#define START_STK_SIZE 256
// 任務句柄
TaskHandle_t StartTask_Handler;
// 任務函數
void start_task(void *pvParameters);

// 任務優先級
#define TASK1_TASK_PRIO 2
// 任務堆棧大小
#define TASK1_STK_SIZE 256
// 任務句柄
TaskHandle_t Task1Task_Handler;
// 任務函數
void task1_task(void *pvParameters);

// 任務優先級
#define TASK2_TASK_PRIO 3
// 任務堆棧大小
#define TASK2_STK_SIZE 256
// 任務句柄
TaskHandle_t Task2Task_Handler;
// 任務函數
void task2_task(void *pvParameters);

// 任務優先級
#define KEYPROCESS1_TASK_PRIO 4
// 任務堆棧大小
#define KEYPROCESS1_STK_SIZE 256
// 任務句柄
TaskHandle_t KeyprocessTask1_Handler;
// 任務函數
void Keyprocess1_task(void *pvParameters);

// 任務優先級
#define KEYPROCESS2_TASK_PRIO 5
// 任務堆棧大小
#define KEYPROCESS2_STK_SIZE 256
// 任務句柄
TaskHandle_t KeyprocessTask2_Handler;
// 任務函數
void Keyprocess2_task(void *pvParameters);

void start_task(void *pvParameters)
{
    taskENTER_CRITICAL();

    Key_Queue1 = xQueueCreate(KEYMSG_Q_NUM1, 20);
    Key_Queue2 = xQueueCreate(KEYMSG_Q_NUM1, 20);
    if (Key_Queue1 == NULL)
    {
        printf("Create Queue Failed!\r\n");
    }
    if (Key_Queue2 == NULL)
    {
        printf("Create Queue2 Failed!\r\n");
    }

    xTaskCreate((TaskFunction_t)task1_task,
                (const char *)"task1_task",
                (uint16_t)TASK1_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)TASK1_TASK_PRIO,
                (TaskHandle_t *)&Task1Task_Handler);

    xTaskCreate((TaskFunction_t)task2_task,
                (const char *)"task2_task",
                (uint16_t)TASK2_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)TASK2_TASK_PRIO,
                (TaskHandle_t *)&Task2Task_Handler);

    xTaskCreate((TaskFunction_t)Keyprocess1_task,
                (const char *)"Keyprocess1_task",
                (uint16_t)KEYPROCESS1_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)KEYPROCESS1_TASK_PRIO,
                (TaskHandle_t *)&KeyprocessTask1_Handler);

    xTaskCreate((TaskFunction_t)Keyprocess2_task,
                (const char *)"Keyprocess2_task",
                (uint16_t)KEYPROCESS2_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)KEYPROCESS2_TASK_PRIO,
                (TaskHandle_t *)&KeyprocessTask2_Handler);

    vTaskDelete(StartTask_Handler);
    taskEXIT_CRITICAL();
}

void task1_task(void *pvParameters)
{
    unsigned char key1;
    BaseType_t err;
    while (1)
    {
        key1 = PH0;
        if ((Key_Queue1 != NULL) && (key1 == 0))
        {
            err = xQueueOverwrite(Key_Queue1, &key1);

            if (err != pdTRUE)
            {
                printf("Queue Send Failed!\r\n");
            }
        }

        vTaskDelay(10);
    }
}

void task2_task(void *pvParameters)
{
    unsigned char key2;
    BaseType_t err;
    while (1)
    {

        key2 = PH1;
        if ((Key_Queue2 != NULL) && (key2 == 0))
        {
            err = xQueueOverwrite(Key_Queue2, &key2);

            if (err != pdTRUE)
            {
                printf("Queue Send Failed!\r\n");
            }
        }

        vTaskDelay(10);
    }
}

// Keyprocess任務函數
void Keyprocess1_task(void *pvParameters)
{
    unsigned char key;
    BaseType_t err;
    while (1)
    {
        if (Key_Queue1 != NULL)
        {
            err = xQueueReceive(Key_Queue1, &key, portMAX_DELAY);
            if (err == pdTRUE) // 獲取消息成功
            {
                printf("key1  value=%d\r\n", key);
            }
        }
        else
        {
            vTaskDelay(1000);
        }
    }
}

void Keyprocess2_task(void *pvParameters)
{
    unsigned char key;
    BaseType_t err;
    while (1)
    {
        if (Key_Queue2 != NULL)
        {
            err = xQueueReceive(Key_Queue2, &key, portMAX_DELAY);
            if (err == pdTRUE) // 獲取消息成功
            {
                printf("key2  value=%d\r\n", key);
            }
        }
        else
        {
            vTaskDelay(1000);
        }
    }
}

int main(void)
{
    /* Configure the hardware ready to run the test. */
    prvSetupHardware();

    xTaskCreate((TaskFunction_t)start_task,
                (const char *)"start_task",
                (uint16_t)START_STK_SIZE,
                (void *)NULL,
                (UBaseType_t)START_TASK_PRIO,
                (TaskHandle_t *)&StartTask_Handler);
    vTaskStartScheduler();
    while (1)
        ;
}
/*-----------------------------------------------------------*/

static void prvSetupHardware(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Set PCLK0 and PCLK1 to HCLK/2 */
    CLK->PCLKDIV = (CLK_PCLKDIV_APB0DIV_DIV2 | CLK_PCLKDIV_APB1DIV_DIV2);

    /* Set core clock to 200MHz */
    CLK_SetCoreClock(200000000);

    /* Enable all GPIO clock */
    CLK->AHBCLK0 |= CLK_AHBCLK0_GPACKEN_Msk | CLK_AHBCLK0_GPBCKEN_Msk | CLK_AHBCLK0_GPCCKEN_Msk | CLK_AHBCLK0_GPDCKEN_Msk |
                    CLK_AHBCLK0_GPECKEN_Msk | CLK_AHBCLK0_GPFCKEN_Msk | CLK_AHBCLK0_GPGCKEN_Msk | CLK_AHBCLK0_GPHCKEN_Msk;
    CLK->AHBCLK1 |= CLK_AHBCLK1_GPICKEN_Msk | CLK_AHBCLK1_GPJCKEN_Msk;

    /* Select peripheral clock source */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));
    CLK_SetModuleClock(TMR0_MODULE, CLK_CLKSEL1_TMR0SEL_HIRC, 0);

    /* Enable peripheral clock */
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_EnableModuleClock(TMR0_MODULE);

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Set multi-function pins for UART0 RXD and TXD */
    SET_UART0_RXD_PB12();
    SET_UART0_TXD_PB13();

    /* Configure PH.4, PH.5 and PH.6 as Output mode */
    GPIO_SetMode(PH, BIT4 | BIT5 | BIT6, GPIO_MODE_OUTPUT);

    /* Lock protected registers */
    SYS_LockReg();

    /* Init UART to 115200-8n1 for print message */
    UART_Open(UART0, 115200);
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
