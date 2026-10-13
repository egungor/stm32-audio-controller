/**
 * @file    touch.h
 * @brief   FT5336 capacitive touch controller on I2C3 (polling).
 *
 * Register map, I2C address and board orientation follow the ST FT5336
 * component driver and the STM32746G-Discovery BSP (stm32746g_discovery_ts.c):
 * the controller reports the panel's long axis as Y, so X and Y are
 * swapped to get LCD pixel coordinates. Only the first touch point is used.
 */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Expected value of the chip ID register (0xA8). */
#define TOUCH_FT5336_ID 0x51U

/** Number of bytes Touch_Decode() expects: registers 0x02 .. 0x06. */
#define TOUCH_REG_BLOCK_LEN 5U

/** First touch point, in LCD pixels. */
typedef struct
{
    bool     pressed;
    uint16_t x;
    uint16_t y;
} TouchPoint;

/**
 * @brief  Read the chip ID and select polling mode.
 * @param  chip_id  receives the ID register value (0 if the read failed)
 * @return true if the controller answered with TOUCH_FT5336_ID.
 *
 * Task context only.
 */
bool Touch_Init(uint8_t *chip_id);

/**
 * @brief  Read the current state of the first touch point.
 * @return false on an I2C error (point is then reported as not pressed).
 *
 * One 5-byte I2C read (about 0.6 ms at 100 kHz). Task context only.
 */
bool Touch_Read(TouchPoint *point);

/**
 * @brief  Decode registers 0x02 .. 0x06 (TD_STATUS, P1_XH, P1_XL, P1_YH,
 *         P1_YL) into LCD coordinates, clamped to width x height.
 *
 * Hardware independent; exposed for unit tests.
 */
void Touch_Decode(const uint8_t regs[TOUCH_REG_BLOCK_LEN], uint16_t width,
                  uint16_t height, TouchPoint *point);

#ifdef __cplusplus
}
#endif

#endif /* TOUCH_H */
