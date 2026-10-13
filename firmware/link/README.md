# Protocol client (`firmware/link`)

STM32 side of protocol v1 (`protocol/protocol.md`). Hardware independent
C: frames go out through a callback and come in through `Link_OnFrame`,
the UI is driven through callbacks, time is passed in. The firmware glue
is `App/Src/link_port.c` (USB HID in `App/Src/usb_link.c`).

What it does:

- after USB is configured, sends `GET_DEVICE_LIST` and repeats it every
  1 s until a complete list has arrived;
- assembles `DEVICE_LIST` frames by `generation` and `position` and shows
  the list only when complete;
- applies `DEVICE_CHANGED` (the default flag in the list until then);
- sends `SELECT_DEVICE` and marks the device as pending; without an answer
  in 2 s the host counts as not responding (notice, waiting screen,
  request the list again);
- shows a notice when the host answers the selection with `ERROR`, and
  re-requests the list after `UNKNOWN_DEVICE`;
- answers invalid frames with `ERROR`, never an `ERROR` frame.

Tests (Linux, any C compiler):

```text
cmake -S firmware/link -B build/link
cmake --build build/link
ctest --test-dir build/link --output-on-failure
```
