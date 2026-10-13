# Protocol (v1)

Authoritative description: [protocol/protocol.md](https://github.com/egungor/stm32-audio-controller/blob/main/protocol/protocol.md).
Both sides link the same C codec (`protocol/src/protocol.c`).

## Frame

Every message is one 64-byte frame, which is exactly one HID report.

| Offset | Size | Field |
|---|---|---|
| 0 | 1 | magic `0xA5` |
| 1 | 1 | version `1` |
| 2 | 1 | command |
| 3 | 1 | sequence (per sender, wraps) |
| 4 | 1 | payload length, 0–57 |
| 5 | n | payload, then zero padding |
| 62 | 2 | CRC-16/CCITT-FALSE of bytes 0–61, low byte first |

## Commands

| Code | Command | Direction | Payload |
|---|---|---|---|
| `0x01` | `GET_DEVICE_LIST` | STM32 → host | – |
| `0x02` | `DEVICE_LIST` | host → STM32 | one device: generation, total, position, index, flags, icon, name (≤ 48 B UTF-8) |
| `0x03` | `SELECT_DEVICE` | STM32 → host | device index |
| `0x04` | `DEVICE_CHANGED` | host → STM32 | index of the default, `0xFF` = none |
| `0x05`/`0x06` | `SET_VOLUME` / `VOLUME_CHANGED` | → host / → STM32 | 0–100 |
| `0x07`/`0x08` | `SET_MUTE` / `MUTE_CHANGED` | → host / → STM32 | 0 or 1 |
| `0x7F` | `ERROR` | either | code, referenced command, referenced sequence |

A list of *n* devices is *n* `DEVICE_LIST` frames with the same
`generation`; the STM32 replaces its list only when all have arrived.

## Typical exchanges

```text
Board plugged in / host opens the board
  STM32 → GET_DEVICE_LIST            (repeated every 1 s until a list arrives)
  host  → DEVICE_LIST × n, DEVICE_CHANGED, VOLUME_CHANGED, MUTE_CHANGED

User taps a device
  STM32 → SELECT_DEVICE index
  host  → DEVICE_CHANGED index       (after Windows switched)
       or ERROR UNKNOWN_DEVICE / OPERATION_FAILED

Windows changes
  host  → DEVICE_CHANGED             (default changed)
  host  → DEVICE_LIST × n, DEVICE_CHANGED   (devices added/removed/renamed)
```

## USB transport

Vendor HID, VID:PID `1209:0001` (pid.codes test range, development only),
usage page `0xFF00`, one 64-byte input and one 64-byte output report on
interrupt endpoints (5 ms), no report ID. On Windows the read/write buffers
are 65 bytes: report ID 0, then the frame.

## Changing the protocol

Any incompatible change needs a version bump, `protocol.md`, codec tests,
and both implementations. Known open point: a selection made in the tens
of milliseconds after the host program starts can refer to the previous
session's list; closing it needs a generation byte in `SELECT_DEVICE`
(version 2).
