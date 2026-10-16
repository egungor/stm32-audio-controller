/**
 * @file    rtos_diag.h
 * @brief   FreeRTOS failure hooks and the MEM log line.
 *
 * rtos_diag.c defines vApplicationStackOverflowHook() and
 * vApplicationMallocFailedHook(), which replace CubeMX's empty __weak
 * stubs in Core/Src/freertos.c. Each logs one line (Log_Panic) and stops
 * with interrupts disabled. Lines: docs/firmware-log.md.
 */
#ifndef RTOS_DIAG_H
#define RTOS_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Log one MEM line: free stack of the calling task (words), the
 *         smallest free FreeRTOS heap since boot and the LVGL heap use.
 *
 * Task context only, after the LVGL UI is initialised (Ui_MemoryUse).
 */
void RtosDiag_LogMemory(void);

/**
 * @brief  Call from the UI loop: logs the MEM line every
 *         RTOS_DIAG_PERIOD_MS. Same context rules as RtosDiag_LogMemory().
 */
void RtosDiag_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* RTOS_DIAG_H */
