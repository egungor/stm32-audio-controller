# STM32 Firmware

This directory will contain the firmware for the STM32F746G-DISCO board
(MCU: STM32F746NGH6). See `docs/hardware.md`.

## Current state

The STM32CubeMX project lives in `STM32AudioController/` (CMake project
type, STM32F746G-DISCO board preset). Build instructions, presets and
tool versions are in `docs/toolchain.md`.

```text
cd firmware/STM32AudioController
cmake --preset Debug
cmake --build --preset Debug
```

Nothing has been flashed or tested on hardware yet (`REQUIRES HARDWARE`).

Open decisions:

- where application code (non-generated) lives and how it is added to
  `STM32AudioController/CMakeLists.txt`; the empty `App/`, `Core/`,
  `Drivers/` and `Middlewares/` placeholder folders predate the CubeMX
  project;
- which board-preset peripherals stay enabled.

## Planned components

- STM32Cube-generated startup/peripheral code
- application layer
- LVGL UI
- touch handling
- USB HID transport
- shared protocol adapter

The board model has been verified (see `docs/hardware.md`). The CubeMX
project should use the STM32F746G-DISCO board configuration, with GPIO and
peripheral assignments taken from the official ST documentation and
STM32CubeF7 BSP rather than guessed.
