# ADR-004: Device preferences and UI settings (protocol version 2)

## Status

Accepted by the developer on 2026-10-14 (issue #58). Protocol version 2
is implemented in `protocol/`, `firmware/link` and the host controller
(#58); the preference file, the `alias` command and the volume
limit in the host program (#59, file `preferences.txt`, text format in
`host/App/PreferenceFile.h`). The UI follows in #60.

Changed by the developer on 2026-10-16: the volume limit acts when the
board selects a device, not at startup (section "Volume limit on
switch"). The record's bytes are unchanged, so the protocol version stays
2.

One addition during implementation: a settings change made on the board
while no host is connected is sent when the host's `SETTINGS` arrive,
instead of being replaced by them (otherwise flipping the display before
the PC is up would be undone).

## Context

The M7 features requested on 2026-10-14 need state that outlives a USB
session and a host restart:

- a user **alias** per playback device (Windows names are long and are
  cut to 48 bytes by protocol v1);
- a user-defined **display order** of the devices, changed on the
  touchscreen;
- **favorites**, shown in their own tab;
- **UI settings** chosen on a settings page on the touchscreen: layout
  (A cards, B list, C large tile — all three ship, #57), theme
  (dark/light), accent colour (6 colours, orange default), card grid
  (2×2, 2×3, 3×3, list), startup tab (last used, all, favorites), the
  last used tab, screen timeout, display flip, language
  (Turkish/English) and an optional **volume limit on switch**.

Decisions taken by the developer on 2026-10-14:

1. Aliases can be edited on the STM32 (on-screen keyboard) and with a
   host command.
2. All of this state is stored on the host and sent to the board when
   they connect. Nothing is stored in the STM32's flash.
3. Volume limit on switch: optional, off by default; when on, a device
   selected on the board whose volume is above the chosen percentage is
   lowered to it. (Until 2026-10-16: all outputs, when the host started
   or the board connected.)

Protocol v1 cannot carry this: it has no messages for it, its
`DEVICE_LIST` flags allow only bit 0, and v1 receivers reject unknown
commands. v1 also has a known limit: `SELECT_DEVICE` carries no list
`generation`, so a selection sent while a new list is on its way can be
read against the wrong list (`protocol.md`, "Device identity").

## Decision

### Where the state lives

The **host** keeps all preferences in one file,
`%APPDATA%\stm32-audio-controller\preferences` (format decided in #59),
written atomically (write a temporary file, then rename).

- Per-device preferences — alias, position in the user's order,
  favorite — are keyed by the **Windows endpoint ID**, the only identity
  that is stable across host restarts and USB reconnects. Endpoint IDs
  still never leave the host; the board only sees session-local
  `device_index` values, as in v1.
- A device that is not present keeps its preferences; when it returns,
  it comes back at its stored position.
- New devices (no stored position) are appended after the known ones,
  in the order Windows enumerates them.
- UI settings are one record (below). The host stores it as received
  and interprets only the volume-limit fields.

The **board** holds no persistent state. Until the host's `SETTINGS`
arrive (about 1–2 s after a cold boot, or never without a host), it
uses the defaults below; the waiting screen is therefore always drawn
in the default theme and orientation. This was accepted on 2026-10-14
in exchange for not writing flash.

### Protocol version 2

Frame format unchanged (64 bytes, CRC-16); `version` = **2**. Receivers
reject version 1 frames with `UNSUPPORTED_VERSION`; firmware and host
are updated together.

Changed messages:

| Code | Command | Change |
|---|---|---|
| `0x02` | `DEVICE_LIST` | Entries in the **user's order**; `flags` bit 1 = favorite, bit 2 = an alias follows in a `DEVICE_ALIAS` frame. `name` stays the Windows name (up to 48 bytes). |
| `0x03` | `SELECT_DEVICE` | Payload `generation`, `device_index` (2 bytes). The host answers `ERROR UNKNOWN_DEVICE` when `generation` is not the list it sent last. Closes the v1 limit. |

New messages:

| Code | Command | Direction | Payload |
|---|---|---|---|
| `0x09` | `DEVICE_ALIAS` | Host → STM32 | `generation`, `position`, `alias_len` (1–32), `alias` (UTF-8) |
| `0x0A` | `SET_ALIAS` | STM32 → Host | `generation`, `device_index`, `alias_len` (0–32; 0 = remove the alias), `alias` |
| `0x0B` | `SET_FAVORITE` | STM32 → Host | `generation`, `device_index`, `favorite` (0 or 1) |
| `0x0C` | `MOVE_DEVICE` | STM32 → Host | `generation`, `device_index`, `new_position` (0 … `total` − 1) |
| `0x0D` | `SETTINGS` | Host → STM32 | settings record (below) |
| `0x0E` | `SET_SETTINGS` | STM32 → Host | settings record (below) |

**Aliases belong to the list.** For every entry with flag bit 2 the host
sends one `DEVICE_ALIAS` frame of the same `generation` after the
`DEVICE_LIST` frames. The firmware replaces its list only when all
`total` entries **and** all announced aliases of one `generation` have
arrived; the 1000 ms incomplete-list rule of v1 covers lost alias frames
too. Aliases are limited to 32 bytes (at least 16 Turkish characters),
cut by the host at a UTF-8 character boundary; they are meant to be
short.

**Edits are answered with a new list.** On `SET_ALIAS`, `SET_FAVORITE`
or `MOVE_DEVICE` with the current `generation` and a known
`device_index`, the host stores the change and sends a new list (new
`generation`), then `DEVICE_CHANGED`. Wrong `generation` or index →
`ERROR UNKNOWN_DEVICE` (the board requests the list again); storage
failure → `ERROR OPERATION_FAILED`. The board shows an edit only when the
new list arrives, as it does for selections in v1; the 2000 ms response
timeout applies.

**Settings record** (11 bytes, payload of `SETTINGS` and
`SET_SETTINGS`):

| Byte | Field | Values | Default |
|---|---|---|---|
| 0 | `layout` | 0 cards (A), 1 list (B), 2 large tile (C) | 0 |
| 1 | `theme` | 0 dark, 1 light | 0 |
| 2 | `accent` | 0 orange, 1 blue, 2 green, 3 purple, 4 pink, 5 yellow | 0 |
| 3 | `grid` | 0 2×2, 1 2×3, 2 3×3, 3 list (layout A only) | 0 |
| 4 | `startup_tab` | 0 last used, 1 all, 2 favorites | 0 |
| 5 | `last_tab` | 0 all, 1 favorites | 0 |
| 6 | `screen_timeout` | minutes until the backlight turns off: 0 never, 1–60 | 0 |
| 7 | `flipped` | 0 upright, 1 turned by 180° | 0 |
| 8 | `language` | 0 Turkish, 1 English | 0 |
| 9 | `volume_limit_on` | 0 off, 1 on | 0 |
| 10 | `volume_limit` | 0–100 percent | 50 |

- The host sends `SETTINGS` after the list and the state frames
  (`DEVICE_CHANGED`, `VOLUME_CHANGED`, `MUTE_CHANGED`) whenever it sends
  a list at the start of a session, and in answer to every
  `SET_SETTINGS`.
- The board sends `SET_SETTINGS` with the whole record whenever the
  user changes a setting or the tab (`last_tab`). The host writes the
  file at most once per second (the record is small and changes in
  bursts) and always answers with `SETTINGS`.
- A field outside its range makes the frame invalid (`BAD_PAYLOAD`), as
  in v1. New fields need a new protocol version.

### Volume limit on switch (host)

When `volume_limit_on` is 1 and a `SELECT_DEVICE` names a device whose
volume is above `volume_limit`, the host lowers that device to
`volume_limit` **before** it makes it the Windows default, so the new
output never plays above the limit.

It acts only on `SELECT_DEVICE` from the board. It does not act when the
host starts, when a board session starts, on a default change made in
Windows (or by Windows, e.g. a headset plugged in), or on later volume
changes. It does not raise volumes and does not touch mute. The firmware
needs nothing special: the limited volume reaches it as
`VOLUME_CHANGED` after `DEVICE_CHANGED`.

Changed on 2026-10-16 (developer's decision). Before, the host lowered
every active output above the limit when it started and when each board
session started. The fields, their ranges and the defaults are the same,
so protocol version 2 is unchanged.

### Host command

`stm32-audio-host alias <n> "<name>"` and `alias <n> --clear` (where
`<n>` is the number printed by `list`) write the preference file. A
running `stm32-audio-host run` notices the file change (checked once per
second) and sends a new list.

## Alternatives considered

- **Alias in the `DEVICE_LIST` frame.** 7 header bytes + 48-byte name +
  alias does not fit the 57-byte payload; dropping the Windows name
  would lose it on layouts B and C and in the alias editor, which show
  both.
- **Board sends the whole order (`SET_ORDER`).** Up to 32 indices in one
  frame fit, but a single move from the edit screen is simpler to
  validate and to answer; a full order is never edited at once on the
  touchscreen.
- **Key/value settings.** More flexible, but every key still needs a
  definition, validation and tests. A fixed 11-byte record is simpler to
  check and to test; adding a field costs a version step, which is
  acceptable for a single firmware/host pair.
- **Settings in STM32 flash.** Gives the right theme and orientation from
  power-on without a host, at the cost of flash erase/write code and a
  stall of the CPU during sector erase. Rejected by the developer on
  2026-10-14; can be revisited with a new version if the default
  waiting screen turns out to be a problem.
- **Staying on version 1 with additions.** v1 receivers reject unknown
  commands and flag bits, so the change is incompatible either way; a
  clean version 2 also fixes the `SELECT_DEVICE` limit.

## Consequences

- `protocol/protocol.md`, `include/protocol.h`, `src/protocol.c` and the
  protocol tests change to version 2; `firmware/link` keeps up to 32
  aliases (32 × 33 bytes, about 1 KB per list buffer) and gets edit
  requests with the response timeout; `tests/integration` covers the
  new exchanges.
- The host gains a preference store, the edit handling, the settings
  record, the volume limit and the `alias` command (#59).
- The firmware UI gets the three layouts, themes, accent colours, the
  edit mode, the keyboard and the settings page (#60); it applies
  `SETTINGS` when they arrive (theme, layout and flip change without a
  reset).
- Board and host must be updated together; a v1 host and a v2 board (or
  the reverse) reject each other's frames and the board stays on the
  waiting screen.
