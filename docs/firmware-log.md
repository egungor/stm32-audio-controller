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
| `SDRAM INIT OK` / `SDRAM INIT FAIL` | JEDEC init sequence accepted by the FMC |
| `SELFTEST SDRAM PASS size=<bytes> ms=<ms>` | Data bus, address bus and full 8 MB fill tests passed |
| `SELFTEST SDRAM FAIL test=<data\|addr\|fill> addr=0x<a> exp=0x<e> got=0x<g>` | First failing word |
| `LCD PATTERN DRAWN w=<px> h=<px> fmt=RGB565 fb=0x<addr> sum=0x<sum>` | Test pattern written; `sum` = 32-bit sum of all 16-bit pixels, compare with `tools/fb2png.py` |
| `LCD PATTERN FAIL reason=pixel_format` | Layer 0 is not RGB565 |
| `APP READY` | Bring-up finished; main loop running |

After a `FAIL` the remaining bring-up steps are skipped (`APP READY` is
still printed) and the LTDC stays disabled, so the panel shows no image.

Example (hardware, 2026-10-13):

```text
BOOT OK sysclk=200000000
SDRAM INIT OK
SELFTEST SDRAM PASS size=8388608 ms=1492
LCD PATTERN DRAWN w=480 h=272 fmt=RGB565 fb=0xC0000000 sum=0x010493A2
APP READY
```
