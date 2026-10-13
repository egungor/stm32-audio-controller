/**
 * @file    lcd_test.h
 * @brief   LCD bring-up test pattern drawn into the LTDC layer 0 framebuffer.
 *
 * Layout (RGB565), top to bottom:
 *   - rows 0 .. 3/4 H: 8 vertical colour bars, left to right:
 *     white, yellow, cyan, green, magenta, red, blue, black;
 *   - remaining rows: 32-step grey ramp, black (left) to white (right);
 *   - 1-pixel white border around the whole screen;
 *   - 16x16 red marker in the top-left corner, 16x16 blue marker in the
 *     bottom-right corner (orientation check).
 *
 * The touch helpers below draw over the pattern after it has been checked;
 * the pattern `sum` only covers LcdTest_DrawPattern().
 *
 * The expected image is reproduced by tools/fb2png.py --expected.
 */
#ifndef LCD_TEST_H
#define LCD_TEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t sum; /**< 32-bit sum of all 16-bit pixels (wraps) */
} LcdTestInfo;

/**
 * @brief  Draw the test pattern into the layer 0 framebuffer.
 *
 * Address, width and height come from the generated LTDC configuration.
 * The framebuffer must be in initialized SDRAM. Task context only.
 */
void LcdTest_DrawPattern(LcdTestInfo *info);

/** Number of touch targets drawn by LcdTest_DrawTouchTargets(). */
#define LCD_TEST_TOUCH_TARGETS 5U

/**
 * @brief  Centre of touch target @p index (0 .. LCD_TEST_TOUCH_TARGETS-1):
 *         top-left, top-right, bottom-left, bottom-right (20 px from the
 *         edges), then the screen centre.
 */
void LcdTest_TouchTarget(uint32_t index, uint16_t *x, uint16_t *y);

/**
 * @brief  Draw the touch targets (white cross with a black centre line)
 *         over the test pattern. Call after LcdTest_DrawPattern().
 */
void LcdTest_DrawTouchTargets(void);

/**
 * @brief  Mark a touch position: 3x3 black square inside a 5x5 white one,
 *         clipped to the screen.
 */
void LcdTest_DrawTouchDot(uint16_t x, uint16_t y);

#ifdef __cplusplus
}
#endif

#endif /* LCD_TEST_H */
