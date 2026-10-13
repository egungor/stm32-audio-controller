# Roadmap

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

Status: **IN PROGRESS**

- [x] Create STM32CubeMX project (CMake, `firmware/STM32AudioController/`)
- [x] Review enabled peripherals — `docs/decisions/ADR-001-firmware-peripheral-set.md`
- [ ] Verify clocks (SYSCLK currently configured at 200 MHz)
- [x] Initialize LCD — SDRAM framebuffer + LTDC, issue #7
- [ ] Initialize touch
- [x] Display a static test screen — M1 test pattern, verified on the panel 2026-10-13
- [ ] Verify touch coordinates
- [x] Document hardware pin/peripheral mapping — `docs/pin-mapping.md`, issue #14

## M2 — STM32 UI

- [ ] Add LVGL
  - [x] LVGL v9.5.0 submodule, `lv_conf.h`, fonts with Turkish letters (#22)
  - [x] Hardware-independent UI module and Linux simulator with screenshots (#22)
  - [ ] Port to the board: LTDC display driver, FT5336 input, timer task
- [ ] Create main screen
- [ ] Create 2×2 audio device buttons
- [ ] Add active-device indication
- [ ] Add pagination
- [ ] Create device model
- [ ] Capture LCD screenshots of the final UI into `docs/images/` and add
      them to the project presentation (see `docs/images/README.md`)

## M3 — USB HID

- [ ] Configure USB Device
- [ ] Define HID descriptor
- [ ] Implement HID transport
- [ ] Test host ↔ STM32 packets
- [ ] Handle disconnect/reconnect

## M4 — Windows Audio

- [ ] Create Windows C++17 host project
- [ ] Enumerate render endpoints
- [ ] Build audio device model
- [ ] Detect current default endpoint
- [ ] Implement default endpoint switching
- [ ] Monitor endpoint/default-device changes

## M5 — Integration

- [ ] Send device list to STM32
- [ ] Select device from STM32
- [ ] Update Windows default endpoint
- [ ] Synchronize Windows changes back to STM32
- [ ] Handle endpoint removal
- [ ] Handle USB disconnect

## M6 — Audio Controls

- [ ] Volume read/write
- [ ] Mute
- [ ] Volume UI
- [ ] Mute UI

## M7 — Polish

- [ ] Friendly names
- [ ] Favorites
- [ ] Custom ordering
- [ ] Icons
- [ ] Windows startup option
- [ ] Error UI
- [ ] Performance/memory review

## v1.0

A reliable physical Windows audio-output controller suitable for daily use.
