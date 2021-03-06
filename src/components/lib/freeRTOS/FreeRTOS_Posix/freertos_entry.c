/*
	FreeRTOS.org V6.0.4 - Copyright (C) 2003-2009 Richard Barry.

	This file is part of the FreeRTOS.org distribution.

	FreeRTOS.org is free software; you can redistribute it and/or modify it
	under the terms of the GNU General Public License (version 2) as published
	by the Free Software Foundation and modified by the FreeRTOS exception.

	FreeRTOS.org is distributed in the hope that it will be useful,	but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
	FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
	more details.

	You should have received a copy of the GNU General Public License along
	with FreeRTOS.org; if not, write to the Free Software Foundation, Inc., 59
	Temple Place, Suite 330, Boston, MA  02111-1307  USA.

	A special exception to the GPL is included to allow you to distribute a
	combined work that includes FreeRTOS.org without being obliged to provide
	the source code for any proprietary components.  See the licensing section
	of http://www.FreeRTOS.org for full details.


	***************************************************************************
	*                                                                         *
	* Get the FreeRTOS eBook!  See http://www.FreeRTOS.org/Documentation      *
	*                                                                         *
	* This is a concise, step by step, 'hands on' guide that describes both   *
	* general multitasking concepts and FreeRTOS specifics. It presents and   *
	* explains numerous examples that are written using the FreeRTOS API.     *
	* Full source code for all the examples is provided in an accompanying    *
	* .zip file.                                                              *
	*                                                                         *
	***************************************************************************

	1 tab == 4 spaces!

	Please ensure to read the configuration and relevant port sections of the
	online documentation.

	http://www.FreeRTOS.org - Documentation, latest information, license and
	contact details.

	http://www.SafeRTOS.com - A version that is certified for use in safety
	critical systems.

	http://www.OpenRTOS.com - Commercial support, development, porting,
	licensing and training services.
*/

/**
 * Creates all the demo application tasks and co-routines, then starts the
 * scheduler.
 *
 * Main. c also creates a task called "Print".  This only executes every
 * five seconds but has the highest priority so is guaranteed to get
 * processor time.  Its main function is to check that all the other tasks
 * are still operational.  Nearly all the tasks in the demo application
 * maintain a unique count that is incremented each time the task successfully
 * completes its function.  Should any error occur within the task the count is
 * permanently halted.  The print task checks the count of each task to ensure
 * it has changed since the last time the print task executed.  If any count is
 * found not to have changed the print task displays an appropriate message.
 * If all the tasks are still incrementing their unique counts the print task
 * displays an "OK" message.
 *
 * The LED flash tasks do not maintain a count as they already provide visual
 * feedback of their status.
 *
 * The print task blocks on the queue into which messages that require
 * displaying are posted.  It will therefore only block for the full 5 seconds
 * if no messages are posted onto the queue.
 *
 * Main. c also provides a demonstration of how the trace visualisation utility
 * can be used, and how the scheduler can be stopped.
 *
 * \page MainC main.c
 * \ingroup DemoFiles
 * <HR>
 */

/* System headers. */
/* #include <stdio.h> */
/* #include <time.h> */
/* #include <sys/time.h> */
/* #include <stdlib.h> */
/* #include <sys/types.h> */
/* #include <netinet/in.h> */
/* #include <sys/socket.h> */
/* #include <arpa/inet.h> */
/* #include <sys/stat.h> */
/* #include <mqueue.h> */
/* #include <errno.h> */
/* #include <unistd.h> */

#include <cos_component.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "croutine.h"
#include <jw_freertos.h>
/* #include "partest.h" */

/* Demo file headers. */
/* #include "BlockQ.h" */
/* #include "PollQ.h" */
/* #include "death.h" */
/* #include "crflash.h" */
#include "flop.h"
/* #include "print.h" */
/* #include "fileIO.h" */
/* #include "semtest.h" */
#include "integer.h"
/* #include "dynamic.h" */
/* #include "mevents.h" */
/* #include "crhook.h" */
/* #include "blocktim.h" */
/* #include "GenQTest.h" */
/* #include "QPeek.h" */
/* #include "countsem.h" */
/* #include "recmutex.h" */

/* #include "AsyncIO/AsyncIO.h" */
/* #include "AsyncIO/AsyncIOSocket.h" */
/* #include "AsyncIO/PosixMessageQueueIPC.h" */
/* #include "AsyncIO/AsyncIOSerial.h" */

/* Priority definitions for the tasks in the demo application. */
#define mainLED_TASK_PRIORITY		( tskIDLE_PRIORITY + 1 )
#define mainCREATOR_TASK_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define mainPRINT_TASK_PRIORITY		( tskIDLE_PRIORITY + 4 )
#define mainQUEUE_POLL_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define mainQUEUE_BLOCK_PRIORITY	( tskIDLE_PRIORITY + 3 )
#define mainCOM_TEST_PRIORITY		( tskIDLE_PRIORITY + 2 )
#define mainSEMAPHORE_TASK_PRIORITY	( tskIDLE_PRIORITY + 1 )
#define mainGENERIC_QUEUE_PRIORITY	( tskIDLE_PRIORITY )

#define mainDEBUG_LOG_BUFFER_SIZE	( ( unsigned short ) 20480 )

/* The number of flash co-routines to create. */
#define mainNUM_FLASH_CO_ROUTINES	( 8 )

