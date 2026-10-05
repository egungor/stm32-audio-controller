# STM32 Firmware

This directory will contain the firmware for the STM32F746G-DISCO board
(MCU: STM32F746NGH6). See `docs/hardware.md`.

Planned components:

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
