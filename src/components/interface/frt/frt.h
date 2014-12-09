#ifndef FRT_H
#define FRT_H

extern int frt_vSemaphoreBinaryCreate(void);
extern int frt_xSemaphoreTake(int xSemaphore, int xTicksToWait);
extern int frt_xSemaphoreGive(int xSemaphore);
extern int frt_xTaskCreate(const char * const pcName, unsigned short usStackDepth, void *pvParameters, int uxPriority);
extern void frt_vTaskDelete(int xTask);
extern void frt_vTaskSuspend(int xTaskToSuspend);
extern int frt_xQueueCreate(int uxQueueLength, int uxItemSize, int ptr, spdid_t spd);
extern int frt_xQueueSend(int xQueue, int item, int xTicksToWait);
extern int frt_xQueueReceive(int xQueue, int buff, int xTicksToWait);
#endif	/* FRT_H */
