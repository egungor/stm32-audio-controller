# Windows Host Application

The Windows host application will:

1. Detect the STM32 HID device.
2. Enumerate Windows audio render endpoints.
3. Maintain a host-side audio device model.
4. Change the default playback endpoint.
5. Monitor endpoint/default-device changes.
6. Communicate state to the STM32.

Target language: C++17.

## Current state

The platform-independent core and the Windows Core Audio layer exist.
USB HID does not (M3): the Windows program is a test console that prints
the frames it would send to the STM32.

```text
host/
├── Audio/
│   ├── IAudioSystem.h      # interface to the Windows audio system (no Windows types)
│   ├── AudioMapping.*      # Windows form factor -> icon, volume scalar <-> percent
│   └── DeviceRegistry.*    # endpoints -> device_index, DEVICE_LIST entries
├── Hid/
│   └── IFrameSink.h        # outgoing 64-byte protocol frames
├── App/
│   ├── HostController.*    # protocol v1 exchanges (protocol/protocol.md)
│   ├── EventQueue.h        # serialises Core Audio notifications into the controller
│   └── FrameText.*         # readable one-line form of a protocol message
├── Platform/Windows/       # Windows only
│   ├── WinAudioSystem.*    # IAudioSystem on Core Audio / MMDevice
│   ├── PolicyConfig.h      # undocumented IPolicyConfig (ADR-002)
│   ├── WinStrings.*        # UTF-16 <-> UTF-8
│   └── main.cpp            # stm32-audio-host console program
├── Tests/
│   └── test_host.cpp       # fake audio system + fake sink, no framework
└── CMakeLists.txt
```

## Windows audio layer

- Enumeration (active render endpoints, friendly name, form factor),
  default endpoint (`eConsole`), volume and mute of the default endpoint:
  documented MMDevice / `IAudioEndpointVolume` APIs.
- Switching the default: `IPolicyConfig::SetDefaultEndpoint` for the
  `eConsole` and `eMultimedia` roles. This interface is undocumented; see
  `docs/decisions/ADR-002-default-device-switching.md`.
- `IMMNotificationClient` and `IAudioEndpointVolumeCallback` only push
  events into `EventQueue`; repeated events of one kind collapse into
  one. The main thread pops them and calls `HostController`.

## Test console (`stm32-audio-host.exe`)

```text
stm32-audio-host list              playback devices (* = default), volume, mute
stm32-audio-host set-default <n>   make device <n> from "list" the default
stm32-audio-host volume <0-100>    volume of the default device
stm32-audio-host mute on|off       mute of the default device
stm32-audio-host watch             run HostController; print the frames it
                                   would send to the STM32 (Ctrl+C stops)
```

`watch` starts with a simulated `GET_DEVICE_LIST`, then prints
`DEVICE_LIST` / `DEVICE_CHANGED` / `VOLUME_CHANGED` / `MUTE_CHANGED` lines
as you change devices or volume in Windows.

Manual check on Windows (REQUIRES WINDOWS):

1. `list` shows the same devices and default as Settings → Sound.
2. `set-default <n>` switches the default; Settings → Sound follows.
3. `watch`, then change the default and the volume in Windows: one
   `DEVICE_CHANGED` / `VOLUME_CHANGED` line per change, no repeats.
4. Plug in or remove a device (e.g. headphones, HDMI): a new
   `DEVICE_LIST` with a new `gen=` and existing devices keeping their
   `index=`.

### Device registry

- Assigns each active render endpoint a session-local `device_index`
  (0–254). An endpoint keeps its index while it stays present. New
  endpoints get the next free index in round-robin order, so an index
  that was just released is not reused for another device at once.
- Keeps at most 32 devices (protocol limit) and always includes the
  default device.
- Truncates names to 48 bytes at a UTF-8 character boundary and drops
  NUL bytes.
- Changes `generation` only when the list content (order, indices, names,
  icons) changes.

### Host controller

- `GET_DEVICE_LIST` → the full list, then `DEVICE_CHANGED`,
  `VOLUME_CHANGED`, `MUTE_CHANGED`.
- `SELECT_DEVICE` → sets the Windows default and confirms with
  `DEVICE_CHANGED`; `ERROR UNKNOWN_DEVICE` for an index not in the list,
  `ERROR OPERATION_FAILED` if Windows refuses.
- `SET_VOLUME` / `SET_MUTE` → applied to the default device, confirmed
  with `VOLUME_CHANGED` / `MUTE_CHANGED`.
- Windows notifications (`onDevicesChanged`, `onDefaultChanged`,
  `onVolumeChanged`) send only what changed since the STM32 was last told,
  and nothing until the STM32 has requested the list after a connect.
- Malformed frames are answered with `ERROR`; an `ERROR` frame is never
  answered. Frames meant for the STM32 are dropped.
- Single-threaded by design: the Windows layer must queue Core Audio
  notifications (which arrive on COM threads) and call the controller from
  one thread.

## Build and test

Windows (Visual Studio 2022 or newer) or Linux. On Linux only the core
and its tests are built.

```text
cmake -S host -B build/host
cmake --build build/host --config Debug
ctest --test-dir build/host --build-config Debug --output-on-failure
```

CI runs this with MSVC on Windows and GCC on Linux
(`.github/workflows/host-ci.yml`).

The Windows program also cross-compiles with MinGW-w64; the sandbox used
that, and Wine, to check that it builds and starts (see the PR for what
Wine can and cannot show).

## Still to do

- HID transport (device discovery, read thread, `IFrameSink`) and
  replacing the print sink in `watch` with it (M3).
- Running without a console (tray icon, start with Windows: M7).
