# Roadmap

## M0 — AI Development Environment

Status: **IN PROGRESS**

- [x] Repository structure
- [x] Claude Code project instructions
- [x] Specialist agent definitions
- [x] Architecture document
- [x] Hardware document
- [x] Protocol document
- [x] GitHub issue/PR templates
- [x] CI skeleton
- [ ] Verify exact STM32 board model

## M1 — Hardware Bring-up

- [ ] Create STM32CubeIDE/CubeMX project
- [ ] Verify clocks
- [ ] Initialize LCD
- [ ] Initialize touch
- [ ] Display a static test screen
- [ ] Verify touch coordinates
- [ ] Document hardware pin/peripheral mapping

## M2 — STM32 UI

- [ ] Add LVGL
- [ ] Create main screen
- [ ] Create 2×2 audio device buttons
- [ ] Add active-device indication
- [ ] Add pagination
- [ ] Create device model

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
