/**
 * @file    lcd_test.c
 * @brief   LCD bring-up test pattern drawn into the LTDC layer 0 framebuffer.
 */
#include "lcd_test.h"

#include "ltdc.h"

#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | ((b) >> 3)))

#define BAR_COUNT    8U
#define RAMP_STEPS   32U
#define MARKER_SIZE  16U
#define TARGET_INSET 20U
#define TARGET_ARM   10U
#define COLOR_WHITE  RGB565(255U, 255U, 255U)
#define COLOR_BLACK  RGB565(0U, 0U, 0U)

static const uint16_t s_bars[BAR_COUNT] = {
    RGB565(255U, 255U, 255U), /* white   */
    RGB565(255U, 255U, 0U),   /* yellow  */
    RGB565(0U, 255U, 255U),   /* cyan    */
    RGB565(0U, 255U, 0U),     /* green   */
    RGB565(255U, 0U, 255U),   /* magenta */
    RGB565(255U, 0U, 0U),     /* red     */
    RGB565(0U, 0U, 255U),     /* blue    */
    RGB565(0U, 0U, 0U),       /* black   */
};

static uint16_t pattern_pixel(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    const uint16_t white = RGB565(255U, 255U, 255U);

    if ((x == 0U) || (y == 0U) || (x == w - 1U) || (y == h - 1U))
    {
        return white;
    }
    if ((x < MARKER_SIZE) && (y < MARKER_SIZE))
    {
        return RGB565(255U, 0U, 0U);
    }
    if ((x >= w - MARKER_SIZE) && (y >= h - MARKER_SIZE))
    {
        return RGB565(0U, 0U, 255U);
    }
    if (y < (h * 3U) / 4U)
    {
        return s_bars[(x * BAR_COUNT) / w];
    }

    const uint32_t step  = (x * RAMP_STEPS) / w;
    const uint32_t level = (step * 255U) / (RAMP_STEPS - 1U);
    return RGB565(level, level, level);
}

void LcdTest_DrawPattern(LcdTestInfo *info)
{
    const LTDC_LayerCfgTypeDef *layer = &hltdc.LayerCfg[0];
    volatile uint16_t *const fb = (volatile uint16_t *)layer->FBStartAdress;
    const uint32_t w = layer->ImageWidth;
    const uint32_t h = layer->ImageHeight;
    uint32_t sum = 0U;

    for (uint32_t y = 0U; y < h; y++)
    {
        for (uint32_t x = 0U; x < w; x++)
        {
            const uint16_t px = pattern_pixel(x, y, w, h);
            fb[(y * w) + x] = px;
            sum += px;
        }
    }

    info->width  = w;
    info->height = h;
    info->sum    = sum;
}

/* Fill a rectangle, clipped to the layer. */
static void fill_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t color)
{
    const LTDC_LayerCfgTypeDef *layer = &hltdc.LayerCfg[0];
    volatile uint16_t *const fb = (volatile uint16_t *)layer->FBStartAdress;
    const int32_t w = (int32_t)layer->ImageWidth;
    const int32_t h = (int32_t)layer->ImageHeight;

    x0 = (x0 < 0) ? 0 : x0;
    y0 = (y0 < 0) ? 0 : y0;
    x1 = (x1 > w - 1) ? w - 1 : x1;
    y1 = (y1 > h - 1) ? h - 1 : y1;
    for (int32_t y = y0; y <= y1; y++)
    {
        for (int32_t x = x0; x <= x1; x++)
        {
            fb[(y * w) + x] = color;
        }
    }
}

void LcdTest_TouchTarget(uint32_t index, uint16_t *x, uint16_t *y)
{
    const LTDC_LayerCfgTypeDef *layer = &hltdc.LayerCfg[0];
    const uint16_t right = (uint16_t)(layer->ImageWidth - 1U - TARGET_INSET);
    const uint16_t bottom = (uint16_t)(layer->ImageHeight - 1U - TARGET_INSET);

    switch (index)
    {
    case 0U: *x = TARGET_INSET; *y = TARGET_INSET; break;
    case 1U: *x = right;        *y = TARGET_INSET; break;
    case 2U: *x = TARGET_INSET; *y = bottom;       break;
    case 3U: *x = right;        *y = bottom;       break;
    default:
        *x = (uint16_t)(layer->ImageWidth / 2U);
        *y = (uint16_t)(layer->ImageHeight / 2U);
        break;
    }
}

void LcdTest_DrawTouchTargets(void)
{
    const int32_t arm = (int32_t)TARGET_ARM;

    for (uint32_t i = 0U; i < LCD_TEST_TOUCH_TARGETS; i++)
    {
        uint16_t tx;
        uint16_t ty;
        int32_t cx;
        int32_t cy;

        LcdTest_TouchTarget(i, &tx, &ty);
        cx = (int32_t)tx;
        cy = (int32_t)ty;
        fill_rect(cx - arm, cy - 1, cx + arm, cy + 1, COLOR_WHITE);
        fill_rect(cx - 1, cy - arm, cx + 1, cy + arm, COLOR_WHITE);
        fill_rect(cx - arm, cy, cx + arm, cy, COLOR_BLACK);
        fill_rect(cx, cy - arm, cx, cy + arm, COLOR_BLACK);
    }
}

void LcdTest_DrawTouchDot(uint16_t x, uint16_t y)
{
    const int32_t cx = (int32_t)x;
    const int32_t cy = (int32_t)y;

    fill_rect(cx - 2, cy - 2, cx + 2, cy + 2, COLOR_WHITE);
    fill_rect(cx - 1, cy - 1, cx + 1, cy + 1, COLOR_BLACK);
}
