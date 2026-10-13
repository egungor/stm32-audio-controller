# Hardware

## Target Board

| Item | Value | Status |
|---|---|---|
| Board | STM32F746G-DISCO (ST order code: 32F746GDISCOVERY) | VERIFIED (physical board) |
| Board reference | MB1191 | VERIFIED (physical board) |
| MCU | STM32F746NGH6 | VERIFIED (physical board) |
| Core | Arm Cortex-M7, up to 216 MHz | From ST documentation |
| Internal memory | 1 Mbyte flash, 340 Kbytes RAM | From ST documentation |

> Note: earlier revisions of this repository referred to the MCU as
> STM32F756NGH6. That was incorrect. The verified MCU is **STM32F746NGH6**.

## Display

| Item | Value | Status |
|---|---|---|
| Size | 4.3" color TFT LCD | VERIFIED (physical board) |
| Resolution | 480 × 272 | VERIFIED (physical board) |
| Panel | ROCKTECH RK043FN48H-CT672B | VERIFIED (physical board) |
| MCU interface | LTDC, parallel RGB (LTDC_R0–R7, G0–G7, B0–B7 per UM1907) | From ST documentation |
| BSP panel driver | `rk043fn48h` (STM32CubeF7 BSP) | From ST BSP |

## Touch

| Item | Value | Status |
|---|---|---|
| Type | Capacitive | VERIFIED (physical board) |
| Controller | FT5336 | VERIFIED (physical board) |
| Bus | I²C | From ST BSP |
| Interrupt signal | `LCD_INT` | From ST documentation |
| BSP driver | `ft5336` component (STM32CubeF7 BSP) | From ST BSP |

## External Memory

- SDRAM: 128-Mbit device on FMC; only the lower 16 data bits are used,
  so 64 Mbit is accessible (UM1907).
  - Part MT48LC4M32B2B5-6A (`.ioc` pin labels), FMC SDRAM bank 1,
    `0xC0000000`–`0xC07FFFFF` (8 MB): 12 row bits, 8 column bits,
    4 internal banks, 16-bit bus.
  - CubeMX configures only the FMC controller; the JEDEC init sequence
    and refresh rate are in `App/Src/sdram.c` (CL3, SDCLK 100 MHz,
    refresh count 1542).
  - The LTDC must not read SDRAM while the init sequence runs (see the
    2026-10-13 LCD bring-up entry below).
- Quad-SPI NOR flash: 128 Mbit (UM1907).

## Debug

- On-board ST-LINK/V2-1 (UM1907).

## GPIO / peripheral mapping

Not yet documented in this repository.

Exact GPIO and peripheral assignments must be taken from the official
STM32F746G-DISCO sources listed below (UM1907 I/O assignment table,
MB1191 schematic, STM32CubeF7 BSP) and must not be guessed. The full
mapping is tracked as an M1 task in `docs/roadmap.md`.

## Bring-up checklist

- [x] Confirm exact board model printed on PCB (STM32F746G-DISCO, MB1191)
- [x] Confirm MCU marking (STM32F746NGH6)
- [x] Identify LCD panel (ROCKTECH RK043FN48H-CT672B)
- [x] Identify touch controller (FT5336)
- [x] Identify LCD resolution (480 × 272)
- [ ] Confirm touch I²C bus/address against BSP on hardware
- [x] Identify USB connector used for device communication — USB OTG FS
  micro-USB connector (not the ST-LINK one); VERIFIED 2026-10-13
- [x] Confirm ST-LINK connection — flash + debug via STM32Cube for VS Code;
  VERIFIED 2026-10-13
- [x] Confirm external SDRAM operation — 8 MB self-test PASS after a
  software reset; VERIFIED 2026-10-13 (power-cycle boot NOT TESTED)
- [ ] Confirm clock configuration
- [x] Confirm ST-LINK virtual COM port output on hardware — USART1,
  TX `PA9`, RX `PB7`, 115200 baud, `COM4` on the developer host;
  VERIFIED 2026-10-13
- [ ] Capture board photos in `docs/images/`

