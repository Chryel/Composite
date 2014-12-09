/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub_simple_stack.h>
.text	

cos_asm_server_stub(frt_vSemaphoreBinaryCreate)
cos_asm_server_stub(frt_xSemaphoreTake)
cos_asm_server_stub(frt_xSemaphoreGive)
cos_asm_server_stub(frt_xTaskCreate)
cos_asm_server_stub(frt_vTaskDelete)
cos_asm_server_stub(frt_vTaskSuspend)
cos_asm_server_stub(frt_xQueueCreate)
cos_asm_server_stub(frt_xQueueSend)
cos_asm_server_stub(frt_xQueueReceive)
