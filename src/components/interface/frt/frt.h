#ifndef FRT_H
#define FRT_H

int frt_vSemaphoreBinaryCreate(void);
int frt_xSemaphoreCreateBinary(void);
int frt_xSemaphoreCreateCounting(int uxMaxCount, int uxInitialCount);
int frt_xSemaphoreCreateMutex(void);
int frt_xSemaphoreCreateRecursiveMutex(void);
int frt_xSemaphoreTake(int xSemaphore, int xTicksToWait);
int frt_xSemaphoreTakeRecursive(int xMutex, int xTicksToWait);
int frt_xSemaphoreTakeFromISR(int xSemaphore, int pxHigherPriorityTaskWoken);
int frt_xSemaphoreGive(int xSemaphore);
int frt_xSemaphoreGiveRecursive(int xMutex);
int frt_xSemaphoreGiveFromISR(int xSemaphore, int pxHigherPriorityTaskWoken);
int frt_xSemaphoreGetMutexHolder(int xSemaphore);
void frt_vSemaphoreDelete(int xSemaphore);
int frt_xTaskCreate(const char * const pcName, unsigned short usStackDepth, void *pvParameters, int uxPriority);
void frt_vTaskDelete(int xTask);
void frt_vTaskSuspend(int xTaskToSuspend);
int frt_xQueueCreate(int uxQueueLength, int uxItemSize, int ptr, spdid_t spd);
int frt_xQueueSend(int xQueue, int item, int xTicksToWait);
int frt_xQueueReceive(int xQueue, int buff, int xTicksToWait);
#endif	/* FRT_H */
