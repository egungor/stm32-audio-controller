/**
 * @file    rtos_diag.c
 * @brief   FreeRTOS failure hooks and the MEM log line.
 */
#include "rtos_diag.h"

#include <stdint.h>
#include <string.h>

#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "log.h"
#include "ui.h"

#define RTOS_DIAG_PERIOD_MS 60000U

/* Room for "RTOS STACK OVERFLOW task=" and a task name. */
#define PANIC_LINE_MAX (32U + configMAX_TASK_NAME_LEN)

static TickType_t s_last_tick;

void RtosDiag_LogMemory(void)
{
    uint32_t lvgl_used = 0U;
    uint32_t lvgl_peak = 0U;

    Ui_MemoryUse(&lvgl_used, &lvgl_peak);
    Log_Line("MEM stack_free=%lu rtos_heap_min=%lu lvgl_used=%lu lvgl_peak=%lu",
             (unsigned long)uxTaskGetStackHighWaterMark(NULL),
             (unsigned long)xPortGetMinimumEverFreeHeapSize(), (unsigned long)lvgl_used,
             (unsigned long)lvgl_peak);
    s_last_tick = xTaskGetTickCount();
}

void RtosDiag_Run(void)
{
    if ((TickType_t)(xTaskGetTickCount() - s_last_tick) >= pdMS_TO_TICKS(RTOS_DIAG_PERIOD_MS))
    {
        RtosDiag_LogMemory();
    }
}

/* Called by the kernel during a context switch (PendSV, handler mode)
 * when configCHECK_FOR_STACK_OVERFLOW detects an overflow. The task's
 * memory may be corrupt: copy at most configMAX_TASK_NAME_LEN printable
 * characters of the name, log without the HAL, then stop with all
 * interrupts masked (PRIMASK), not only those below the syscall level. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    static const char prefix[] = "RTOS STACK OVERFLOW task=";
    char line[PANIC_LINE_MAX];
    size_t len = sizeof(prefix) - 1U;

    (void)xTask;
    __disable_irq();

    memcpy(line, prefix, len);
    for (size_t i = 0U; (pcTaskName != NULL) && (i < configMAX_TASK_NAME_LEN); i++)
    {
        const char c = pcTaskName[i];

        if (c == '\0')
        {
            break;
        }
        line[len++] = ((c > ' ') && (c < 0x7F)) ? c : '?';
    }
    line[len] = '\0';
    Log_Panic(line);

    for (;;)
    {
    }
}

/* Called by pvPortMalloc() when the FreeRTOS heap is exhausted (task
 * context, scheduler suspended). Log, then stop. */
void vApplicationMallocFailedHook(void)
{
    __disable_irq();
    Log_Panic("RTOS MALLOC FAILED");

    for (;;)
    {
    }
}
