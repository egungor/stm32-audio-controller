# STM32 Audio Controller

A touchscreen hardware controller for switching the default Windows audio output device.

The project consists of:

- **STM32F7 firmware** — touchscreen UI and USB HID communication
- **Windows host application** — enumerates Windows audio endpoints and changes the default playback device
- **Shared protocol** — the contract between STM32 and Windows
- **Tests and CI** — automated validation for host/protocol code

## Project status

M0–M6 are done: firmware, host and protocol are tested in CI, and the
board and Windows checks ran on 2026-10-14 (bring-up log in
[`docs/hardware.md`](docs/hardware.md)). M7 (polish) is in progress:
aliases, ordering, favorites, three layouts with themes and a settings
page, on protocol version 2 (ADR-004). Milestones:
[`docs/roadmap.md`](docs/roadmap.md).

| Part | State |
|---|---|
| Board bring-up: boot, ST-LINK VCP log, clock, SDRAM (also from a power cycle), LCD | VERIFIED on the board |
| FT5336 touch | VERIFIED on the board |
| LVGL UI (device buttons, pages, waiting screen, notices) | Simulator in CI; VERIFIED on the board |
| USB HID interface `1209:0001` and STM32 protocol client | Unit tests in CI; VERIFIED on the board and Windows |
| Protocol v1 (64-byte frames, CRC-16) | Codec tests in CI (Linux, Windows) |
| Windows host: Core Audio, default device switching, HID transport | Unit tests in CI; VERIFIED on Windows |
| Firmware ↔ host together | Loopback integration test in CI; end to end VERIFIED; adding or removing a Windows device while connected NOT TESTED |
| Volume and mute on the touchscreen (M6) | VERIFIED on the board and Windows |
| Display flip (180°) | VERIFIED on the board (#42, #51) |
| Polish (M7) | In progress (#57–#60) |

## Architecture

```text
┌───────────── STM32F746G-DISCO ─────────────┐
│ firmware/ui        LVGL screens            │
│      ▲ │                                   │
│      │ ▼                                   │
│ firmware/link      protocol client         │
│      ▲ │                                   │
│      │ ▼                                   │
│ App/usb_link.c     vendor HID 1209:0001    │
└──────────────────────┬─────────────────────┘
                       │ USB, 64-byte reports (protocol/protocol.md)
┌──────────────────────▼─────────────────────┐
│ WinHidLink         HID discovery and I/O   │
│      ▲ │                                   │
│      │ ▼                                   │
│ HostController     protocol, device list   │
│      ▲ │                                   │
│      │ ▼                                   │
│ WinAudioSystem     Core Audio              │
└─────────────── Windows host ───────────────┘
```

Details: [`docs/architecture.md`](docs/architecture.md).

## Features

Done in code (verification status above):

- device list from Windows on the touchscreen, four per page, with icons
  and Turkish text;
- select a device on the touchscreen → Windows default playback device;
- Windows changes (default, devices added or removed) shown on the board;
- USB unplug/replug and host program restart handled on both sides;
- volume slider and mute button on the touchscreen, following Windows.

Planned:

- friendly names, favourites, custom ordering, start with Windows, tray
  icon (M7).

## Repository layout

```text
stm32-audio-controller/
├── firmware/
│   ├── STM32AudioController/   # STM32CubeMX project (CMake)
│   │   └── App/                # hand-written firmware code
│   ├── ui/                     # LVGL UI, hardware independent
│   ├── link/                   # STM32 protocol client + tests
│   └── simulator/              # headless Linux build of the UI
├── host/
│   ├── Audio/ App/ Hid/        # platform-independent core
│   ├── Platform/Windows/       # Core Audio, HID, console program
│   └── Tests/
├── protocol/                   # protocol.md, C codec, tests
├── tests/integration/          # firmware ↔ host loopback test
├── docs/                       # architecture, hardware, roadmap, ADRs
├── tools/                      # build, simulator, font and log helpers
├── .github/                    # CI workflows, issue/PR templates
├── .claude/                    # Claude Code agents and commands
├── AGENTS.md
└── CLAUDE.md
```

## Build and test

| What | How |
|---|---|
| Firmware | [`docs/toolchain.md`](docs/toolchain.md) (Windows ST toolchain); `tools/build-firmware-linux.sh` on Linux |
| UI simulator | `tools/run-ui-simulator.sh` ([`firmware/simulator`](firmware/simulator/README.md)) |
| Host | [`host/README.md`](host/README.md) |
| Protocol codec | [`protocol/tests`](protocol/tests/README.md) |
| Integration test | [`tests/integration`](tests/integration/README.md) |

After cloning, run `git submodule update --init` (LVGL).

An overview for readers is in the
[wiki](https://github.com/egungor/stm32-audio-controller/wiki); its pages
are kept in [`docs/wiki/`](docs/wiki/README.md).

## Development model

This repository uses **Claude Code** as the AI development environment.

GitHub is used for:

- source control
- issues
- pull requests
- code review
- CI
- releases
- public documentation

The human developer remains responsible for architecture decisions, hardware validation, and final merges.

## Important rule

An AI agent must never claim hardware functionality was verified unless it was actually tested on the physical board.

See:

- [`CLAUDE.md`](CLAUDE.md)
- [`AGENTS.md`](AGENTS.md)
- [`docs/development-workflow.md`](docs/development-workflow.md)
- [`docs/architecture.md`](docs/architecture.md)
