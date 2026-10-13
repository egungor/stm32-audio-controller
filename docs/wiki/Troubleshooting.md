# Troubleshooting

## Firmware build stops with "LVGL not found"

The LVGL submodule is missing: `git submodule update --init`.

## CMake is the wrong version / the preset fails

The system CMake (3.26.3 on the developer PC) is first on the PATH. Put the
STM32Cube bundles first, as in [Getting Started](Getting-Started#2-build-the-firmware-windows-powershell);
`cmake --version` must print 4.4.0. From Git Bash convert the path with
`cygpath` (a Windows-style path in the Bash PATH is silently ignored).

## No log lines

- The log is on the **ST-LINK** USB connector, 115200 8N1. The COM port
  number differs per PC: `Get-CimInstance Win32_SerialPort`.
- Open the port before the reset (`read-vcp.ps1 -Reset`), or the first
  lines are lost. Only one program can hold the port.
- Close a VS Code debug session before flashing; it holds the ST-LINK.

## `SELFTEST SDRAM FAIL`

Note the `test=` and the address. The LTDC must be off during SDRAM init
(see [Hardware](Hardware#bring-up-lessons)); `STM32_Programmer_CLI -r32`
reads SDRAM words directly.

## The host does not find the board

- The PC link is the **USB OTG FS** micro-USB connector, not ST-LINK.
- The log must show `USB CONFIGURED`; Device Manager must list VID `1209`
  PID `0001`. A firmware built with `APP_UI_DEMO` does not use the PC link.
- `run` scans once a second; another program holding the device is fine
  (shared access), but two `run` instances both receive every frame.

## The LCD shows the waiting screen ("Bilgisayar bekleniyor")

No host is talking to the board: start `stm32-audio-host run`. After a
selection that the host did not answer in 2 s the board also returns to
the waiting screen and asks for the list again.

## `set-default` fails

Switching uses the undocumented `IPolicyConfig` interface
([ADR-002](https://github.com/egungor/stm32-audio-controller/blob/main/docs/decisions/ADR-002-default-device-switching.md)). If a
Windows update breaks it, `set-default` reports FAIL and the board shows
the notice "Ses çıkışı değiştirilemedi"; the rest keeps working.
