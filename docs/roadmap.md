# Roadmap

Status legend: **DONE** — implemented and verified where hardware or
Windows is involved; **CODE DONE** — implemented and tested in CI, the
hardware/Windows check is open (`docs/hardware.md`, "Pending hardware and
Windows tests"); **NOT STARTED**.

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

Status: **CODE DONE** — clock and touch checks REQUIRE HARDWARE

- [x] Create STM32CubeMX project (CMake, `firmware/STM32AudioController/`)
- [x] Review enabled peripherals — `docs/decisions/ADR-001-firmware-peripheral-set.md`
- [ ] Verify clocks (SYSCLK configured at 200 MHz) — measurement against the LSE at boot (#16), REQUIRES HARDWARE
- [x] Initialize LCD — SDRAM framebuffer + LTDC, issue #7
- [ ] Initialize touch — FT5336 driver (#18), REQUIRES HARDWARE
- [x] Display a static test screen — M1 test pattern, verified on the panel 2026-10-13
- [ ] Verify touch coordinates — five targets on the test screen (#18), REQUIRES HARDWARE
- [x] Document hardware pin/peripheral mapping — `docs/pin-mapping.md`, issue #14

## M2 — STM32 UI

Status: **CODE DONE** — UI on the LCD REQUIRES HARDWARE

- [ ] Add LVGL
  - [x] LVGL v9.5.0 submodule, `lv_conf.h`, fonts with Turkish letters (#22)
  - [x] Hardware-independent UI module and Linux simulator with screenshots (#22)
  - [ ] Port to the board: LTDC display driver, FT5336 input, timer task — code in #24, REQUIRES HARDWARE
- [ ] Create main screen — `firmware/ui` (#22), simulator; on the LCD REQUIRES HARDWARE
- [ ] Create 2×2 audio device buttons — as above
- [ ] Add active-device indication — colour and text, plus a pending mark until the host confirms; as above
- [ ] Add pagination — four devices per page, arrows; as above
- [ ] Create device model — device list from the host (`firmware/link`, #30), no model of its own on the STM32; as above
- [ ] Capture LCD screenshots of the final UI into `docs/images/` and add
      them to the project presentation (see `docs/images/README.md`)

## M3 — USB HID

Status: **CODE DONE** — REQUIRES HARDWARE + WINDOWS

- [ ] Configure USB Device — vendor HID class, `1209:0001` (#30, ADR-003), REQUIRES HARDWARE
- [x] Define HID descriptor — ADR-003, `protocol.md` "Transport" (#30)
- [ ] Implement HID transport — firmware side and protocol client (#30), Windows `WinHidLink` and `run` (#32); REQUIRES HARDWARE + WINDOWS
- [ ] Test host ↔ STM32 packets — REQUIRES HARDWARE + WINDOWS
- [ ] Handle disconnect/reconnect — firmware: waiting screen and list re-request (#30); host: rescan every second, controller reset (#32); REQUIRES HARDWARE + WINDOWS

## M4 — Windows Audio

Status: **CODE DONE** — REQUIRES WINDOWS

- [x] Create Windows C++17 host project — platform-independent core + tests (#26)
- [ ] Enumerate render endpoints — code in #28, REQUIRES WINDOWS
- [x] Build audio device model — `DeviceRegistry`, protocol controller (#26)
- [ ] Detect current default endpoint — code in #28, REQUIRES WINDOWS
- [ ] Implement default endpoint switching — code in #28 (ADR-002), REQUIRES WINDOWS
- [ ] Monitor endpoint/default-device changes — code in #28, REQUIRES WINDOWS

## M5 — Integration

Status: **CODE DONE** — REQUIRES HARDWARE + WINDOWS

The firmware protocol client and the host controller run against each
other in `tests/integration` (simulated USB link and Windows, #34). That
covers the logic of every item below; each still needs the board and a
Windows PC (`host/README.md`, "Manual check with the board").

- [ ] Send device list to STM32 — logic tested (#34), REQUIRES HARDWARE + WINDOWS
- [ ] Select device from STM32 — logic tested (#34), REQUIRES HARDWARE + WINDOWS
- [ ] Update Windows default endpoint — logic tested (#34), REQUIRES HARDWARE + WINDOWS
- [ ] Synchronize Windows changes back to STM32 — logic tested (#34), REQUIRES HARDWARE + WINDOWS
- [ ] Handle endpoint removal — logic tested (#34), REQUIRES HARDWARE + WINDOWS
- [ ] Handle USB disconnect — logic tested (#34), REQUIRES HARDWARE + WINDOWS

## M6 — Audio Controls

Status: **CODE DONE** — REQUIRES HARDWARE + WINDOWS

- [ ] Volume read/write — host (#26, #28), STM32 protocol client with rate limiting (#40); REQUIRES HARDWARE + WINDOWS
- [ ] Mute — host (#26, #28), STM32 (#40); REQUIRES HARDWARE + WINDOWS
- [ ] Volume UI — bottom strip slider and percentage (#40), simulator; on the LCD REQUIRES HARDWARE
- [ ] Mute UI — bottom strip button (#40), simulator; on the LCD REQUIRES HARDWARE

## M7 — Polish

Status: **NOT STARTED** (two items partly exist)

- [ ] Friendly names
- [ ] Favorites
- [ ] Custom ordering
- [ ] Icons — form factor icons exist (speakers, headphones, headset, display, digital, generic; #22, #26)
- [ ] Windows startup option
- [ ] Error UI — waiting screen, error screen and notices for a refused or unanswered selection exist (#22, #30)
- [ ] Performance/memory review

## v1.0

A reliable physical Windows audio-output controller suitable for daily use.
