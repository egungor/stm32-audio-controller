# Hardware

## Current hardware information

- MCU: STM32F756NGH6
- Board family: STM32 F7 Discovery
- Exact board model/part number: **TO BE VERIFIED**

## Why verification is required

STM32F7 Discovery/evaluation boards use different display, touch,
memory, USB, and codec configurations. The exact board model must be
confirmed before selecting the final BSP and LCD/touch drivers.

## Bring-up checklist

- [ ] Confirm exact board model printed on PCB
- [ ] Confirm MCU marking
- [ ] Identify LCD controller
- [ ] Identify touch controller
- [ ] Identify LCD resolution
- [ ] Identify touch interface/address
- [ ] Identify USB connector used for device communication
- [ ] Confirm ST-LINK connection
- [ ] Confirm available external RAM
- [ ] Confirm clock configuration
- [ ] Capture board photos in `docs/images/`

## Planned firmware peripherals

- LCD
- Touch controller
- USB Device / HID
- GPIO
- DMA where appropriate

## Hardware validation policy

A hardware-dependent feature is considered verified only after it has been
tested on the physical board.

AI agents must not infer hardware success from successful compilation.
