/**
 * @file    sdram.c
 * @brief   External SDRAM (MT48LC4M32B2B5-6A on FMC bank 1) bring-up.
 *
 * Init sequence follows the device datasheet power-up procedure, as also
 * done by the STM32CubeF7 BSP (stm32746g_discovery_sdram.c).
 */
#include "sdram.h"

#include "fmc.h"

/* SDRAM mode register fields (JEDEC). */
#define SDRAM_MODEREG_BURST_LENGTH_1          0x0000U
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   0x0000U
#define SDRAM_MODEREG_CAS_LATENCY_3           0x0030U
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000U
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  0x0200U

#define SDRAM_CMD_TIMEOUT 0xFFFFU

/*
 * Refresh counter = refresh period x SDCLK - 20.
 * 64 ms / 4096 rows = 15.625 us; SDCLK = HCLK / 2 = 100 MHz
 * (HCLK 200 MHz, FMC_SDRAM_CLOCK_PERIOD_2) -> 1562 - 20 = 1542.
 * Must be recomputed if HCLK or SDClockPeriod changes.
 */
#define SDRAM_REFRESH_COUNT 1542U

/* Number of auto-refresh cycles issued during init (datasheet: >= 2). */
#define SDRAM_INIT_AUTOREFRESH 8U

static bool send_command(uint32_t mode, uint32_t refresh_number, uint32_t mode_reg)
{
    FMC_SDRAM_CommandTypeDef cmd = {0};

    cmd.CommandMode            = mode;
    cmd.CommandTarget          = FMC_SDRAM_CMD_TARGET_BANK1;
    cmd.AutoRefreshNumber      = refresh_number;
    cmd.ModeRegisterDefinition = mode_reg;

    return HAL_SDRAM_SendCommand(&hsdram1, &cmd, SDRAM_CMD_TIMEOUT) == HAL_OK;
}

bool Sdram_Init(void)
{
    /* 1. Start the SDRAM clock (CKE high). */
    if (!send_command(FMC_SDRAM_CMD_CLK_ENABLE, 1U, 0U))
    {
        return false;
    }

    /* 2. Power-up delay: datasheet requires >= 100 us. */
    HAL_Delay(1);

    /* 3. Precharge all banks. */
    if (!send_command(FMC_SDRAM_CMD_PALL, 1U, 0U))
    {
        return false;
    }

    /* 4. Auto-refresh cycles. */
    if (!send_command(FMC_SDRAM_CMD_AUTOREFRESH_MODE, SDRAM_INIT_AUTOREFRESH, 0U))
    {
        return false;
    }

    /* 5. Load mode register: burst 1, sequential, CL3, single write burst. */
    if (!send_command(FMC_SDRAM_CMD_LOAD_MODE, 1U,
                      SDRAM_MODEREG_BURST_LENGTH_1 |
                      SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
                      SDRAM_MODEREG_CAS_LATENCY_3 |
                      SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                      SDRAM_MODEREG_WRITEBURST_MODE_SINGLE))
    {
        return false;
    }

    /* 6. Program the refresh rate. */
    return HAL_SDRAM_ProgramRefreshRate(&hsdram1, SDRAM_REFRESH_COUNT) == HAL_OK;
}

static void set_fail(SdramTestResult *r, const char *test, volatile uint32_t *addr,
                     uint32_t expected, uint32_t actual)
{
    r->passed      = false;
    r->failed_test = test;
    r->address     = (uint32_t)addr;
    r->expected    = expected;
    r->actual      = actual;
}

/* Walking-ones on one word: finds stuck or shorted data lines. */
static bool test_data_bus(SdramTestResult *r)
{
    volatile uint32_t *const p = (volatile uint32_t *)SDRAM_BASE_ADDR;

    for (uint32_t pattern = 1U; pattern != 0U; pattern <<= 1)
    {
        *p = pattern;
        const uint32_t got = *p;
        if (got != pattern)
        {
            set_fail(r, "data", p, pattern, got);
            return false;
        }
    }
    return true;
}

/* Power-of-two word offsets: finds stuck or shorted address lines. */
static bool test_addr_bus(SdramTestResult *r)
{
    volatile uint32_t *const base = (volatile uint32_t *)SDRAM_BASE_ADDR;
    const uint32_t words   = SDRAM_SIZE_BYTES / sizeof(uint32_t);
    /* Halves differ so that halfword (column A0) aliasing is also caught. */
    const uint32_t pattern = 0xAAAA5555U;
    const uint32_t anti    = 0x5555AAAAU;

    for (uint32_t off = 1U; off < words; off <<= 1)
    {
        base[off] = pattern;
    }
    base[0] = anti;

    /* Address lines stuck high (an offset aliases to 0). */
    for (uint32_t off = 1U; off < words; off <<= 1)
    {
        const uint32_t got = base[off];
        if (got != pattern)
        {
            set_fail(r, "addr", &base[off], pattern, got);
            return false;
        }
    }
    base[0] = pattern;

    /* Address lines stuck low or shorted (offsets alias to each other). */
    for (uint32_t test = 1U; test < words; test <<= 1)
    {
        base[test] = anti;
        for (uint32_t off = 1U; off < words; off <<= 1)
        {
            const uint32_t got = base[off];
            if ((off != test) && (got != pattern))
            {
                set_fail(r, "addr", &base[off], pattern, got);
                return false;
            }
        }
        base[test] = pattern;
    }
    return true;
}

/* Fill every word with an address-derived value, then read back. */
static bool test_fill(SdramTestResult *r, uint32_t xor_mask)
{
    volatile uint32_t *const base = (volatile uint32_t *)SDRAM_BASE_ADDR;
    const uint32_t words = SDRAM_SIZE_BYTES / sizeof(uint32_t);

    for (uint32_t i = 0U; i < words; i++)
    {
        base[i] = i ^ xor_mask;
    }
    for (uint32_t i = 0U; i < words; i++)
    {
        const uint32_t got = base[i];
        if (got != (i ^ xor_mask))
        {
            set_fail(r, "fill", &base[i], i ^ xor_mask, got);
            return false;
        }
    }
    return true;
}

void Sdram_SelfTest(SdramTestResult *result)
{
    result->passed      = true;
    result->failed_test = NULL;
    result->address     = 0U;
    result->expected    = 0U;
    result->actual      = 0U;

    (void)(test_data_bus(result) &&
           test_addr_bus(result) &&
           test_fill(result, 0x00000000U) &&
           test_fill(result, 0xFFFFFFFFU));
}
