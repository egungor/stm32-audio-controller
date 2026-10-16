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
#include "touch_log.h"
#include "link_port.h"
#include "rtos_diag.h"
#include "ui.h"
#include "ui_demo.h"
#include "ui_port.h"

/* 1: M1 test screen (pattern, touch targets) instead of the UI.
 * Set with cmake -DAPP_TEST_SCREEN=ON (see firmware/README.md). */
#ifndef APP_TEST_SCREEN
#define APP_TEST_SCREEN 0
#endif

/* 1: drive the UI with demo data instead of the USB host link.
 * Set with cmake -DAPP_UI_DEMO=ON. */
#ifndef APP_UI_DEMO
#define APP_UI_DEMO 0
#endif

#define TOUCH_POLL_MS  20U
#define UI_MIN_WAIT_MS 1U
#define UI_MAX_WAIT_MS 10U

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

/* Clock check, then SDRAM. Log lines are documented in
 * docs/firmware-log.md. Returns true if the SDRAM is usable. */
static bool bringup(void)
{
    SdramTestResult test;
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
    return true;
}

static bool touch_init(void)
{
    uint8_t id;

    if (!Touch_Init(&id))
    {
        Log_Line("TOUCH INIT FAIL id=0x%02X", (unsigned)id);
        return false;
    }
    Log_Line("TOUCH INIT OK id=0x%02X", (unsigned)id);
    return true;
}

#if APP_TEST_SCREEN

/* M1 test screen: pattern, touch targets, dot under the finger. */
static void run_test_screen(bool sdram_ok, bool touch_ok)
{
    bool lcd_ok = false;
    LcdTestInfo lcd;

    if (!sdram_ok)
    {
        /* LTDC stays off: no valid framebuffer. */
    }
    else if (hltdc.LayerCfg[0].PixelFormat != LTDC_PIXEL_FORMAT_RGB565)
    {
        Log_Line("LCD PATTERN FAIL reason=pixel_format");
    }
    else
    {
        LcdTest_DrawPattern(&lcd);
        __HAL_LTDC_ENABLE(&hltdc);
        Log_Line("LCD PATTERN DRAWN w=%lu h=%lu fmt=RGB565 fb=0x%08lX sum=0x%08lX",
                 (unsigned long)lcd.width, (unsigned long)lcd.height,
                 (unsigned long)hltdc.LayerCfg[0].FBStartAdress, (unsigned long)lcd.sum);
        lcd_ok = true;
    }

    if (lcd_ok && touch_ok)
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
    Log_Line("APP READY");

    for (;;)
    {
        if (touch_ok)
        {
            TouchPoint now;
            const bool ok = Touch_Read(&now);

            TouchLog_Track(ok, &now);
            if (ok && now.pressed && lcd_ok)
            {
                LcdTest_DrawTouchDot(now.x, now.y);
            }
        }
        osDelay(TOUCH_POLL_MS);
    }
}

#else /* !APP_TEST_SCREEN */

/* Flip button on the touchscreen: turn the image by 180°. With the host
 * link the state is part of the settings record the host stores. */
static void on_ui_flip(void *user)
{
    (void)user;
#if APP_UI_DEMO
    UiDemo_ToggleFlip();
#else
    LinkPort_ToggleFlip();
#endif
}

/* Screen timeout (settings.screen_timeout): the UI decides, the port
 * switches the backlight. */
static void on_ui_backlight(bool on, void *user)
{
    (void)user;
    UiPort_SetBacklight(on);
}

/* LVGL UI, fed by the USB host link (or by demo data with APP_UI_DEMO). */
static void run_ui(bool sdram_ok, bool touch_ok)
{
    const bool ui_ok = sdram_ok && UiPort_Init(touch_ok);

    if (!ui_ok)
    {
        Log_Line("UI INIT FAIL reason=%s", sdram_ok ? "lcd_format" : "sdram");
    }
    else
    {
#if APP_UI_DEMO
        UiDemo_Start();
#else
        LinkPort_Init();
#endif
        Ui_SetFlipHandler(on_ui_flip, NULL);
        Ui_SetBacklightHandler(on_ui_backlight, NULL);
        UiPort_Start();
        Log_Line("UI INIT OK fb0=0x%08lX fb1=0x%08lX", (unsigned long)UI_PORT_FB0_ADDR,
                 (unsigned long)UI_PORT_FB1_ADDR);
        RtosDiag_LogMemory();
    }
    Log_Line("APP READY");

    for (;;)
    {
        uint32_t wait = UI_MAX_WAIT_MS;

        if (ui_ok)
        {
#if !APP_UI_DEMO
            LinkPort_Run();
#endif
            wait = UiPort_Run();
            RtosDiag_Run();
        }

        wait = (wait < UI_MIN_WAIT_MS) ? UI_MIN_WAIT_MS : wait;
        wait = (wait > UI_MAX_WAIT_MS) ? UI_MAX_WAIT_MS : wait;
        osDelay(wait);
    }
}

#endif /* APP_TEST_SCREEN */

void App_Main(void)
{
    const bool sdram_ok = bringup();
    const bool touch_ok = touch_init();

#if APP_TEST_SCREEN
    run_test_screen(sdram_ok, touch_ok);
#else
    run_ui(sdram_ok, touch_ok);
#endif
}
