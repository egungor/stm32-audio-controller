# Roadmap

Status legend: **DONE** — implemented and verified where hardware or
Windows is involved; **CODE DONE** — implemented and tested in CI, the
hardware/Windows check is open (`docs/hardware.md`, "Pending hardware and
Windows tests"); **IN PROGRESS**; **NOT STARTED**.

The M1–M6 board and Windows checks ran on 2026-10-14 (issue #48; results
in the `docs/hardware.md` bring-up log).

## M0 — AI Development Environment

Status: **DONE**

- [x] Repository structure
- [x] Claude Code project instructions
- [x] Specialist agent definitions
- [x] Architecture document
- [x] Hardware document
- [x] Protocol document
- [x] GitHub issue/PR templates
- [x] CI skeleton
- [x] Verify exact STM32 board model (STM32F746G-DISCO / MB1191, STM32F746NGH6)

## M1 — Hardware Bring-up

Status: **DONE**

- [x] Create STM32CubeMX project (CMake, `firmware/STM32AudioController/`)
- [x] Review enabled peripherals — `docs/decisions/ADR-001-firmware-peripheral-set.md`
- [x] Verify clocks (SYSCLK configured at 200 MHz) — measurement against the LSE at boot (#16), `ppm=12` on the board
- [x] Initialize LCD — SDRAM framebuffer + LTDC, issue #7; SDRAM also from a power cycle
- [x] Initialize touch — FT5336 driver (#18)
- [x] Display a static test screen — M1 test pattern, verified on the panel 2026-10-13
- [x] Verify touch coordinates — five targets on the test screen (#18)
- [x] Document hardware pin/peripheral mapping — `docs/pin-mapping.md`, issue #14

## M2 — STM32 UI

Status: **DONE**

- [x] Add LVGL
  - [x] LVGL v9.5.0 submodule, `lv_conf.h`, fonts with Turkish letters (#22)
  - [x] Hardware-independent UI module and Linux simulator with screenshots (#22)
  - [x] Port to the board: LTDC display driver, FT5336 input, timer task (#24)
- [x] Create main screen — `firmware/ui` (#22)
- [x] Create 2×2 audio device buttons
- [x] Add active-device indication — colour and text, plus a pending mark until the host confirms
- [x] Add pagination — four devices per page, arrows
- [x] Create device model — device list from the host (`firmware/link`, #30), no model of its own on the STM32
- [x] Capture LCD screenshots of the UI into `docs/images/` (#48)
- [x] Add the screenshots to the project presentation (#67, see `docs/images/README.md`)

## M3 — USB HID

Status: **DONE**

- [x] Configure USB Device — vendor HID class, `1209:0001` (#30, ADR-003)
- [x] Define HID descriptor — ADR-003, `protocol.md` "Transport" (#30)
- [x] Implement HID transport — firmware side and protocol client (#30), Windows `WinHidLink` and `run` (#32)
- [x] Test host ↔ STM32 packets
- [x] Handle disconnect/reconnect — firmware: waiting screen and list re-request (#30), no false `CONFIGURED` after unplugging (#52); host: rescan every second, controller reset (#32)

## M4 — Windows Audio

Status: **DONE**

- [x] Create Windows C++17 host project — platform-independent core + tests (#26)
- [x] Enumerate render endpoints (#28)
- [x] Build audio device model — `DeviceRegistry`, protocol controller (#26)
- [x] Detect current default endpoint (#28)
- [x] Implement default endpoint switching (#28, ADR-002)
- [x] Monitor endpoint/default-device changes (#28)

## M5 — Integration

Status: **DONE** — except endpoint removal while connected, NOT TESTED on hardware

The firmware protocol client and the host controller also run against
each other in `tests/integration` (simulated USB link and Windows, #34).

- [x] Send device list to STM32
- [x] Select device from STM32
- [x] Update Windows default endpoint
- [x] Synchronize Windows changes back to STM32
- [ ] Handle endpoint removal — logic tested (#34); adding or removing a Windows device while the board is connected NOT TESTED on hardware
- [x] Handle USB disconnect

## M6 — Audio Controls

Status: **DONE**

- [x] Volume read/write — host (#26, #28), STM32 protocol client with rate limiting (#40)
- [x] Mute — host (#26, #28), STM32 (#40)
- [x] Volume UI — bottom strip slider and percentage (#40)
- [x] Mute UI — bottom strip button (#40)

## M7 — Polish

Status: **IN PROGRESS**

Order: protocol version 2 (ADR-004, #58) → host (#59) → firmware UI
(#60). Designs: #57.

- [ ] Friendly names — aliases, edited on the touchscreen and with `stm32-audio-host alias` (#58–#60)
- [ ] Favorites — favorite flag and a favorites tab (#58–#60)
- [ ] Custom ordering — edit mode on the touchscreen (#58–#60)
- [ ] Layouts and themes — A cards (2×2, 2×3, 3×3, list), B list, C large tile; dark/light; six accent colours (#57, #60)
- [ ] Settings page — layout, theme, colour, grid, startup tab, screen timeout, flip, language, startup volume limit (#58–#60); page coded and tested in the simulator except "layout" (waits for layouts B and C), on the board REQUIRES HARDWARE; the screen timeout is stored but not acted on yet
- [ ] Icons — form factor icons exist (speakers, headphones, headset, display, digital, generic; #22, #26)
- [ ] Windows startup option
- [ ] Error UI — waiting screen, error screen and notices for a refused or unanswered selection exist (#22, #30)
- [ ] Performance/memory review
- [x] Display flip (180°) — button and port (#42), frame copy 5 ms (#51), verified on the board

## v1.0

A reliable physical Windows audio-output controller suitable for daily use.
