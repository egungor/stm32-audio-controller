# Pin, Peripheral and Clock Mapping

What the firmware in `firmware/STM32AudioController/` configures, read
from the generated code (`Core/Src/*.c`, `Core/Inc/main.h`) and the
`.ioc` file. Pin functions on the board follow UM1907 and the
STM32CubeF7 BSP; the `.ioc` pin labels (e.g. `LCD_R0 [RK043FN48H-CT672B_R0]`)
come from the CubeMX STM32F746G-DISCO board preset.

Status labels:

- `VERIFIED` — exercised by a hardware test recorded in `docs/hardware.md`.
- `FROM CODE` — read from the generated configuration only.
- `FROM ST BSP` — taken from ST documentation/BSP, not checked here.

## Pins used by the firmware

### LCD (LTDC, RGB565 framebuffer, 24-bit bus)

| Signal | Pin | | Signal | Pin |
|---|---|---|---|---|
| R0 | PI15 | | B0 | PE4 |
| R1–R7 | PJ0–PJ6 | | B1–B3 | PJ13–PJ15 |
| G0–G4 | PJ7–PJ11 | | B4 | PG12 (AF9) |
| G5–G7 | PK0–PK2 | | B5–B7 | PK4–PK6 |
| CLK | PI14 | | HSYNC | PI10 |
| VSYNC | PI9 | | DE | PK7 |

All LTDC pins use AF14 except PG12 (AF9). Status: `VERIFIED` — the
M1 test pattern was correct on the panel (colours, orientation).

### LCD control

| Signal | Pin | Configuration | Status |
|---|---|---|---|
| `LCD_DISP` (panel enable) | PI12 | Output, set high in `MX_GPIO_Init` | `VERIFIED` (panel shows the image) |
| `LCD_BL_CTRL` (backlight driver enable) | PK3 | Output, set high in `MX_GPIO_Init` | `VERIFIED` (backlight on) |

### Touch (FT5336)

| Signal | Pin | Configuration | Status |
|---|---|---|---|
| I²C3 SCL | PH7 | AF4, I²C3 | `FROM CODE` |
| I²C3 SDA | PH8 | AF4, I²C3 | `FROM CODE` |
| `LCD_INT` (touch interrupt) | PI13 | `GPIO_MODE_EVT_RISING` (event, no IRQ) | `FROM CODE` |

- I²C3: 7-bit addressing, kernel clock PCLK1 (50 MHz), `Timing =
  0x00C0EAFF`, which works out to about 100 kHz (standard mode).
- FT5336 address: `0x70` in the BSP's 8-bit form (`0x38` 7-bit) —
  `FROM ST BSP`, to be confirmed on hardware (open M1 item in
  `docs/hardware.md`).

### SDRAM (FMC bank 1, MT48LC4M32B2B5-6A, 16-bit bus)

| Signal | Pins |
|---|---|
| D0–D1 | PD14, PD15 |
| D2–D3 | PD0, PD1 |
| D4–D12 | PE7–PE15 |
| D13–D15 | PD8–PD10 |
| A0–A5 | PF0–PF5 |
| A6–A9 | PF12–PF15 |
| A10–A11 | PG0, PG1 |
| BA0, BA1 | PG4, PG5 |
| NBL0, NBL1 | PE0, PE1 |
| SDCLK | PG8 |
| SDNCAS / SDNRAS | PG15 / PF11 |
| SDNWE | PH5 |
| SDNE0 | PH3 |
| SDCKE0 | PC3 |

All FMC pins use AF12. Status: `VERIFIED` — full 8 MB self-test
(data bus, address bus, two patterns) passed.

### ST-LINK virtual COM port (USART1, 115200 8N1)

| Signal | Pin | Status |
|---|---|---|
| TX | PA9 (AF7) | `VERIFIED` (log on `COM4`) |
| RX | PB7 (AF7) | `FROM CODE` (firmware does not receive yet) |

### USB OTG FS (device, HID)

