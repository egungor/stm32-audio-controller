/**
 * @file    app.c
 * @brief   Application entry point (non-generated firmware code).
 */
#include "app.h"

#include "clock_check.h"
#include "cmsis_os.h"
#include "lcd_test.h"
#include "log.h"
#include "ltdc.h"
#include "sdram.h"
#include "touch.h"

#define TOUCH_POLL_MS 20U

/* Report the configured clock tree, then measure it against the LSE. A
 * failure is logged but does not stop the bring-up. */
static void clock_check(void)
{
    ClockCheckResult clk;

    Log_Line("CLOCK CONFIG OK sysclk=%lu hclk=%lu pclk1=%lu pclk2=%lu",
             (unsigned long)HAL_RCC_GetSysClockFreq(), (unsigned long)HAL_RCC_GetHCLKFreq(),
             (unsigned long)HAL_RCC_GetPCLK1Freq(), (unsigned long)HAL_RCC_GetPCLK2Freq());

    ClockCheck_Measure(&clk);
    if (clk.passed)
    {
        Log_Line("CLOCK MEASURE PASS ref=LSE tim=%lu exp=%lu ppm=%ld",
                 (unsigned long)clk.measured_hz, (unsigned long)clk.expected_hz,
                 (long)clk.error_ppm);
    }
    else
    {
        Log_Line("CLOCK MEASURE FAIL reason=%s ref=LSE tim=%lu exp=%lu ppm=%ld",
                 clk.fail_reason, (unsigned long)clk.measured_hz,
                 (unsigned long)clk.expected_hz, (long)clk.error_ppm);
    }
}

/* M1 bring-up: clock check, SDRAM, then the LCD test pattern. Log lines
 * are documented in docs/firmware-log.md. Returns true if the test pattern
 * is on the panel. */
static bool bringup(void)
{
    SdramTestResult test;
    LcdTestInfo lcd;
    uint32_t t0;

    Log_Line("BOOT OK sysclk=%lu", (unsigned long)HAL_RCC_GetSysClockFreq());
    clock_check();

    /* MX_LTDC_Init() starts LTDC fetches from the SDRAM framebuffer. The FMC
     * interleaves those reads with the SDRAM init commands, and a LOAD MODE
     * REGISTER issued while a bank is open is ignored by the device, which
     * then stays in its power-up burst mode (writes smear into neighbouring
     * columns; seen on hardware). Keep LTDC off until SDRAM is ready and the
     * framebuffer holds valid content. */
    __HAL_LTDC_DISABLE(&hltdc);
    HAL_Delay(1); /* let an in-flight LTDC fetch complete */

    if (!Sdram_Init())
    {
        Log_Line("SDRAM INIT FAIL");
        return false;
    }
    Log_Line("SDRAM INIT OK");

    t0 = HAL_GetTick();
    Sdram_SelfTest(&test);
    if (!test.passed)
    {
        Log_Line("SELFTEST SDRAM FAIL test=%s addr=0x%08lX exp=0x%08lX got=0x%08lX",
                 test.failed_test, (unsigned long)test.address,
                 (unsigned long)test.expected, (unsigned long)test.actual);
        return false;
    }
    Log_Line("SELFTEST SDRAM PASS size=%lu ms=%lu", (unsigned long)SDRAM_SIZE_BYTES,
             (unsigned long)(HAL_GetTick() - t0));

    if (hltdc.LayerCfg[0].PixelFormat != LTDC_PIXEL_FORMAT_RGB565)
    {
        Log_Line("LCD PATTERN FAIL reason=pixel_format");
        return false;
    }
    LcdTest_DrawPattern(&lcd);
    __HAL_LTDC_ENABLE(&hltdc);
    Log_Line("LCD PATTERN DRAWN w=%lu h=%lu fmt=RGB565 fb=0x%08lX sum=0x%08lX",
             (unsigned long)lcd.width, (unsigned long)lcd.height,
             (unsigned long)hltdc.LayerCfg[0].FBStartAdress, (unsigned long)lcd.sum);
    return true;
}

/* Touch bring-up (M1). Targets are drawn only when the panel shows the
 * test pattern. Returns true if the FT5336 answered. */
static bool touch_bringup(bool lcd_ok)
{
    uint8_t id;

    if (!Touch_Init(&id))
    {
        Log_Line("TOUCH INIT FAIL id=0x%02X", (unsigned)id);
        return false;
    }
    Log_Line("TOUCH INIT OK id=0x%02X", (unsigned)id);

    if (lcd_ok)
    {
        LcdTest_DrawTouchTargets();
        for (uint32_t i = 0U; i < LCD_TEST_TOUCH_TARGETS; i++)
        {
            uint16_t x;
            uint16_t y;

            LcdTest_TouchTarget(i, &x, &y);
            Log_Line("TOUCH TARGET n=%lu x=%u y=%u", (unsigned long)i, (unsigned)x, (unsigned)y);
        }
    }
    return true;
}

/* Log press and release transitions; mark the finger position on the panel
 * while pressed. */
static void touch_poll(bool lcd_ok)
{
    static TouchPoint s_last;
    static bool s_read_failed;
    TouchPoint now;

    if (!Touch_Read(&now))
    {
        if (!s_read_failed)
        {
            Log_Line("TOUCH READ FAIL");
            s_read_failed = true;
        }
        return;
    }
    s_read_failed = false;

    if (now.pressed && !s_last.pressed)
    {
        Log_Line("TOUCH DOWN x=%u y=%u", (unsigned)now.x, (unsigned)now.y);
    }
    else if (!now.pressed && s_last.pressed)
    {
        Log_Line("TOUCH UP x=%u y=%u", (unsigned)s_last.x, (unsigned)s_last.y);
    }

    if (now.pressed && lcd_ok)
    {
        LcdTest_DrawTouchDot(now.x, now.y);
    }
    if (now.pressed)
    {
        s_last = now;
    }
    else
    {
        s_last.pressed = false; /* keep the last position for TOUCH UP */
    }
}

void App_Main(void)
{
    const bool lcd_ok = bringup();
    const bool touch_ok = touch_bringup(lcd_ok);

    Log_Line("APP READY");

    /* Application features are added in later milestones (see docs/roadmap.md). */
    for (;;)
    {
        if (touch_ok)
        {
            touch_poll(lcd_ok);
        }
        osDelay(TOUCH_POLL_MS);
    }
}
