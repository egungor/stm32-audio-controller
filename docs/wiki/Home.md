# STM32 Audio Controller

A touchscreen controller on an **STM32F746G-DISCO** board that switches the
default Windows playback device. Tap a device on the 4.3" LCD; Windows
switches to it. Change the device in Windows; the LCD follows.

![Main screen on the LCD](https://raw.githubusercontent.com/egungor/stm32-audio-controller/main/docs/images/ui-main-screen.png)

```text
 STM32F746G-DISCO (touchscreen)  ──USB HID──►  Windows PC (stm32-audio-tray)
   LVGL UI, protocol client      ◄──────────   Core Audio, default device
```

**Download:** the Windows installer and the firmware (`.elf`, `.hex`,
`.bin`) are on the [Releases page](https://github.com/egungor/stm32-audio-controller/releases); latest:
[v0.7.0-beta.2](https://github.com/egungor/stm32-audio-controller/releases/tag/v0.7.0-beta.2) (pre-release). How to install and
flash: [Getting Started](Getting-Started#quick-start-from-a-release).

## Where the project stands

| Part | State |
|---|---|
| Board bring-up: boot, log, clock, SDRAM, LCD | VERIFIED on the board |
| Touch, UI on the LCD, USB HID, display flip | VERIFIED on the board |
| Windows host: audio, default switching, HID | VERIFIED on Windows |
| Firmware ↔ host together | Simulated in CI; end to end VERIFIED |
| Volume/mute on the touchscreen (M6) | VERIFIED on the board and Windows |
| Polish (M7): layouts, themes, settings, edit mode, aliases | Coded; layout A, themes, accent colours, favorites tab, mute/volume VERIFIED on the board; the rest REQUIRES HARDWARE |
| Volume limit on switch | VERIFIED on the board and Windows (2026-10-16) |
| Tray program, start with Windows, installer | Built in CI; installed and used on Windows; full check REQUIRES WINDOWS |

The M1–M6 board and Windows checks ran on 2026-10-14, the M7 checks are
in progress; see
[Hardware Test Checklist](Hardware-Test-Checklist) and the bring-up log
in `docs/hardware.md`.

## Pages

- [User Guide](User-Guide) — screens, settings, favorites and aliases, the tray program
- [Getting Started](Getting-Started) — install from a release, or clone, build, flash, run
- [Architecture](Architecture) — the three parts and how they talk
- [Hardware](Hardware) — board, peripherals, pins
- [Firmware](Firmware) — STM32 code, UI, build options, log lines
- [Host Application](Host-Application) — tray program, `stm32-audio-host` and its commands
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