| Signal | Pin | Configuration | Status |
|---|---|---|---|
| DM / DP | PA11 / PA12 | AF10, OTG FS device (`usbd_conf.c`) | `VERIFIED` (enumerates on Windows) |
| ID | PA10 | AF10, OTG FS (set up in `usbd_conf.c`) | `FROM CODE` |
| `OTG_FS_VBUS` | PJ12 | Input | `FROM CODE` |
| `OTG_FS_PowerSwitchOn` | PD5 | Output, set high | `FROM CODE` |
| `OTG_FS_OverCurrent` | PD4 | Input | `FROM CODE` |

### Debug and oscillators

| Function | Pins | Status |
|---|---|---|
| SWD (SWDIO, SWCLK), SWO | PA13, PA14, PB3 | `VERIFIED` (flash and debug) |
| HSE, 25 MHz oscillator | PH0, PH1 | `FROM CODE` (`HSE_VALUE = 25000000`) |
| LSE, 32.768 kHz | PC14, PC15 | `FROM CODE` (no RTC in use) |

## Clocks

Computed from `SystemClock_Config` and the peripheral clock setup in
`Core/Src/main.c`. Only SYSCLK has been observed on hardware: the boot
log prints `sysclk=200000000`, which `HAL_RCC_GetSysClockFreq()` derives
from the RCC registers. It does not measure the real frequency. The
rest is `FROM CODE` until the "Verify clocks" M1 item is done.

| Clock | Source and divider | Value |
|---|---|---|
| PLL input | HSE 25 MHz / PLLM 25 | 1 MHz |
| PLL VCO | × PLLN 400 | 400 MHz |
| SYSCLK | VCO / PLLP 2 | **200 MHz** |
| HCLK (AHB) | SYSCLK / 1 | 200 MHz |
| PCLK1 (APB1) | HCLK / 4 | 50 MHz (timers 100 MHz) |
| PCLK2 (APB2) | HCLK / 2 | 100 MHz (timers 200 MHz) |
| PLL Q output | VCO / 9 | 44.4 MHz (not used: 48 MHz comes from PLLSAI) |
| PLLSAI VCO | 1 MHz × PLLSAIN 384 | 384 MHz |
| USB 48 MHz (CLK48) | PLLSAI VCO / PLLSAIP 8 | 48 MHz |
| LTDC pixel clock | PLLSAI VCO / PLLSAIR 5 / 8 | 9.6 MHz |
| FMC SDCLK | HCLK / 2 (`App/Src/sdram.c`) | 100 MHz |

- Voltage scale 1 with over-drive enabled, flash latency 6 wait states.
- The STM32F746 allows up to 216 MHz; the project runs at 200 MHz.
- LTDC timing: total 566 × 286 pixel clocks per frame, so
  9.6 MHz gives about 59.3 frames per second.

## Open items found while reading the code

1. **Pins of removed peripherals are still configured.** ADR-001 lists
   the remaining configured GPIO as `PD4`, `PD5`, `PI12`, `PI13`,
   `PJ12` and `PK3`, but `MX_GPIO_Init` still sets alternate functions
   on the pins of the peripherals ADR-001 removed: Ethernet RMII,
   USB OTG HS ULPI, SDMMC1, SAI2, DCMI, Quad-SPI, SPDIF-RX, SPI2, I2C1,
   USART6, the Arduino timer pins and the Arduino analog inputs. The
   peripherals themselves are not clocked or initialised. The `.ioc`
   still carries those pin signals, so CubeMX regenerates them. Fix
   (needs CubeMX, on the developer host): reset those pins in the
   `.ioc`, regenerate, and confirm the board still boots. Not analysed
   further here; no failure has been observed.
2. **Touch interrupt is an event, not an IRQ.** `LCD_INT` (PI13) is
   `GPIO_MODE_EVT_RISING`, and no EXTI interrupt is enabled. The touch
   driver will either poll the FT5336 over I²C or need PI13 changed to
   an EXTI interrupt with the edge the FT5336 actually drives. Decide in
   the touch bring-up task.
