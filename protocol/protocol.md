# STM32 ↔ Windows Protocol

## Status

**Protocol version 1.** Frame format, message set and validation rules are
fixed. Firmware and host integration follow in M3/M4.

This document is the source of truth for communication between the STM32
firmware and the Windows host application. The reference implementation
is `include/protocol.h` + `src/protocol.c` (portable C99, used by both
sides); `tests/test_protocol.c` checks it against this document.

## Transport

USB HID, Full Speed (design: `docs/decisions/ADR-003-usb-hid-interface.md`).

| Item | Value |
|---|---|
| VID / PID | `0x1209` / `0x0001` (pid.codes test range, development only) |
| Interface | HID class, no boot subclass; vendor usage page `0xFF00`, usage `0x01` |
| Device → host | 64-byte input report on interrupt IN endpoint `0x81`, 5 ms interval |
| Host → device | 64-byte output report on interrupt OUT endpoint `0x01`, 5 ms interval |
| Report ID | none — the report is exactly one frame |

On Windows, `ReadFile` / `WriteFile` on the HID handle use 65-byte
buffers: byte 0 is the report ID (always 0), bytes 1–64 are the frame.

Timing, applied by the STM32:

| Rule | Value |
|---|---|
| `GET_DEVICE_LIST` repeated until a complete list has arrived | every 1000 ms |
| No `DEVICE_CHANGED` or `ERROR` after `SELECT_DEVICE` | 2000 ms → host not responding: notice, waiting screen, request the list again |
| A new list stays incomplete (a frame was lost) while an older list is shown | 1000 ms after its last entry → `GET_DEVICE_LIST` again; the older list stays on screen |
| `SET_VOLUME` while the user drags the slider | at most every 100 ms, the latest value; the last value is always sent |
| `VOLUME_CHANGED` during a local volume change | not shown until 300 ms after the last `SET_VOLUME`; then the host's value is shown |

The host does not time out; it answers requests and reports changes.
These values do not change the frame format, so they are not a protocol
version change.

The protocol does not depend on a specific HID API.

## Goals

- Simple
- Deterministic
- Versioned
- Easy to debug
- Robust against malformed packets
- Suitable for small control messages

## Frame

All frames are 64 bytes. Multi-byte fields are little endian.

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 1 | magic | `0xA5` |
| 1 | 1 | version | `1` |
| 2 | 1 | command | see message set |
| 3 | 1 | sequence | sender's counter, incremented per frame sent, wraps 255 → 0 |
| 4 | 1 | length | payload bytes, 0–57 |
| 5 | length | payload | per command |
| 5 + length | … | padding | zero |
| 62 | 2 | crc | CRC-16 of bytes 0–61 |

CRC: CRC-16/CCITT-FALSE: polynomial `0x1021`, initial value `0xFFFF`,
no input/output reflection, no final XOR. Check value: `"123456789"` →
`0x29B1`. Stored low byte first at offset 62.

Senders must zero the padding. Receivers do not check it beyond the CRC.

### Example

`SELECT_DEVICE` with sequence `0x2A`, `device_index` = 2:

```text
offset 0..5:   A5 01 03 2A 01 02
offset 6..61:  00 (56 bytes)
offset 62..63: 87 2B            (CRC 0x2B87)
```

## Message set

| Code | Command | Direction | Payload |
|---|---|---|---|
| `0x01` | `GET_DEVICE_LIST` | STM32 → Host | none |
| `0x02` | `DEVICE_LIST` | Host → STM32 | one device entry (below) |
| `0x03` | `SELECT_DEVICE` | STM32 → Host | `device_index` (0–254) |
| `0x04` | `DEVICE_CHANGED` | Host → STM32 | `device_index` of the current default (0–254), or `0xFF` = none |
| `0x05` | `SET_VOLUME` | STM32 → Host | `volume` (0–100, percent) |
| `0x06` | `VOLUME_CHANGED` | Host → STM32 | `volume` (0–100) |
| `0x07` | `SET_MUTE` | STM32 → Host | `muted` (0 or 1) |
| `0x08` | `MUTE_CHANGED` | Host → STM32 | `muted` (0 or 1) |
| `0x7F` | `ERROR` | Either | `code`, `ref_command`, `ref_sequence` (1 byte each) |

Volume and mute refer to the current default playback device. After
`DEVICE_CHANGED` with `0xFF` (no default device) the STM32 treats them as
unknown; when a default device appears again the host sends
`VOLUME_CHANGED` and `MUTE_CHANGED` even if the values did not change.

### Device list

A device list is sent as `total` `DEVICE_LIST` frames, one device each, in
display order. Payload (7 + `name_len` bytes):

