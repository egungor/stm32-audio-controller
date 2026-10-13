/**
 * @file    clock_check.h
 * @brief   Boot-time measurement of the system clock against the LSE crystal.
 *
 * HAL_RCC_GetSysClockFreq() only computes the frequency from the RCC
 * register settings. ClockCheck_Measure() measures it: it counts TIM5
 * ticks (APB1 timer clock, derived from HSE through the PLL) over a fixed
 * number of periods of the independent 32.768 kHz LSE crystal, which TIM5
 * channel 4 captures through its input remap.
 */
#ifndef CLOCK_CHECK_H
#define CLOCK_CHECK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Largest accepted deviation from the configured timer clock. */
#define CLOCK_CHECK_MAX_PPM 500L

/** Result of ClockCheck_Measure(). */
typedef struct
{
    bool        passed;
    const char *fail_reason;  /**< "lse_start", "capture_timeout", "overcapture",
                                   "out_of_range"; NULL if passed */
    uint32_t    expected_hz;  /**< configured TIM5 kernel clock */
    uint32_t    measured_hz;  /**< 0 if no measurement was made */
    int32_t     error_ppm;    /**< (measured - expected) / expected, in ppm */
} ClockCheckResult;

/**
 * @brief  Enable the LSE and measure the TIM5 clock against it.
 *
 * Leaves the LSE running and TIM5 disabled. Blocks for the LSE start-up
 * time (up to LSE_STARTUP_TIMEOUT if the crystal does not start) plus
 * about 250 ms of polling. Task context only.
 */
void ClockCheck_Measure(ClockCheckResult *result);

#ifdef __cplusplus
}
#endif

#endif /* CLOCK_CHECK_H */
