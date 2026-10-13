# Firmware Log Format

The firmware writes a machine-readable log to the ST-LINK virtual COM
port. It is a **debug/bring-up interface**, not part of the STM32 ↔ Windows
protocol (that runs over USB HID, see `protocol/`). Scripts and agents
parse it, so changes to existing lines must update this file.

## Transport

| Item | Value |
|---|---|
| UART | USART1, TX `PA9`, RX `PB7` (ST-LINK VCP, see ADR-001) |
| Settings | 115200 baud, 8N1, no flow control |
| Windows port | `COM4` on the developer host (see `docs/toolchain.md`) |
| Reader | `tools/read-vcp.ps1` |

## Line format

```text
<SUBJECT> [<ITEM>] <RESULT> [key=value ...]\r\n
```

- One event per line, ASCII, CR LF terminated, at most 125 characters
  (longer lines are truncated).
- `RESULT` is `OK`, `PASS`, `FAIL`, `DRAWN`, `READY`. A line containing
  the word `FAIL` reports an error.
- Numbers in `key=value` pairs are decimal unless prefixed with `0x`.
- Lines are emitted only from task context (blocking transmit).

## Lines

Emitted in this order at boot, in every build:

| Line | Meaning |
|---|---|
| `BOOT OK sysclk=<Hz>` | Default task started; SYSCLK frequency |
| `CLOCK CONFIG OK sysclk=<Hz> hclk=<Hz> pclk1=<Hz> pclk2=<Hz>` | Clock tree as computed by the HAL from the RCC registers (not a measurement) |
| `CLOCK MEASURE PASS ref=LSE tim=<Hz> exp=<Hz> ppm=<n>` | TIM5 clock measured against the 32.768 kHz LSE crystal over 8192 periods; within ±500 ppm of `exp` |
| `CLOCK MEASURE FAIL reason=<r> ref=LSE tim=<Hz> exp=<Hz> ppm=<n>` | `r` = `lse_start` (LSE did not start), `capture_timeout`, `overcapture` (a capture was missed) or `out_of_range` (error above ±500 ppm); `tim=0` when nothing was measured |
| `SDRAM INIT OK` / `SDRAM INIT FAIL` | JEDEC init sequence accepted by the FMC |
| `SELFTEST SDRAM PASS size=<bytes> ms=<ms>` | Data bus, address bus and full 8 MB fill tests passed |
| `SELFTEST SDRAM FAIL test=<data\|addr\|fill> addr=0x<a> exp=0x<e> got=0x<g>` | First failing word |
| `TOUCH INIT OK id=0x51` / `TOUCH INIT FAIL id=0x<id>` | FT5336 chip ID read over I2C3 (`id=0x00` if the read failed); polling mode selected |

A `CLOCK MEASURE FAIL` is reported only; the boot continues. After an
SDRAM failure the LTDC stays disabled (no image); `APP READY` is still
printed.

### UI build (default)

| Line | Meaning |
|---|---|
| `UI INIT OK fb0=0x<addr> fb1=0x<addr>` | LVGL running on the two SDRAM framebuffers; LTDC enabled |
| `UI INIT FAIL reason=<sdram\|lcd_format>` | No UI: SDRAM failed, or LTDC layer 0 is not RGB565 480×272 |
| `UI FLIP on\|off copy_max_ms=<n>` | User held the flip button (#42): the image turns by 180° (`on`) or back (`off`). `n` is the longest 180° frame copy while flipped, so it is 0 on `on` and meaningful on `off` |
| `APP READY` | Main loop running |

Then, as the USB host link works (`App/Src/link_port.c`, `protocol.md`):

| Line | Meaning |
|---|---|
| `USB CONFIGURED` | The PC configured the HID interface; `GET_DEVICE_LIST` is sent (and repeated every 1 s) |
| `USB DISCONNECTED` | Unplugged, reset or suspended; waiting screen |
| `LINK LIST OK n=<n> gen=<g>` | A complete device list arrived and is shown |
| `LINK ACTIVE id=<id>` | The host reported a new default device (`DEVICE_CHANGED`) |
| `UI SELECT id=<id>` | User tapped a device that is not active; `SELECT_DEVICE` sent |
| `LINK SELECT FAIL reason=host_error` | The host answered the selection with `ERROR` |
| `LINK SELECT FAIL reason=timeout` | No answer within 2 s; waiting screen, list requested again |
| `UI VOLUME value=<n>` | User released the volume slider at `n` percent (`SET_VOLUME` is sent while dragging, at most every 100 ms) |
| `UI MUTE on\|off` | User tapped the mute button; `SET_MUTE` sent |
| `LINK MUTE muted=<0\|1>` | The host reported the mute state (`MUTE_CHANGED`) |

With `-DAPP_UI_DEMO=ON` demo data replaces the host link (no USB lines):

| Line | Meaning |
|---|---|
| `UI DEMO START` | UI built with demo data |
| `UI DEMO DEVICES n=<n> active=<id>` | About 1.5 s later: demo device list shown |
| `UI SELECT id=<id>` | User tapped a device that is not active |
| `UI DEMO ACTIVE id=<id>` | 0.4 s later the demo confirms the selection |
| `UI VOLUME value=<n>` / `UI MUTE on\|off` | Volume slider released / mute tapped; the demo accepts at once |

### Test screen build (`-DAPP_TEST_SCREEN=ON`)

The M1 bring-up screen instead of the UI.

| Line | Meaning |
|---|---|
| `LCD PATTERN DRAWN w=<px> h=<px> fmt=RGB565 fb=0x<addr> sum=0x<sum>` | Test pattern written; `sum` = 32-bit sum of all 16-bit pixels, compare with `tools/fb2png.py` |
| `LCD PATTERN FAIL reason=pixel_format` | Layer 0 is not RGB565 |
| `TOUCH TARGET n=<0..4> x=<px> y=<px>` | Centre of each touch target drawn on the pattern (corners, then centre) |
| `APP READY` | Main loop running |

While a finger is down, a small black-and-white dot is drawn under it on
the test pattern. To check the coordinates, touch each target and compare
`TOUCH DOWN` with the matching `TOUCH TARGET` line.

### Touch (both builds)

| Line | Meaning |
|---|---|
| `TOUCH DOWN x=<px> y=<px>` | First touch point pressed, LCD pixel coordinates (0,0 = top-left) |
| `TOUCH UP x=<px> y=<px>` | Released; last position seen while pressed |
| `TOUCH READ FAIL` | I2C read failed; printed once until a read succeeds again |

The test screen polls the FT5336 every 20 ms; the UI build polls it each
time LVGL reads the input device (every 33 ms).

`tim` is the APB1 timer clock (2 × PCLK1, 100 MHz with the current
configuration), which comes from HSE through the main PLL, so a `PASS`
confirms the HSE frequency and the PLL/bus dividers together. The LSE is
left running after the check.

Example (hardware, 2026-10-13, M1 firmware before the clock and touch
checks existed):

```text
BOOT OK sysclk=200000000
SDRAM INIT OK
SELFTEST SDRAM PASS size=8388608 ms=1492
LCD PATTERN DRAWN w=480 h=272 fmt=RGB565 fb=0xC0000000 sum=0x010493A2
APP READY
```
