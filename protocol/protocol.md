# STM32 ↔ Windows Protocol

## Status

Draft — Protocol v0.

This document is the source of truth for communication between the STM32
firmware and the Windows host application.

## Transport

Planned transport: USB HID.

The protocol must remain independent of the specific HID API implementation.

## Goals

- Simple
- Deterministic
- Versioned
- Easy to debug
- Robust against malformed packets
- Suitable for small control messages

## Initial message set

| Command | Direction | Purpose |
|---|---|---|
| `GET_DEVICE_LIST` | STM32 → Host | Request current audio devices |
| `DEVICE_LIST` | Host → STM32 | Send device list |
| `SELECT_DEVICE` | STM32 → Host | Select an audio endpoint |
| `DEVICE_CHANGED` | Host → STM32 | Notify current default endpoint |
| `SET_VOLUME` | STM32 → Host | Change volume |
| `VOLUME_CHANGED` | Host → STM32 | Report volume |
| `SET_MUTE` | STM32 → Host | Change mute state |
| `MUTE_CHANGED` | Host → STM32 | Report mute state |
| `ERROR` | Either | Report protocol/application error |

## Packet format

Initial proposal:

```text
+--------+--------+--------+----------------+----------+
| MAGIC  | VERSION| CMD    | LENGTH         | PAYLOAD  |
+--------+--------+--------+----------------+----------+
| 1 byte | 1 byte | 1 byte | 2 bytes LE     | N bytes  |
+--------+--------+--------+----------------+----------+
```

The final packet size and integrity mechanism will be defined before M3.

## Device identity

Windows endpoint IDs are opaque strings and must not be interpreted by
firmware.

The host should assign a session-local numeric `device_index` for the
STM32 UI. The host remains the owner of the actual Windows endpoint ID.

## Compatibility

Any incompatible protocol change requires:

1. version update;
2. protocol documentation update;
3. protocol tests;
4. firmware and host implementation updates.
