/**
 * @file    ui_port.c
 * @brief   LVGL port for the STM32F746G-DISCO: LTDC display, FT5336 input.
 */
#include "ui_port.h"

#include "cmsis_os.h"
#include "log.h"
#include "lvgl.h"
#include "ltdc.h"
#include "touch.h"
#include "touch_log.h"

#define LCD_WIDTH        480U
#define LCD_HEIGHT       272U
#define FB_BYTES         (LCD_WIDTH * LCD_HEIGHT * 2U)
#define SWAP_TIMEOUT_MS  50U /* three frames at ~59 Hz */

static lv_display_t *s_display;

/* Flipped image (board upside down). The LTDC cannot rotate and the DMA2D
 * cannot copy backwards, so LVGL keeps drawing into FB0/FB1 as usual and
 * each finished frame is copied, turned by 180°, into FB2 or FB3, which
 * the LTDC then scans out. All of this runs in the UI task. */
static bool     s_flipped;
static uint32_t s_shown;        /* address the LTDC scans out */
static uint32_t s_copy_max_ms;  /* longest copy since the last toggle */

/* Turning a framebuffer by 180° reverses the order of its pixels. With two
 * RGB565 pixels per word (the pixel count is even): reverse the words and
 * swap the two halves of each.
 *
 * The SDRAM is not cached and all four framebuffers lie in the same SDRAM
 * bank, so alternating single reads from FB0/FB1 with single writes to
 * FB2/FB3 keep switching SDRAM rows (measured 22 ms per
 * frame, #51). Instead each 1 KB chunk is read in bursts into SRAM, then
 * written turned in bursts (4.8 ms).
 * Both loops are assembly so their speed does not depend on the build
 * type (Debug is -O0). r7 is left alone: it is the -O0 frame pointer. */
#define TURN_CHUNK_WORDS 256U /* 1 KB, two SDRAM rows */

_Static_assert(((FB_BYTES / 4U) % TURN_CHUNK_WORDS) == 0U, "framebuffer in whole chunks");

static uint32_t s_turn_chunk[TURN_CHUNK_WORDS];

/* words: multiple of 8, not 0. */
static void copy_words(uint32_t *dst, const uint32_t *src, uint32_t words)
{
    __asm volatile("1:\n\t"
                   "ldmia %[src]!, {r4-r6, r8-r12}\n\t"
                   "stmia %[dst]!, {r4-r6, r8-r12}\n\t"
                   "subs  %[n], %[n], #8\n\t"
                   "bne   1b"
                   : [src] "+r"(src), [dst] "+r"(dst), [n] "+r"(words)
                   :
                   : "r4", "r5", "r6", "r8", "r9", "r10", "r11", "r12", "cc", "memory");
}

/* dst[i] = ROR(src_end[-1 - i], 16); words: multiple of 4, not 0. */
static void turn_words(uint32_t *dst, const uint32_t *src_end, uint32_t words)
{
    __asm volatile("1:\n\t"
                   "ldmdb %[src]!, {r4-r6, r8}\n\t"
                   "ror   r9, r8, #16\n\t"
                   "ror   r10, r6, #16\n\t"
                   "ror   r11, r5, #16\n\t"
                   "ror   r12, r4, #16\n\t"
                   "stmia %[dst]!, {r9-r12}\n\t"
                   "subs  %[n], %[n], #4\n\t"
                   "bne   1b"
                   : [src] "+r"(src_end), [dst] "+r"(dst), [n] "+r"(words)
                   :
                   : "r4", "r5", "r6", "r8", "r9", "r10", "r11", "r12", "cc", "memory");
}

static void copy_turned(uint32_t *dst, const uint32_t *src)
{
    const uint32_t words = FB_BYTES / sizeof(uint32_t);

    for (uint32_t done = 0U; done < words; done += TURN_CHUNK_WORDS)
    {
        copy_words(s_turn_chunk, &src[done], TURN_CHUNK_WORDS);
        turn_words(&dst[words - done - TURN_CHUNK_WORDS], &s_turn_chunk[TURN_CHUNK_WORDS],
                   TURN_CHUNK_WORDS);
    }
}

