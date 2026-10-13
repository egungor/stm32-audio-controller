/**
 * @file    log.c
 * @brief   Machine-readable line logger on the ST-LINK virtual COM port.
 */
#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "usart.h"

#define LOG_LINE_MAX     128U
#define LOG_TX_TIMEOUT_MS 100U

static char s_line[LOG_LINE_MAX];

void Log_Line(const char *fmt, ...)
{
    va_list args;
    int len;

    va_start(args, fmt);
    len = vsnprintf(s_line, LOG_LINE_MAX - 2U, fmt, args);
    va_end(args);

    if (len < 0)
    {
        return;
    }
    if ((unsigned)len > LOG_LINE_MAX - 3U)
    {
        len = (int)(LOG_LINE_MAX - 3U); /* truncated by vsnprintf */
    }
    s_line[len++] = '\r';
    s_line[len++] = '\n';

    (void)HAL_UART_Transmit(&huart1, (const uint8_t *)s_line, (uint16_t)len,
                            LOG_TX_TIMEOUT_MS);
}
