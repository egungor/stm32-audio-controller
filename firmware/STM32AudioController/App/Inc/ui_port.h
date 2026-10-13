/**
 * @file    ui_port.h
 * @brief   LVGL port for the STM32F746G-DISCO: LTDC display, FT5336 input.
 *
 * Display: two RGB565 framebuffers in SDRAM, LVGL direct mode. LVGL draws
 * into the hidden buffer; at the end of a frame the LTDC layer 0 address
 * is switched during vertical blanking, so the panel never shows a half
 * drawn frame.
 *
 * Flipped (UiPort_SetFlipped): each finished frame is also copied, turned
 * by 180°, into FB2 or FB3 and those are scanned out instead; touch
 * coordinates are mirrored to match. The LVGL layout does not change.
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

/** Framebuffers in SDRAM (480 x 272 x 2 bytes each, 256 KB aligned).
 *  FB0/FB1: LVGL draws here. FB2/FB3: turned copies, only when flipped. */
#define UI_PORT_FB0_ADDR 0xC0000000UL
#define UI_PORT_FB1_ADDR 0xC0040000UL
#define UI_PORT_FB2_ADDR 0xC0080000UL
#define UI_PORT_FB3_ADDR 0xC00C0000UL

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
 * @brief  Show the image turned by 180° (true) or normally (false).
 *
 * Takes effect with the next frame. Not stored: every boot starts normal.
 * Call from the UI task.
 */
void UiPort_SetFlipped(bool flipped);

/** True while the image is turned by 180°. */
bool UiPort_IsFlipped(void);

/**
 * @brief  Run LVGL timers, input and rendering once.
 * @return milliseconds until LVGL needs to run again.
 */
uint32_t UiPort_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_PORT_H */
