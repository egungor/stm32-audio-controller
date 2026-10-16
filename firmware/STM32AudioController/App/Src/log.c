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
/* Polls per byte in Log_Panic(): one byte at 115200 baud is about 19 000
 * cycles at 216 MHz, far below this bound. */
#define LOG_PANIC_SPIN    200000U

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

static void panic_wait(uint32_t flag)
{
    for (uint32_t spin = 0U; spin < LOG_PANIC_SPIN; spin++)
    {
        if ((huart1.Instance->ISR & flag) != 0U)
        {
            return;
        }
    }
}

static void panic_put(char c)
{
    panic_wait(USART_ISR_TXE);
    huart1.Instance->TDR = (uint8_t)c;
}

void Log_Panic(const char *line)
{
    for (uint32_t i = 0U; (line[i] != '\0') && (i < LOG_LINE_MAX - 3U); i++)
    {
        panic_put(line[i]);
    }
    panic_put('\r');
    panic_put('\n');
    panic_wait(USART_ISR_TC);
}