| Byte | Field | Value |
|---|---|---|
| 0 | `generation` | same in every frame of one list; the host changes it for each new list |
| 1 | `total` | number of devices, 0–32 |
| 2 | `position` | 0 … `total` − 1 |
| 3 | `device_index` | 0–254, assigned by the host |
| 4 | `flags` | bit 0: current default device; other bits 0 |
| 5 | `icon` | 0 generic, 1 speakers, 2 headphones, 3 headset, 4 display, 5 digital output |
| 6 | `name_len` | 0–48 |
| 7 … | `name` | `name_len` bytes of UTF-8, no NUL bytes, no terminator |

- An empty list is one frame with `total` = 0, `position` = 0,
  `device_index` = `0xFF`, `flags` = 0, `icon` = 0, `name_len` = 0.
- The host truncates names to 48 bytes at a UTF-8 character boundary.
- The firmware assembles the list by `position` and replaces its list
  only when all `total` entries of one `generation` have arrived. A frame
  with a different `generation` discards a partially received list.
- The host sends a list in reply to `GET_DEVICE_LIST` and whenever the set
  of playback devices or their names change.

## Device identity

Windows endpoint IDs are opaque strings and never leave the host.

The host assigns each device a session-local `device_index` (0–254) and
keeps the mapping to the Windows endpoint ID. An index is valid until the
next device list; the firmware only echoes indices it received.

Indices belong to the session that sent them. A session starts when the
host sends the list after it has (re)opened the device, on its own or in
answer to `GET_DEVICE_LIST`. Until then the host answers `SELECT_DEVICE`
with `ERROR UNKNOWN_DEVICE`, which makes the STM32 request the list: its
indices may come from an earlier session (for example, the host program
was restarted while the board stayed connected) and could name another
device now.

Known limit: a `SELECT_DEVICE` the STM32 sends after the host opened the
device but before the new list reached the STM32 (the list transfer, tens
of milliseconds after the host starts) carries an index of the earlier
session and is treated as one of the new session. Closing this needs the
list `generation` in `SELECT_DEVICE`, a frame-format change (version 2).

## Exchanges

| Trigger | Frames |
|---|---|
| Firmware starts or USB (re)connects | STM32 → `GET_DEVICE_LIST`; Host → `DEVICE_LIST` × total, then `DEVICE_CHANGED`, `VOLUME_CHANGED`, `MUTE_CHANGED` |
| Host opens the device (host program starts, or USB (re)connects) | Host → `DEVICE_LIST` × total, then `DEVICE_CHANGED`, `VOLUME_CHANGED`, `MUTE_CHANGED`, without a request |
| User selects a device | STM32 → `SELECT_DEVICE`; Host → `DEVICE_CHANGED` after Windows switched, or `ERROR` |
| Default device, volume or mute changed in Windows | Host → `DEVICE_CHANGED` / `VOLUME_CHANGED` / `MUTE_CHANGED` |
| Devices added, removed or renamed | Host → new `DEVICE_LIST` (new `generation`), then `DEVICE_CHANGED` |
| User changes volume or mute | STM32 → `SET_VOLUME` / `SET_MUTE`; Host → `VOLUME_CHANGED` / `MUTE_CHANGED`, or `ERROR OPERATION_FAILED` |

The firmware shows the state reported by the host. It does not mark a
device as active after `SELECT_DEVICE` until `DEVICE_CHANGED` arrives.
It accepts a `DEVICE_LIST` at any time, requested or not. When the board
is plugged in, its request and the host's own list can cross; the second
complete list simply replaces the first.

Retries and timeouts: see "Transport".

## Errors and validation

A receiver checks, in this order, and stops at the first failure:

1. magic is `0xA5`;
2. version is `1`;
3. CRC matches;
4. length ≤ 57;
5. command is known;
6. payload length and every field value match the rules above.

A rejected frame is dropped. The receiver may answer with `ERROR`:

| Code | Name | Meaning |
|---|---|---|
| `0x01` | `BAD_FRAME` | magic, CRC or length wrong |
| `0x02` | `UNSUPPORTED_VERSION` | version is not 1 |
| `0x03` | `UNKNOWN_COMMAND` | command code not in the message set |
| `0x04` | `BAD_PAYLOAD` | payload length or a field value invalid |
| `0x05` | `UNKNOWN_DEVICE` | `device_index` not in the current list |
| `0x06` | `OPERATION_FAILED` | valid request, but it could not be carried out (e.g. Windows refused) |
| `0x07` | `BUSY` | request cannot be handled now |

`ref_command` and `ref_sequence` name the frame being answered (0 when
none). A receiver never answers an `ERROR` frame with another `ERROR`.

## Compatibility

Any incompatible protocol change requires:

1. version update;
2. protocol documentation update;
3. protocol tests;
4. firmware and host implementation updates.

A receiver rejects frames of any other version (`UNSUPPORTED_VERSION`).

## History

| Version | Change |
|---|---|
| 0 | Draft: message list and a header proposal (magic, version, command, 16-bit length). |
| 1 | Fixed 64-byte frame with sequence byte, 8-bit length and CRC-16; payloads, device list format, error codes and validation rules defined. |
