/**
 * @file    app.c
 * @brief   Application entry point (non-generated firmware code).
 */
#include "app.h"

#include "cmsis_os.h"
#include "lcd_test.h"
#include "log.h"
#include "ltdc.h"
#include "sdram.h"

/* M1 bring-up: SDRAM, then the LCD test pattern. Log lines are documented
 * in docs/firmware-log.md. */
static void bringup(void)
{
    SdramTestResult test;
    LcdTestInfo lcd;
    uint32_t t0;

    Log_Line("BOOT OK sysclk=%lu", (unsigned long)HAL_RCC_GetSysClockFreq());

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
        return;
    }
    Log_Line("SDRAM INIT OK");

    t0 = HAL_GetTick();
    Sdram_SelfTest(&test);
    if (!test.passed)
    {
        Log_Line("SELFTEST SDRAM FAIL test=%s addr=0x%08lX exp=0x%08lX got=0x%08lX",
                 test.failed_test, (unsigned long)test.address,
                 (unsigned long)test.expected, (unsigned long)test.actual);
        return;
    }
    Log_Line("SELFTEST SDRAM PASS size=%lu ms=%lu", (unsigned long)SDRAM_SIZE_BYTES,
             (unsigned long)(HAL_GetTick() - t0));

    if (hltdc.LayerCfg[0].PixelFormat != LTDC_PIXEL_FORMAT_RGB565)
    {
        Log_Line("LCD PATTERN FAIL reason=pixel_format");
        return;
    }
    LcdTest_DrawPattern(&lcd);
    __HAL_LTDC_ENABLE(&hltdc);
    Log_Line("LCD PATTERN DRAWN w=%lu h=%lu fmt=RGB565 fb=0x%08lX sum=0x%08lX",
             (unsigned long)lcd.width, (unsigned long)lcd.height,
             (unsigned long)hltdc.LayerCfg[0].FBStartAdress, (unsigned long)lcd.sum);
}

void App_Main(void)
{
    bringup();
    Log_Line("APP READY");

    /* Application features are added in later milestones (see docs/roadmap.md). */
    for (;;)
    {
        osDelay(1);
    }
}
