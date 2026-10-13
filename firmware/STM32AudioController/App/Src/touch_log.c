/**
 * @file    touch_log.c
 * @brief   TOUCH DOWN / TOUCH UP / TOUCH READ FAIL log lines.
 */
#include "touch_log.h"

#include "log.h"

static TouchPoint s_last;
static bool s_read_failed;

void TouchLog_Track(bool read_ok, const TouchPoint *point)
{
    if (!read_ok)
    {
        if (!s_read_failed)
        {
            Log_Line("TOUCH READ FAIL");
            s_read_failed = true;
        }
        return;
    }
    s_read_failed = false;

    if (point->pressed && !s_last.pressed)
    {
        Log_Line("TOUCH DOWN x=%u y=%u", (unsigned)point->x, (unsigned)point->y);
    }
    else if (!point->pressed && s_last.pressed)
    {
        Log_Line("TOUCH UP x=%u y=%u", (unsigned)s_last.x, (unsigned)s_last.y);
    }

    if (point->pressed)
    {
        s_last = *point;
    }
    else
    {
        s_last.pressed = false; /* keep the last position for TOUCH UP */
    }
}
