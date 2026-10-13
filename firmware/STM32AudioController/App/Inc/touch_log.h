/**
 * @file    touch_log.h
 * @brief   TOUCH DOWN / TOUCH UP / TOUCH READ FAIL log lines.
 *
 * Shared by the M1 test screen and the LVGL input device, so the touch
 * coordinates can be checked from the VCP log in both modes
 * (docs/firmware-log.md).
 */
#ifndef TOUCH_LOG_H
#define TOUCH_LOG_H

#include <stdbool.h>

#include "touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Feed one Touch_Read() result; logs press/release transitions and
 *         the first read failure of a failure streak. Task context only.
 */
void TouchLog_Track(bool read_ok, const TouchPoint *point);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_LOG_H */
