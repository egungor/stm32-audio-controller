/**
 * @file    ui_port.h
 * @brief   LVGL port for the STM32F746G-DISCO: LTDC display, FT5336 input.
 *
 * Display: two RGB565 framebuffers in SDRAM, LVGL direct mode. LVGL draws
 * into the hidden buffer; at the end of a frame the LTDC layer 0 address
 * is switched during vertical blanking, so the panel never shows a half
 * drawn frame.
 *
 * Input: FT5336 polled through Touch_Read() each time LVGL reads the
 * pointer (every LV_DEF_REFR_PERIOD ms).
 *
 * All LVGL calls must come from the task that calls UiPort_Run().
 */
#ifndef UI_PORT_H
#define UI_PORT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Framebuffers in SDRAM (480 x 272 x 2 bytes each, 256 KB aligned). */
#define UI_PORT_FB0_ADDR 0xC0000000UL
#define UI_PORT_FB1_ADDR 0xC0040000UL

/**
 * @brief  Initialise LVGL, the display and (if @p touch_ok) the pointer
 *         input device.
 * @return false if the LTDC layer is not RGB565 480 x 272.
 *
 * Requires initialised SDRAM. The LTDC stays disabled until the first
 * frame has been drawn (UiPort_Start).
 */
bool UiPort_Init(bool touch_ok);

/** Draw the first frame and enable the LTDC. Call after building the UI. */
void UiPort_Start(void);

/**
 * @brief  Run LVGL timers, input and rendering once.
 * @return milliseconds until LVGL needs to run again.
 */
uint32_t UiPort_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_PORT_H */
