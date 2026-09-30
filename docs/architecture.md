# System Architecture

## Goal

Provide a physical touchscreen controller for selecting the default Windows
audio playback endpoint.

## High-level architecture

```text
                         USB HID
┌──────────────────────┐            ┌────────────────────────┐
│      STM32F7         │            │    Windows Host App    │
│                      │            │                        │
│  Touch UI            │            │  HID Transport         │
│      │               │            │       │                │
│      ▼               │            │       ▼                │
│  UI/Application      │◄──────────►│  Audio Device Manager │
│      │               │            │       │                │
│      ▼               │            │       ▼                │
│  Protocol            │            │ Windows Core Audio     │
└──────────────────────┘            └────────────────────────┘
```

## Component boundaries

### Firmware

Responsible for:

- LCD/touch initialization
- rendering the UI
- handling touch events
- maintaining a display model
- sending user actions to the host
- receiving device information/state from the host

Not responsible for:

- enumerating Windows audio devices
- knowing Windows device IDs
- changing Windows audio policy directly

### Windows host

Responsible for:

- detecting the STM32 HID device
- enumerating active Windows render endpoints
- identifying the current default endpoint
- changing the default endpoint
- monitoring endpoint/default-device changes
- sending a device model to the STM32

### Protocol

Responsible for:

- framing
- message types
- payload formats
- protocol version
- request/response semantics
- error reporting

## UI concept

The initial UI uses large square buttons.

Target layout:

```text
┌──────────────────────────────────────────┐
│              AUDIO OUTPUT                │
│                                          │
│  ┌──────────────┐  ┌──────────────┐     │
│  │      🎧      │  │      🔊      │     │
│  │              │  │              │     │
│  │  Device A    │  │  Device B    │     │
│  │              │  │              │     │
│  │      ✓       │  │              │     │
│  └──────────────┘  └──────────────┘     │
│                                          │
│  ┌──────────────┐  ┌──────────────┐     │
│  │      🖥      │  │      🎧      │     │
│  │              │  │              │     │
│  │  Device C    │  │  Device D    │     │
│  └──────────────┘  └──────────────┘     │
│                                          │
│              ◀   1 / N   ▶              │
└──────────────────────────────────────────┘
```

The exact dimensions/layout depend on the verified board LCD.

## Important design decisions

- STM32 ↔ Windows: USB HID
- Host language: C++17
- GUI: LVGL, pending hardware bring-up validation
- Audio control: Windows Core Audio/MMDevice
- Device information is discovered dynamically by Windows
- User-friendly display names are separated from Windows endpoint IDs

## Future extensions

- Volume
- Mute
- Favorites
- Custom ordering/names
- Startup integration
- Device groups
