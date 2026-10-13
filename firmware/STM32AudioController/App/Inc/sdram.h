/**
 * @file    sdram.h
 * @brief   External SDRAM (MT48LC4M32B2B5-6A on FMC bank 1) bring-up.
 *
 * STM32CubeMX configures the FMC controller (MX_FMC_Init) but does not
 * generate the JEDEC power-up command sequence for the memory device.
 * Sdram_Init() issues that sequence; it must run after MX_FMC_Init() and
 * before anything reads or writes SDRAM.
 */
#ifndef SDRAM_H
#define SDRAM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** SDRAM base address (FMC SDRAM bank 1). */
#define SDRAM_BASE_ADDR  0xC0000000UL

/** Usable size: 16-bit bus x 4096 rows x 256 columns x 4 banks. */
#define SDRAM_SIZE_BYTES (8UL * 1024UL * 1024UL)

/** Result of the SDRAM self-test. */
typedef struct
{
    bool        passed;
    const char *failed_test; /**< "data", "addr" or "fill"; NULL if passed */
    uint32_t    address;     /**< first failing address */
    uint32_t    expected;
    uint32_t    actual;
} SdramTestResult;

/**
 * @brief  Run the JEDEC power-up sequence and program the refresh rate.
 * @return true on success, false if the FMC rejected a command.
 *
 * Blocks for about 1 ms. Task context only.
 */
bool Sdram_Init(void);

/**
 * @brief  Destructive test of the whole SDRAM: data bus, address bus and a
 *         full write/read-back pass. Overwrites all SDRAM contents.
 */
void Sdram_SelfTest(SdramTestResult *result);

#ifdef __cplusplus
}
#endif

#endif /* SDRAM_H */
