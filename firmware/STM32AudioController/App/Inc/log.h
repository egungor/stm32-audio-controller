/**
 * @file    log.h
 * @brief   Machine-readable line logger on the ST-LINK virtual COM port.
 *
 * Output goes to USART1 (ST-LINK VCP, 115200 8N1). Every call emits one
 * line terminated by CR LF. Line format (see docs/firmware-log.md):
 *
 *     <SUBJECT> [<ITEM>] <RESULT> [key=value ...]
 *
 * Transmission is blocking: call only from task context, never from an
 * interrupt handler. Not thread-safe yet; only one task logs today.
 * Log_Panic() is the exception for fatal errors (see below).
 */
#ifndef LOG_H
#define LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Format and send one log line (printf-style, CR LF appended).
 *
 * Lines longer than the internal buffer are truncated.
 */
void Log_Line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));

/**
 * @brief  Send one fixed line (CR LF appended) for a fatal error.
 *
 * Writes the USART1 registers directly by polling, without the HAL, the
 * tick, a format buffer or a lock, so it works with interrupts disabled
 * and in handler mode. Each byte waits a bounded time for the
 * transmitter. Meant to be called once, just before the firmware stops.
 */
void Log_Panic(const char *line);

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
