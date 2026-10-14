# STM32 Audio Controller

A touchscreen controller on an **STM32F746G-DISCO** board that switches the
default Windows playback device. Tap a device on the 4.3" LCD; Windows
switches to it. Change the device in Windows; the LCD follows.

```text
 STM32F746G-DISCO (touchscreen)  ──USB HID──►  Windows PC (stm32-audio-host)
   LVGL UI, protocol client      ◄──────────   Core Audio, default device
```

## Where the project stands

| Part | State |
|---|---|
| Board bring-up: boot, log, clock, SDRAM, LCD | VERIFIED on the board |
| Touch, UI on the LCD, USB HID, display flip | VERIFIED on the board |
| Windows host: audio, default switching, HID | VERIFIED on Windows |
| Firmware ↔ host together | Simulated in CI; end to end VERIFIED |
| Volume/mute on the touchscreen (M6) | VERIFIED on the board and Windows |
| Polish (M7) | In progress: aliases, ordering, favorites, layouts and themes, settings page (protocol v2) |

The board and Windows checks ran on 2026-10-14; see
[Hardware Test Checklist](Hardware-Test-Checklist) and the bring-up log
in `docs/hardware.md`.

## Pages

- [Getting Started](Getting-Started) — clone, build, flash, run
- [Architecture](Architecture) — the three parts and how they talk
- [Hardware](Hardware) — board, peripherals, pins
- [Firmware](Firmware) — STM32 code, UI, build options, log lines
- [Host Application](Host-Application) — `stm32-audio-host` and its commands
- [Protocol](Protocol) — the 64-byte frame and the message exchanges
- [Testing](Testing) — CI, local tests, the loopback integration test
- [Hardware Test Checklist](Hardware-Test-Checklist) — what to try on the board and on Windows
- [Development Workflow](Development-Workflow) — branches, commits, PRs, Claude Code
- [Troubleshooting](Troubleshooting)

## Source of truth

This wiki is an overview. The repository documents are authoritative and
are updated with the code:
[README](https://github.com/egungor/stm32-audio-controller/blob/main/README.md) ·
[roadmap](https://github.com/egungor/stm32-audio-controller/blob/main/docs/roadmap.md) ·
[architecture](https://github.com/egungor/stm32-audio-controller/blob/main/docs/architecture.md) ·
[hardware](https://github.com/egungor/stm32-audio-controller/blob/main/docs/hardware.md) ·
[protocol](https://github.com/egungor/stm32-audio-controller/blob/main/protocol/protocol.md) ·
[toolchain](https://github.com/egungor/stm32-audio-controller/blob/main/docs/toolchain.md).
