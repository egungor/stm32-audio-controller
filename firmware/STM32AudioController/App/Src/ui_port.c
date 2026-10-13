/**
 * @file    ui_port.c
 * @brief   LVGL port for the STM32F746G-DISCO: LTDC display, FT5336 input.
 */
#include "ui_port.h"

#include "cmsis_os.h"
#include "lvgl.h"
#include "ltdc.h"
#include "touch.h"
#include "touch_log.h"

#define LCD_WIDTH        480U
#define LCD_HEIGHT       272U
#define FB_BYTES         (LCD_WIDTH * LCD_HEIGHT * 2U)
#define SWAP_TIMEOUT_MS  50U /* three frames at ~59 Hz */

static lv_display_t *s_display;

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
        const uint32_t start = HAL_GetTick();

        (void)HAL_LTDC_SetAddress_NoReload(&hltdc, (uint32_t)px_map, 0U);
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
        data->point.x = (int32_t)point.x;
        data->point.y = (int32_t)point.y;
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

uint32_t UiPort_Run(void)
{
    return lv_timer_handler();
}
