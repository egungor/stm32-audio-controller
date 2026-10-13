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