static uint32_t tick_cb(void)
{
    return HAL_GetTick();
}

/* Direct mode: LVGL has drawn into px_map (the whole hidden framebuffer)
 * and keeps the two buffers in sync itself. On the last area of a frame,
 * show that buffer from the next vertical blanking on. */
static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    LV_UNUSED(area);

    if (lv_display_flush_is_last(disp))
    {
        uint32_t start = HAL_GetTick();
        uint32_t show = (uint32_t)px_map;
        uint32_t copy_ms;

        if (s_flipped)
        {
            /* Into the scan-out buffer that is not shown. */
            show = (s_shown == UI_PORT_FB2_ADDR) ? UI_PORT_FB3_ADDR : UI_PORT_FB2_ADDR;
            copy_turned((uint32_t *)show, (const uint32_t *)px_map);
            copy_ms = HAL_GetTick() - start;
            s_copy_max_ms = (copy_ms > s_copy_max_ms) ? copy_ms : s_copy_max_ms;
            start += copy_ms; /* the swap timeout starts after the copy */
        }
        s_shown = show;

        (void)HAL_LTDC_SetAddress_NoReload(&hltdc, show, 0U);
        if ((hltdc.Instance->GCR & LTDC_GCR_LTDCEN) == 0U)
        {
            /* No scan-out yet (first frame): no blanking to wait for. */
            (void)HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_IMMEDIATE);
        }
        else
        {
            (void)HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING);

            /* VBR is cleared by hardware once the shadow registers are
             * loaded. Until then the old buffer is still scanned out and
             * must not be drawn into. */
            while (((hltdc.Instance->SRCR & LTDC_SRCR_VBR) != 0U) &&
                   ((HAL_GetTick() - start) < SWAP_TIMEOUT_MS))
            {
                osDelay(1);
            }
        }
    }
    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    TouchPoint point;
    const bool ok = Touch_Read(&point);

    LV_UNUSED(indev);
    TouchLog_Track(ok, &point);
    if (point.pressed)
    {
        /* Touch_Read clamps to the panel, so the mirrored point is too. */
        data->point.x = s_flipped ? (int32_t)(LCD_WIDTH - 1U - point.x) : (int32_t)point.x;
        data->point.y = s_flipped ? (int32_t)(LCD_HEIGHT - 1U - point.y) : (int32_t)point.y;
    }
    /* On release LVGL keeps the last pressed point. */
    data->state = point.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bool UiPort_Init(bool touch_ok)
{
    const LTDC_LayerCfgTypeDef *layer = &hltdc.LayerCfg[0];

    if ((layer->PixelFormat != LTDC_PIXEL_FORMAT_RGB565) || (layer->ImageWidth != LCD_WIDTH) ||
        (layer->ImageHeight != LCD_HEIGHT))
    {
        return false;
    }

    lv_init();
    lv_tick_set_cb(tick_cb);

    s_display = lv_display_create((int32_t)LCD_WIDTH, (int32_t)LCD_HEIGHT);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_display, (void *)UI_PORT_FB0_ADDR, (void *)UI_PORT_FB1_ADDR,
                           FB_BYTES, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_display, flush_cb);

    if (touch_ok)
    {
        lv_indev_t *indev = lv_indev_create();

        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, touch_read_cb);
    }
    return true;
}

void UiPort_Start(void)
{
    /* The first frame selects a framebuffer through flush_cb; only then is
     * there valid content to scan out. */
    lv_refr_now(s_display);
    __HAL_LTDC_ENABLE(&hltdc);
}

void UiPort_SetFlipped(bool flipped)
{
    if (flipped == s_flipped)
    {
        return;
    }
    /* The copy times belong to the orientation being left. */
    Log_Line("UI FLIP %s copy_max_ms=%lu", flipped ? "on" : "off",
             (unsigned long)s_copy_max_ms);
    s_copy_max_ms = 0U;
    s_flipped = flipped;
    /* Redraw so the next frame is shown in the new orientation. */
    lv_obj_invalidate(lv_display_get_screen_active(s_display));
}

bool UiPort_IsFlipped(void)
{
    return s_flipped;
}

uint32_t UiPort_Run(void)
{
    return lv_timer_handler();
}
