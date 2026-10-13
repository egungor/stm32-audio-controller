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

## Lines (M1)

Emitted in this order at boot:

| Line | Meaning |
|---|---|
| `BOOT OK sysclk=<Hz>` | Default task started; SYSCLK frequency |
| `CLOCK CONFIG OK sysclk=<Hz> hclk=<Hz> pclk1=<Hz> pclk2=<Hz>` | Clock tree as computed by the HAL from the RCC registers (not a measurement) |
| `CLOCK MEASURE PASS ref=LSE tim=<Hz> exp=<Hz> ppm=<n>` | TIM5 clock measured against the 32.768 kHz LSE crystal over 8192 periods; within ±500 ppm of `exp` |
| `CLOCK MEASURE FAIL reason=<r> ref=LSE tim=<Hz> exp=<Hz> ppm=<n>` | `r` = `lse_start` (LSE did not start), `capture_timeout`, `overcapture` (a capture was missed) or `out_of_range` (error above ±500 ppm); `tim=0` when nothing was measured |
| `SDRAM INIT OK` / `SDRAM INIT FAIL` | JEDEC init sequence accepted by the FMC |
| `SELFTEST SDRAM PASS size=<bytes> ms=<ms>` | Data bus, address bus and full 8 MB fill tests passed |
| `SELFTEST SDRAM FAIL test=<data\|addr\|fill> addr=0x<a> exp=0x<e> got=0x<g>` | First failing word |
| `LCD PATTERN DRAWN w=<px> h=<px> fmt=RGB565 fb=0x<addr> sum=0x<sum>` | Test pattern written; `sum` = 32-bit sum of all 16-bit pixels, compare with `tools/fb2png.py` |
| `LCD PATTERN FAIL reason=pixel_format` | Layer 0 is not RGB565 |
| `APP READY` | Bring-up finished; main loop running |

A `CLOCK MEASURE FAIL` is reported only; the bring-up continues. After
any other `FAIL` the remaining bring-up steps are skipped (`APP READY` is
still printed) and the LTDC stays disabled, so the panel shows no image.

`tim` is the APB1 timer clock (2 × PCLK1, 100 MHz with the current
configuration), which comes from HSE through the main PLL, so a `PASS`
confirms the HSE frequency and the PLL/bus dividers together. The LSE is
left running after the check.

Example (hardware, 2026-10-13):

```text
BOOT OK sysclk=200000000
SDRAM INIT OK
SELFTEST SDRAM PASS size=8388608 ms=1492
LCD PATTERN DRAWN w=480 h=272 fmt=RGB565 fb=0xC0000000 sum=0x010493A2
APP READY
```
