# STM32 Audio Controller

A touchscreen hardware controller for switching the default Windows audio output device.

The project consists of:

- **STM32F7 firmware** — touchscreen UI and USB HID communication
- **Windows host application** — enumerates Windows audio endpoints and changes the default playback device
- **Shared protocol** — the contract between STM32 and Windows
- **Tests and CI** — automated validation for host/protocol code

## Project status

**Milestone M0 — AI Development Environment**

Repository structure and development rules are established. Hardware bring-up has not started yet.

## Target architecture

```text
┌───────────────────────────────┐
│          STM32F7              │
│                               │
│  Touchscreen UI               │
│       │                       │
│       ▼                       │
│  Audio Device Model           │
│       │                       │
│       ▼                       │
│  USB HID                      │
└──────────────┬────────────────┘
               │ USB
               ▼
┌───────────────────────────────┐
│       Windows Host App        │
│                               │
│  HID Transport                │
│       │                       │
│       ▼                       │
│  Audio Device Manager         │
│       │                       │
│       ▼                       │
│  Windows Core Audio           │
└───────────────────────────────┘
```

## Planned features

### v0.1
- STM32 LCD initialization
- Touchscreen initialization
- Basic audio-output button UI
- USB HID communication
- Windows audio-device enumeration

### v0.2
- Select an audio device from STM32
- Change Windows default playback endpoint
- Synchronize Windows changes back to STM32

### v0.3
- Volume control
- Mute
- Pagination
- Device icons and improved UI

### v1.0
- Friendly device names
- Favorites/custom ordering
- Startup with Windows
- Robust error handling
- Hardware/host integration tests

## Repository layout

```text
stm32-audio-controller/
├── .claude/
│   ├── agents/
│   └── commands/
├── .github/
│   ├── ISSUE_TEMPLATE/
│   ├── workflows/
│   ├── pull_request_template.md
│   └── dependabot.yml
├── docs/
│   ├── decisions/
│   ├── images/
│   ├── architecture.md
│   ├── hardware.md
│   ├── roadmap.md
│   └── development-workflow.md
├── firmware/
│   ├── App/
│   ├── Core/
│   ├── Drivers/
│   ├── Middlewares/
│   └── README.md
├── host/
│   ├── Audio/
│   ├── Hid/
│   ├── App/
│   ├── Tests/
│   └── README.md
├── protocol/
│   ├── include/
│   ├── src/
│   ├── tests/
│   └── protocol.md
├── tests/
├── tools/
├── AGENTS.md
├── CLAUDE.md
└── README.md
```

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
