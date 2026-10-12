/**
 * @file    app.h
 * @brief   Application entry point (non-generated firmware code).
 *
 * Everything under App/ is hand-written and is never touched by
 * STM32CubeMX. Generated code calls into the application only from
 * CubeMX USER CODE sections.
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Application main loop.
 *
 * Called from the FreeRTOS default task after the generated peripheral
 * and USB device initialization has run. Does not return.
 */
void App_Main(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
