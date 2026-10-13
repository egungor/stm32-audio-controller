# Firmware ↔ host loopback test

`test_integration.cpp` runs the STM32 protocol client
(`firmware/link/link_client.c`) and the host's `HostController` against
each other. Both are the real code; the rest is modelled:

| Part | Model |
|---|---|
| USB | one 64-byte report per direction every 4 ms; reports sent while the host has no handle open are lost |
| STM32 receive queue | `USB_LINK_RX_DEPTH` frames, overflow drops the frame (as `usb_link.c`) |
| STM32 UI loop | every 10 ms, at most 8 frames, then `Link_Poll` (as `link_port.c`); can be made busy |
| LCD | records what the link client shows (list, active, pending, notices) |
| Windows | fake `IAudioSystem`; notifications go straight to the controller |
| Host HID | the device is opened 700 ms after plug-in (`WinHidLink` scans every second) |

Time is simulated in 1 ms steps, starting just before the 32-bit tick
wraps.

Scenarios: connect, host started late, select on the board, Windows
default change, devices added and removed, default device removed, all
devices removed, Windows refuses the switch, a select racing a device
removal, USB unplug/replug (also during a list), host stops answering,
host restarted with the board still showing the old list, 40 devices with
long UTF-8 names, volume frames, and the board busy while long lists
arrive.

The constants that mirror the firmware are at the top of the test; keep
them in step with `usb_link.h` and `link_port.c`.

What this cannot show: real USB timing and enumeration, Windows' HID
driver, Core Audio, the LCD. See "Manual check with the board" in
`host/README.md` (REQUIRES HARDWARE + WINDOWS).

```text
cmake -S tests/integration -B build/integration
cmake --build build/integration --config Debug
ctest --test-dir build/integration --build-config Debug --output-on-failure
```

CI: `.github/workflows/integration-ci.yml` (Linux GCC, Windows MSVC).