extern volatile int thread_extern_spd;

extern void print(char *str);

static void vWat (void)
{
        int a = 0, b = 0, give=0, take=0, i = 0;
        xSemaphoreHandle xSemaphore;
        freertos_print("eeeessss gud: %d, spdid = %d\n", freertos_get_thread_id(), freertos_spd_id());
        rdtscll(a);
        vSemaphoreCreateBinary(xSemaphore);
        rdtscll(b);
        freertos_print("Sem %d created with %d cycles used\n", xSemaphore, (b - a));

         printc("About to take sem\n");
         for(i; i < 1000000; i++) {
                 rdtscll(a);
                 xSemaphoreTake(xSemaphore, (TickType) 0);
                 rdtscll(b);
                 take+= (b-a);
                 rdtscll(a);
                 xSemaphoreGive(xSemaphore);
                 rdtscll(b);
                 give+= (b-a);
         }
         give = give / i;
         take = take / i;
         printc("Native Give avg = %d\n", give);
         printc("Native Take avg = %d\n", take);


        taskYIELD();
}

void vMainQueueSendPassed( void )
{
        /* This is just an example implementation of the "queue send" trace hook. */
	//	uxQueueSendPassedCount++; return;
}

portBASE_TYPE xTaskSpdCreate( pdTASK_CODE pxTaskCode, const char * const pcName, unsigned short usStackDepth, void *pvParameters, unsigned portBASE_TYPE uxPriority, xTaskHandle *pxCreatedTask)
{
        thread_extern_spd = 1;
        return xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
}

int freeRTOS_entry( void )
{
	/* CREATE ALL THE DEMO APPLICATION TASKS. */
	//vStartMathTasks( tskIDLE_PRIORITY ); 
        /* vStartCheckpointTask(); */
        //xTaskCreate( vWat, "Print", configMINIMAL_STACK_SIZE, NULL, mainPRINT_TASK_PRIORITY, NULL );
//        xTaskCreate( vWat, "wat", configMINIMAL_STACK_SIZE, NULL, mainPRINT_TASK_PRIORITY + 1, NULL );
        vStartPolledQueueTasks( mainQUEUE_POLL_PRIORITY );
/* 	vCreateBlockTimeTasks(); */
 	//vStartSemaphoreTasks( mainSEMAPHORE_TASK_PRIORITY ); 
/* 	vStartMultiEventTasks(); */
/* 	vStartQueuePeekTasks(); */
/* 	vStartBlockingQueueTasks( mainQUEUE_BLOCK_PRIORITY ); */
/* #if mainCPU_INTENSIVE_TASKS == 1 */
/* 	vStartRecursiveMutexTasks(); */
/* 	vStartDynamicPriorityTasks(); */
/* 	vStartGenericQueueTasks( mainGENERIC_QUEUE_PRIORITY ); */
/* 	vStartCountingSemaphoreTasks(); */
/* #endif */

	/* Create the co-routines that communicate with the tick hook. */
//	vStartHookCoRoutines();

	/* Create the "Print" task as described at the top of the file. */
	//xTaskCreate( vWat, "Print", configMINIMAL_STACK_SIZE, NULL, mainPRINT_TASK_PRIORITY, NULL );

	/* This task has to be created last as it keeps account of the number of tasks
	it expects to see running. */
/* #if mainUSE_SUICIDAL_TASKS_DEMO == 1 */
/* 	vCreateSuicidalTasks( mainCREATOR_TASK_PRIORITY ); */
/* #endif */

	/* Create a Task which waits to receive messages and sends its own when it times out. */
	/* xTaskCreate( prvUDPTask, "UDPRxTx", configMINIMAL_STACK_SIZE, xUDPReceiveQueue, tskIDLE_PRIORITY + 1, &hUDPTask ); */

	/* Create a Task which waits to receive messages and sends its own when it times out. */
	/* xTaskCreate( prvMessageQueueTask, "MQ RxTx", configMINIMAL_STACK_SIZE, xIPCQueue, tskIDLE_PRIORITY + 1, &hMQTask ); */

	/* Create a Task which waits to receive bytes. */
	/* xTaskCreate( prvSerialConsoleEchoTask, "SerialRx", configMINIMAL_STACK_SIZE, xSerialRxQueue, tskIDLE_PRIORITY + 4, &hSerialTask ); */
	/* Set the scheduler running.  This function will not return unless a task calls vTaskEndScheduler(). */
        xTaskSpdCreate( vWat, "Print", configMINIMAL_STACK_SIZE, NULL, 9, NULL );
        vTaskStartScheduler();
	freertos_print("END OF FREERTOS EXECUTION\n");
	return 1;
}
/*-----------------------------------------------------------*/


void vApplicationIdleHook( void )
{
	/* The co-routines are executed in the idle task using the idle task hook. */
	//	vCoRoutineSchedule();	/* Comment this out if not using Co-routines. */

/* #ifdef __GCC_POSIX__ */
/* 	struct timespec xTimeToSleep, xTimeSlept; */
/* 		/\* Makes the process more agreeable when using the Posix simulator. *\/ */
/* 		xTimeToSleep.tv_sec = 1; */
/* 		xTimeToSleep.tv_nsec = 0; */
/* 		nanosleep( &xTimeToSleep, &xTimeSlept ); */
/* #endif */
}
/*-----------------------------------------------------------*/
