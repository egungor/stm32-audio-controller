/**
 * @file    clock_check.c
 * @brief   Boot-time measurement of the system clock against the LSE crystal.
 */
#include "clock_check.h"

#include "stm32f7xx_hal.h"

/* Input capture prescaler /8: one capture every 8 LSE periods (~244 us),
 * far longer than any interrupt handler, so polling never misses one. */
#define LSE_PERIODS_PER_CAPTURE 8U
#define CAPTURE_COUNT           1024U /* 8192 LSE periods = 250 ms */
#define MEASURE_TIMEOUT_MS      1000U

/* APB1 timers run at 2 x PCLK1 when the APB1 prescaler is not 1. */
static uint32_t tim5_kernel_hz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) == RCC_CFGR_PPRE1_DIV1)
    {
        return pclk1;
    }
    return 2U * pclk1;
}

static bool lse_start(void)
{
    RCC_OscInitTypeDef osc = {0};

    osc.OscillatorType = RCC_OSCILLATORTYPE_LSE;
    osc.LSEState = RCC_LSE_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    return HAL_RCC_OscConfig(&osc) == HAL_OK;
}

/* Wait for the next channel 4 capture; reading CCR4 clears CC4IF. */
static bool next_capture(uint32_t deadline, uint32_t *value)
{
    while ((TIM5->SR & TIM_SR_CC4IF) == 0U)
    {
        if ((int32_t)(HAL_GetTick() - deadline) >= 0)
        {
            return false;
        }
    }
    *value = TIM5->CCR4;
    return true;
}

static void tim5_start(void)
{
    __HAL_RCC_TIM5_CLK_ENABLE();
    TIM5->CR1 = 0U;
    TIM5->PSC = 0U;
    TIM5->ARR = 0xFFFFFFFFU;
    TIM5->OR = TIM_TIM5_LSE;                     /* TI4 <- LSE */
    TIM5->CCMR2 = TIM_CCMR2_CC4S_0               /* CC4 input, IC4 on TI4 */
                  | TIM_CCMR2_IC4PSC_0 | TIM_CCMR2_IC4PSC_1; /* /8 */
    TIM5->CCER = TIM_CCER_CC4E;                  /* rising edge */
    TIM5->EGR = TIM_EGR_UG;
    TIM5->SR = 0U;
    TIM5->CR1 = TIM_CR1_CEN;
}

static void tim5_stop(void)
{
    TIM5->CR1 = 0U;
    TIM5->CCER = 0U;
    TIM5->OR = TIM_TIM5_GPIO;
    __HAL_RCC_TIM5_CLK_DISABLE();
}

void ClockCheck_Measure(ClockCheckResult *result)
{
    uint32_t first;
    uint32_t last = 0U;
    uint32_t deadline;
    uint32_t ticks;
    uint64_t measured;
    int64_t error_ppm;
    uint32_t i;

    result->passed = false;
    result->fail_reason = NULL;
    result->expected_hz = tim5_kernel_hz();
    result->measured_hz = 0U;
    result->error_ppm = 0;

    if (!lse_start())
    {
        result->fail_reason = "lse_start";
        return;
    }

    tim5_start();
    deadline = HAL_GetTick() + MEASURE_TIMEOUT_MS;

    /* The first capture only sets the reference point. */
    if (!next_capture(deadline, &first))
    {
        tim5_stop();
        result->fail_reason = "capture_timeout";
        return;
    }
    TIM5->SR = ~TIM_SR_CC4OF; /* rc_w0: clear only the overcapture flag */

    for (i = 0U; i < CAPTURE_COUNT; i++)
    {
        if (!next_capture(deadline, &last))
        {
            tim5_stop();
            result->fail_reason = "capture_timeout";
            return;
        }
    }
    if ((TIM5->SR & TIM_SR_CC4OF) != 0U)
    {
        tim5_stop();
        result->fail_reason = "overcapture";
        return;
    }
    tim5_stop();

    /* ticks / (CAPTURE_COUNT * LSE_PERIODS_PER_CAPTURE / LSE_VALUE) */
    ticks = last - first; /* modulo 2^32; 250 ms at 100 MHz fits */
    measured = ((uint64_t)ticks * LSE_VALUE)
               / (CAPTURE_COUNT * LSE_PERIODS_PER_CAPTURE);
    error_ppm = (((int64_t)measured - (int64_t)result->expected_hz) * 1000000LL)
                / (int64_t)result->expected_hz;

    result->measured_hz = (measured > UINT32_MAX) ? UINT32_MAX : (uint32_t)measured;
    if ((error_ppm > CLOCK_CHECK_MAX_PPM) || (error_ppm < -CLOCK_CHECK_MAX_PPM))
    {
        /* Saturate: a grossly wrong clock must not wrap the logged value. */
        result->error_ppm = (error_ppm > INT32_MAX) ? INT32_MAX
                          : (error_ppm < INT32_MIN) ? INT32_MIN
                          : (int32_t)error_ppm;
        result->fail_reason = "out_of_range";
        return;
    }
    result->error_ppm = (int32_t)error_ppm;
    result->passed = true;
}
