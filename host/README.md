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

The platform-independent core, the Windows Core Audio layer and the USB
HID transport exist. The Windows program is a console application:
`run` serves the controller over USB HID; the other commands test the
audio layer on their own.

```text
host/
├── Audio/
│   ├── IAudioSystem.h      # interface to the Windows audio system (no Windows types)
│   ├── AudioMapping.*      # Windows form factor -> icon, volume scalar <-> percent
│   └── DeviceRegistry.*    # endpoints -> device_index, DEVICE_LIST entries
├── Hid/
│   ├── IFrameSink.h        # outgoing 64-byte protocol frames
│   └── HidMatch.h          # which HID interface is the controller (ADR-003)
├── App/
│   ├── HostController.*    # protocol v1 exchanges (protocol/protocol.md)
│   ├── EventQueue.h        # serialises Core Audio and HID events into the controller
│   └── FrameText.*         # readable one-line form of a protocol message
├── Platform/Windows/       # Windows only
│   ├── WinAudioSystem.*    # IAudioSystem on Core Audio / MMDevice
│   ├── WinHidLink.*        # HID discovery, reader thread, IFrameSink
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
  events into `EventQueue`; repeated audio events of one kind collapse
  into one. The main thread pops them and calls `HostController`.

## HID transport

Details of the USB interface: `docs/decisions/ADR-003-usb-hid-interface.md`.

- Discovery: every second, while not connected, the HID interfaces
  present (`SetupDiGetClassDevs`) are checked with `HidD_GetAttributes`
  and `HidP_GetCaps`. The controller is VID:PID `1209:0001`, usage page
  `0xFF00`, usage `1`, 65-byte input and output reports (report ID 0 +
  one 64-byte frame). The rule is `isController()` in `Hid/HidMatch.h`
  and is unit tested.
- Reading: a worker thread reads input reports (overlapped I/O) and
  queues each frame (`EventQueue::pushFrame`). Frames are not validated
  there; `HostController` decodes them. At most 64 received frames wait
  in the queue; more are dropped and counted.
- Writing: `sendFrame()` writes one output report from the consumer
  thread and gives up after 1 s if the STM32 does not take it.
- Connect / disconnect are queued as `HidConnected` / `HidDisconnected`.
  On connect the controller sends the full list, default, volume and
  mute at once (`onConnected()`), so a board that stayed plugged in while
  the host restarted drops the previous session's list. On disconnect it
  forgets what was sent (`onDisconnected()`) and sends nothing.
- Only the consumer thread touches `HostController` and the audio
  system; the HID thread and the Core Audio threads only push events.

## Console program (`stm32-audio-host.exe`)

```text
stm32-audio-host run               serve the controller over USB HID; log every
                                   frame in both directions (Ctrl+C stops)
stm32-audio-host list              playback devices (* = default), volume, mute
stm32-audio-host set-default <n>   make device <n> from "list" the default
stm32-audio-host volume <0-100>    volume of the default device
stm32-audio-host mute on|off       mute of the default device
stm32-audio-host watch             run HostController; print the frames it
                                   would send to the STM32 (Ctrl+C stops)
```

`run` prints `Controller connected: <product> <device path>` and
`Controller disconnected.`, then one line per frame: `<-` received from the
STM32, `->` sent to it (`[send failed]` if the write failed). Ctrl+C prints
counters (sent, send failures, rejected, ignored, errors received,
dropped).

`watch` needs no board: it starts as if the board had just been opened
(the full list), then prints
`DEVICE_LIST` / `DEVICE_CHANGED` / `VOLUME_CHANGED` / `MUTE_CHANGED` lines
as you change devices or volume in Windows.

Manual check of the audio layer on Windows (REQUIRES WINDOWS):

1. `list` shows the same devices and default as Settings → Sound.
2. `set-default <n>` switches the default; Settings → Sound follows.
3. `watch`, then change the default and the volume in Windows: one
   `DEVICE_CHANGED` / `VOLUME_CHANGED` line per change, no repeats.
4. Plug in or remove a device (e.g. headphones, HDMI): a new
   `DEVICE_LIST` with a new `gen=` and existing devices keeping their
   `index=`.

Manual check with the board (REQUIRES HARDWARE + WINDOWS; firmware from
#31, built without `APP_UI_DEMO`):

1. Start `run` before plugging in the board: `Controller connected`,
   then `-> DEVICE_LIST …` for every device, `-> DEVICE_CHANGED`,
   `-> VOLUME_CHANGED`, `-> MUTE_CHANGED` (a `<- GET_DEVICE_LIST` and a
   second list may follow if the board asked at the same moment). The
   LCD shows the same devices with the default marked.
2. Touch another device on the LCD: `<- SELECT_DEVICE index=…`,
   `-> DEVICE_CHANGED index=…`; Settings → Sound follows.
3. Change the default in Windows: one `-> DEVICE_CHANGED`; the LCD
   follows.
4. Unplug the board: `Controller disconnected.`; plug it back in: it
   reconnects within about 2 s and the list is sent again.
5. Start `run` with the board already plugged in: same as 1.
6. Ctrl+C: `send_failures=0 rejected=0 dropped=0` in a normal session.
7. Restart `run` with the board plugged in, after changing the default
   in Windows while it was closed: the LCD shows the current list and
   default right after `Controller connected`, without a tap.
8. Drag the volume slider on the LCD: a few `<- SET_VOLUME` lines (not
   one per pixel), the Windows volume follows and the knob does not jump
   back. Tap mute: `<- SET_MUTE`, `-> MUTE_CHANGED`, the icon changes.
   Change volume and mute in Windows: the LCD follows.

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

- Device opened (`onConnected`) or `GET_DEVICE_LIST` → the full list,
  then `DEVICE_CHANGED`, `VOLUME_CHANGED`, `MUTE_CHANGED`.
- `SELECT_DEVICE` → sets the Windows default and confirms with
  `DEVICE_CHANGED`; `ERROR UNKNOWN_DEVICE` for an index not in the list
  or before the list has been sent since the last connect (its index may
  be from an earlier session), `ERROR OPERATION_FAILED` if Windows
  refuses.
- `SET_VOLUME` / `SET_MUTE` → applied to the default device, confirmed
  with `VOLUME_CHANGED` / `MUTE_CHANGED`.
- Windows notifications (`onDevicesChanged`, `onDefaultChanged`,
  `onVolumeChanged`) send only what changed since the STM32 was last told,
  and nothing while no list has been sent since the last connect.
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
(`.github/workflows/host-ci.yml`). The controller is also run against
the firmware's protocol client in `tests/integration`.

The Windows program also cross-compiles with MinGW-w64; the sandbox used
that, and Wine, to check that it builds and starts (see the PRs for what
Wine can and cannot show). Link libraries: `ole32`, `setupapi`, `hid`.

## Still to do

- Hardware + Windows test of `run` with the board (list above).
- Running without a console (tray icon, start with Windows: M7).
