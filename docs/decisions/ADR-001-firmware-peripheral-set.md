# ADR-001: Firmware peripheral set

## Status

Proposed — awaiting developer approval. Nothing in the `.ioc` has been
changed yet.

## Context

`firmware/STM32AudioController/STM32AudioController.ioc` was created from
the STM32CubeMX **STM32F746G-DISCO board preset**. The preset enables
almost every peripheral on the board (Ethernet, audio codec, SD card,
Arduino header, …) and generates an `MX_*_Init()` call for each in
`main.c`.

The product only needs: display, touch, USB device, and a debug UART.
Every extra peripheral costs boot time, flash/RAM, interrupt and pin
budget, and makes failures during bring-up harder to isolate.

Findings from the current `.ioc` (read from the file, not tested on
hardware):

- The ST-LINK virtual COM port is **not** configured. `PB7` is labelled
  `VCP_RX [STM32F103CBT6_PA2]` and assigned to `USART1_RX`, but USART1 is
  not an enabled IP, no TX pin is assigned, and `MX_USART1_UART_Init()`
  is not generated. `USART6` (enabled) is on the Arduino header
  (`PC6`/`PC7`, labelled `ARDUINO TX/D1`, `ARDUINO RX/D0`), not on the VCP.
- SYSCLK is 200 MHz (`RCC.SYSCLKFreq_VALUE=200000000`).

## Decision (proposed)

### Keep

| Peripheral / pin | Why |
|---|---|
| LTDC (28 pins, 480×272) | LCD panel RK043FN48H |
| DMA2D | Fast fills/blits for LVGL |
| FMC SDRAM | Frame buffer at `0xC0000000` (`LTDC.FBStartAdress_L0`) |
| I2C3 (`PH7`/`PH8`) | FT5336 touch controller bus (from ST BSP; to confirm in M1) |
| `PI13` EXTI (`LCD_INT`) | Touch interrupt |
| `PI12` GPIO out (`LCD_DISP`) | LCD enable |
| `PK3` GPIO out (`LCD_BL_CTRL`) | Backlight enable |
| USB_OTG_FS + USB_DEVICE (`PA11`/`PA12`, `PA10` ID, `PJ12` VBUS) | Host link (HID; class changes in M3) |
| FREERTOS | Already the task model |
| CRC | Cheap; candidate for protocol integrity check (decided before M3) |
| SYS (SWD + SWO), TIM6 timebase, RCC | Debug and HAL tick |

### Add

| Peripheral | Why |
|---|---|
| USART1, asynchronous, RX `PB7` + TX on the VCP TX pin from UM1907 (to confirm; expected `PA9`) | Debug log over ST-LINK VCP, no extra cable |

### Disable

| Peripheral / pins | Board function |
|---|---|
| ETH + `PG2` `RMII_RXER`, `PG3` `EXT_RST` | Ethernet PHY LAN8742A |
| SAI2 + `PD6` EXTI `Audio_INT` | WM8994 audio codec |
| SPDIFRX | S/PDIF input |
| SDMMC1 + FATFS + `PC13` `uSD_Detect` | microSD card |
| QUADSPI | 128-Mbit NOR flash — not needed while assets fit in internal flash (7 % used); re-enable if LVGL assets outgrow it |
| ADC3 | Arduino A1–A5 |
| SPI2 | Arduino D11–D13 |
| I2C1 | Arduino D14/D15 |
| USART6 | Arduino D0/D1 (replaced by USART1) |
| TIM1, TIM2, TIM3, TIM5, TIM8, TIM12 | Arduino PWM pins; none needed (backlight is plain GPIO `PK3`) |
| RTC | Not needed |
| Arduino GPIO outputs (`PG6`, `PG7`, `PI2`, `PI3`, …), `PH13` `DCMI_PWR_EN` | Camera / Arduino header — return to reset state |

`PD5` (`OTG_FS_PowerSwitchOn`) drives the VBUS power switch used in USB
**host** mode. In device mode it should stay at its inactive level;
verify its configured state against the MB1191 schematic before M3.

### Clock

Keep 200 MHz for now. Moving to 216 MHz (over-drive) is a separate
decision once the display and USB clocks are verified on hardware.

## Alternatives

- **Keep the full preset** — fastest start, but every bring-up problem has
  29 possible sources and boot runs ~20 unrelated init functions.
- **Start from a bare MCU (not board) project** — loses the verified board
  pin labels that make the `.ioc` self-documenting.

## Consequences

- `main.c` keeps only the needed `MX_*_Init()` calls; flash/RAM drop.
- Re-enabling a peripheral later is a CubeMX change plus a PR.
- Generated HAL modules that are no longer used are removed by CubeMX.
- Requires regeneration with STM32CubeMX 6.18.1 (docs/toolchain.md).

## Verification after regeneration

- Linux build (`tools/build-firmware-linux.sh`) and Windows ST build:
  no errors, no new warnings.
- `main.c`: only the kept `MX_*_Init()` calls remain; USER CODE sections
  (including `App_Main()` in `freertos.c`) are preserved.
- On hardware (REQUIRES HARDWARE): board boots, USB enumerates, VCP shows
  output once logging exists.
