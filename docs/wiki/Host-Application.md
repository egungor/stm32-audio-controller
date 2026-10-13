# Host Application (`stm32-audio-host`)

A Windows console program (C++17). It finds the board over USB HID,
reads Windows' playback devices with Core Audio, and keeps both in step.

## Commands

| Command | What it does |
|---|---|
| `run` | Serve the board over USB HID; log every frame (`<-` from the board, `->` to it). Ctrl+C prints counters |
| `list` | Playback devices (`*` = default), volume, mute |
| `set-default <n>` | Make device `<n>` from `list` the default |
| `volume <0-100>` | Volume of the default device |
| `mute on\|off` | Mute of the default device |
| `watch` | Run the protocol controller without a board and print what it would send |

## How it works

- **Audio** (`WinAudioSystem`): active render endpoints, names and form
  factors through the documented MMDevice API. Switching the default uses
  the undocumented `IPolicyConfig` interface for the console and
  multimedia roles; see
  [ADR-002](https://github.com/egungor/stm32-audio-controller/blob/main/docs/decisions/ADR-002-default-device-switching.md) for
  the risk.
- **USB HID** (`WinHidLink`): once a second while not connected, look
  for VID:PID `1209:0001`, usage page `0xFF00`, 65-byte reports. A reader
  thread queues received frames; writes time out after 1 s.
- **Protocol** (`HostController`, `DeviceRegistry`):
  - gives each device a session-local index (0–254) that it keeps while
    the device stays present; at most 32 devices, names cut to 48 bytes
    at a UTF-8 boundary;
  - sends the full list when the board is opened or asks for it, then
    only what changed;
  - refuses a selection that arrives before this session's list was sent
    (its index may be from an earlier session).
- **Threads**: Core Audio callbacks and the HID reader only push events;
  the main thread runs the controller.

## Code layout

```text
host/
├── Audio/              IAudioSystem, DeviceRegistry, AudioMapping (portable)
├── App/                HostController, EventQueue, FrameText (portable)
├── Hid/                IFrameSink, HidMatch (portable)
├── Platform/Windows/   WinAudioSystem, WinHidLink, PolicyConfig.h, main.cpp
└── Tests/              test_host.cpp (Linux and Windows)
```

The portable part builds and is tested on Linux too. Details and manual
checks: [host/README.md](https://github.com/egungor/stm32-audio-controller/blob/main/host/README.md).
