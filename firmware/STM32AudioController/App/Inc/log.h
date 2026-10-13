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

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