## Bring-up log

### 2026-10-13 — first boot (developer, physical board)

Firmware: branch `feature/1-firmware-skeleton` after ADR-001 (trimmed
peripherals + MPU fix). Built and flashed with the Windows ST toolchain
from STM32Cube for VS Code.

| Check | Result |
|---|---|
| Board flashes and halts at `main` | VERIFIED |
| `MX_FMC_Init()` no longer faults (MPU regions 1–2) | VERIFIED — reaches `osKernelStart()` |
| FreeRTOS scheduler runs the default task | VERIFIED — implied by USB enumeration, which happens in `StartDefaultTask` |
| USB OTG FS enumerates on Windows | VERIFIED — "HID-compliant mouse" (CubeMX default HID class; Custom HID is M3 work) |
| ST-LINK VCP (USART1) output | NOT TESTED — no logging code yet |
| LCD / touch | NOT TESTED — SDRAM init sequence and display code not written yet |

### 2026-10-13 — SDRAM and LCD bring-up (M1, issue #7)

Firmware: branch `hardware/7-lcd-bringup`. Built with the command-line
Windows ST toolchain (CMake 4.4.0+st.1), flashed with
`STM32_Programmer_CLI`, log read from `COM4`. Commands: `docs/toolchain.md`.

| Check | Result |
|---|---|
| VCP log on `COM4` (USART1, 115200 8N1) | VERIFIED — `BOOT OK sysclk=200000000` |
| SDRAM JEDEC init | VERIFIED — `SDRAM INIT OK`; FMC registers read over SWD: SDCR1 `0x19D4`, SDTR1 `0x01126361`, SDRTR `0x0C0C`, last SDCMR = LOAD MODE `0x230` |
| SDRAM self-test (data bus, address bus, full 8 MB, two patterns) | VERIFIED — `SELFTEST SDRAM PASS size=8388608 ms=1492`, 5 consecutive software resets |
| Test pattern in framebuffer | VERIFIED — framebuffer dumped over SWD, converted to PNG, pixel-exact match with the expected pattern; pixel sum `0x010493A2` matches the log |
| Pattern visible on the panel (colours, orientation, stability) | VERIFIED — developer visual check on the physical panel |
| Boot after a power cycle (SDRAM from cold) | NOT TESTED — all runs used a software reset via ST-LINK |
| Long-run SDRAM retention / temperature | NOT TESTED |

Failure found and fixed during bring-up: with the LTDC enabled (as
`MX_LTDC_Init()` leaves it), the self-test failed in the fill pass
(`SELFTEST SDRAM FAIL test=fill addr=0xC0000010 exp=0x00000004
got=0x00000000`). Single SWD writes to SDRAM smeared into neighbouring
columns, while the same writes to internal SRAM did not, i.e. the
SDRAM was still in its power-up burst mode. Cause: LTDC framebuffer
fetches were interleaved by the FMC with the init commands, and the
LOAD MODE REGISTER command was ignored by the device because a bank was
open. Fix: `App_Main` disables the LTDC until SDRAM is initialised and
the pattern is drawn. Disabling the LTDC only during the self-test did
not help; it must be off during the init sequence.

## Planned firmware peripherals

- LCD (LTDC)
- Touch controller (FT5336 over I²C)
- USB Device / HID
- GPIO
- DMA where appropriate

## Reference sources

- ST product page: 32F746GDISCOVERY —
  https://www.st.com/en/evaluation-tools/32f746gdiscovery.html
- UM1907 — Discovery kit for STM32F7 Series with STM32F746NG MCU
- MB1191 board schematic (available from the ST product page)
- STM32CubeF7 BSP for STM32746G-Discovery —
  https://github.com/STMicroelectronics/32f746gdiscovery-bsp

## Hardware validation policy

"VERIFIED (physical board)" above means the value was read from the
physical board and its original documentation. It does **not** mean the
peripheral has been brought up or tested in firmware.

A hardware-dependent feature is considered verified only after it has been
tested on the physical board.

AI agents must not infer hardware success from successful compilation.
