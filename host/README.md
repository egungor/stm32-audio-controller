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

The platform-independent core exists and is tested; the Windows layers
(Core Audio, HID, the application's event loop) do not exist yet.

```text
host/
├── Audio/
│   ├── IAudioSystem.h      # interface to the Windows audio system (no Windows types)
│   └── DeviceRegistry.*    # endpoints -> device_index, DEVICE_LIST entries
├── Hid/
│   └── IFrameSink.h        # outgoing 64-byte protocol frames
├── App/
│   └── HostController.*    # protocol v1 exchanges (protocol/protocol.md)
├── Tests/
│   └── test_host.cpp       # fake audio system + fake sink, no framework
└── CMakeLists.txt
```

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

Windows (Visual Studio 2022 or newer) or Linux:

```text
cmake -S host -B build/host
cmake --build build/host --config Debug
ctest --test-dir build/host --build-config Debug --output-on-failure
```

CI runs this with MSVC on Windows and GCC on Linux
(`.github/workflows/host-ci.yml`).

## Still to do (REQUIRES WINDOWS)

- `IAudioSystem` implementation with Core Audio / MMDevice (enumeration,
  `IPolicyConfig` or equivalent for the default device, endpoint volume,
  notification client).
- HID transport (device discovery, read thread, `IFrameSink`).
- Application loop that serialises HID input and audio notifications into
  `HostController`.
