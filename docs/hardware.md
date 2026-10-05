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
- [ ] Identify USB connector used for device communication
- [ ] Confirm ST-LINK connection
- [ ] Confirm external SDRAM operation
- [ ] Confirm clock configuration
- [ ] Capture board photos in `docs/images/`

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
