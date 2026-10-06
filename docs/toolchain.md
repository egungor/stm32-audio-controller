# Firmware Toolchain

This document records the development toolchain for the STM32 firmware.

It separates **verified facts** (checked by the developer on the Windows
development host) from **future decisions** (not yet made). Anything not
listed as verified must not be assumed.

> Status: the toolchain has been installed and its versions verified.
> **No firmware project exists yet. No firmware has been built, flashed,
> debugged or tested.** No STM32CubeMX project has been created.

## Target

| Item | Value | Status |
|---|---|---|
| Board | STM32F746G-DISCO (ST order code: 32F746GDISCOVERY) | VERIFIED (physical board) |
| MCU | STM32F746NGH6 | VERIFIED (physical board) |
| On-board debug probe | ST-LINK/V2-1 | From ST documentation (see `docs/hardware.md`) |

See `docs/hardware.md` for the full hardware description.

## 1. Required developer tools

| Tool | Purpose |
|---|---|
| STM32CubeMX | MCU/clock/peripheral configuration and code generation (`.ioc`) |
| STM32CubeF7 MCU package | HAL/LL drivers, CMSIS, board support package (BSP) |
| GNU Tools for STM32 (`arm-none-eabi-gcc`) | Cross-compiler, assembler, linker, binutils |
| GNU GDB for STM32 (`arm-none-eabi-gdb`) | Debugger client |
| CMake | Build system generator |
| Ninja | Build tool used by CMake |
| STM32CubeProgrammer | Flashing the board through the on-board ST-LINK |
| Git | Source control |

## 2. Verified versions

Verified by the developer on the Windows development host.

| Tool | Verified version |
|---|---|
| STM32CubeMX | 6.18.1 |
| STM32CubeF7 | 1.17.4 |
| GNU Tools for STM32 (ARM GCC bundle) | 14.3.1+st.2 |
| GNU GDB for STM32 (ARM GDB bundle) | 14.3.1+st.2 |
| CMake | 3.26.3 (selected project CMake version) |
| Ninja | 1.13.2 |
| STM32CubeProgrammer | 2.23.0 |
| Git | 2.56.0.windows.1 |

The compiler (`arm-none-eabi-gcc`) comes from the GNU Tools for STM32
bundle 14.3.1+st.2. The debugger (`arm-none-eabi-gdb`) is a separate tool
and comes from the GNU GDB for STM32 bundle 14.3.1+st.2. Both are ST
builds (`+st` suffix), installed through the STM32Cube bundle manager of
the STM32Cube for VS Code extension.

GCC and GDB are separate tools with their own versioning. The two
bundle version strings above are bundle identifiers; they do not mean
that GCC and GDB are the same software version.

### Verified ARM GCC compiler details

| Item | Verified value |
|---|---|
| Executable | `arm-none-eabi-gcc` |
| Toolchain | GNU Tools for STM32 14.3.rel1.20251027-0700 |
| GCC version | 14.3.1 |
| GCC release | 20250623 |

## 3. Expected Windows PATH / tool locations

Only locations that have been observed are listed. Exact executable
paths inside these folders have **not** been recorded yet and must not
be assumed.

| Tool | Location | Status |
|---|---|---|
| STM32Cube bundles (installed by the STM32Cube for VS Code extension) | `%LOCALAPPDATA%\stm32cube\bundles\<bundle-name>\<version>\` | Folder layout observed |
| GNU GDB for STM32 | `%LOCALAPPDATA%\stm32cube\bundles\gnu-gdb-for-stm32\14.3.1+st.2\` | Folder observed |
| Ninja | `%LOCALAPPDATA%\stm32cube\bundles\ninja\1.13.2+st.1\` | Folder observed |
| STM32CubeProgrammer | `%LOCALAPPDATA%\stm32cube\bundles\programmer\2.23.0\` | Folder observed |
| ST-LINK GDB server | `%LOCALAPPDATA%\stm32cube\bundles\stlink-gdbserver\7.14.0+st.2\` | Folder observed (version not separately verified) |
| GNU Tools for STM32 (GCC) | `%LOCALAPPDATA%\stm32cube\bundles\gnu-tools-for-stm32\14.3.1+st.2\` | Verified |
| CMake | `C:\Program Files\CMake\` | Folder observed |
| Git | `C:\Program Files\Git\` | Folder observed |
| STM32CubeMX | Not recorded | TO BE RECORDED |
| STM32CubeF7 package repository | Not recorded (STM32CubeMX repository folder) | TO BE RECORDED |

Notes:

- STM32Cube bundles are **not added to the system PATH** by design. The
  STM32Cube for VS Code extension selects them per project.
- CMake **3.26.3** is the selected CMake version for this project. The
  STM32Cube bundle CMake 4.4.0+st.1
  (`%LOCALAPPDATA%\stm32cube\bundles\cmake\4.4.0+st.1\`) is also
  installed but is **not used for this project at this time**.
- Which tools must be reachable from a plain PowerShell PATH is a
  future decision.

## 4. Firmware build flow

**Planned, not yet implemented.** No CubeMX project or `CMakeLists.txt`
exists in the repository.

1. Configure the MCU, clocks and peripherals in STM32CubeMX and save
   the `.ioc` file under `firmware/`.
2. Generate code with the **CMake** project type.
3. Commit the `.ioc` file together with the generated code so that every
   regeneration is reviewable in a PR.
4. Keep application code in `firmware/App/`; keep edits to generated
   code inside CubeMX `USER CODE` sections.
5. Configure and build with CMake and Ninja, for example:

   ```text
   cmake --preset <preset>
   cmake --build --preset <preset>
   ```

   Preset names are a future decision.
6. Expected outputs: `.elf`, plus `.bin`/`.hex` and a `.map` file.
   Build outputs are not committed.

## 5. Firmware flashing flow

**Planned, not yet performed.**

- Flashing requires the **physical STM32F746G-DISCO connected by USB to
  the Windows development host** (ST-LINK/V2-1 USB connector).
- Tool: STM32CubeProgrammer (GUI or `STM32_Programmer_CLI`) over SWD
  through the on-board ST-LINK.
- The exact command line, connection options and ST-LINK firmware
  version are to be recorded when flashing is first performed.

## 6. Debugging flow

**Planned, not yet performed.**

- Requires the physical board connected to the Windows host.
- Expected tools: ST-LINK GDB server with GNU GDB for STM32, driven from
  VS Code (STM32Cube for VS Code extension) or the command line.
- Register view requires the STM32F746 SVD file (provided by the ST
  tooling; location not recorded).
- Logging over the ST-LINK virtual COM port is planned; the UART used
  must be taken from UM1907, not assumed.
- Debug configuration files (for example `.vscode/launch.json`) are not
  yet in the repository. Whether shared debug configuration is
  committed is a future decision.

## 7. Developer-only tools vs. CI

| Tool | Developer | CI (future) |
|---|---|---|
| STM32CubeMX | Required | Not required (generated code is committed) |
| STM32CubeF7 package | Required (used by CubeMX) | Not required (needed files are committed) |
| GNU Tools for STM32 (GCC) | Required | Required |
| CMake | Required | Required |
| Ninja | Required | Required |
| GNU GDB for STM32 | Required | Not required |
| ST-LINK GDB server | Required | Not required |
| STM32CubeProgrammer | Required | Not required |
| VS Code + STM32Cube extension | Optional | Not required |
| Git | Required | Provided by the CI runner |

No firmware CI job exists yet. The CI host OS and how the compiler is
provided in CI (ST bundle for Linux, Arm official build of the same GCC
version, or a container image) are future decisions.

## 8. Reproducibility requirements

- Every build-relevant tool version is pinned in this document. Changing
  a version requires a PR that updates this file.
- CI must use the same GCC version as developers (14.3.1).
- The STM32CubeMX version used to generate code must match the verified
  version (6.18.1). Regenerating with a different version requires a PR
  that updates this file.
- The STM32CubeF7 version (1.17.4) must match the version recorded in
  the `.ioc` file once the project exists.
- The project uses CMake 3.26.3. The STM32Cube bundle CMake 4.4.0+st.1
  is not used for this project at this time. The minimum CMake version
  declared in `CMakeLists.txt` is a future decision.
- Do not commit installed tools, the full STM32CubeF7 package, build
  output, or personal IDE settings.
- Line-ending policy (`.gitattributes`) is a future decision; the
  repository currently shows line-ending-only differences on Windows.

## 9. Hardware validation limitations

- Flashing, debugging and any hardware test require the physical
  STM32F746G-DISCO connected to the Windows development host.
- Claude Code / Claude running in the Cowork Linux VM **cannot directly
  access the physical STM32 board** (no USB/ST-LINK access). It can
  read and edit repository files and, once the toolchain is available
  there, may build firmware, but it cannot flash, debug or observe the
  board.
- Any hardware result must be produced by the developer on the Windows
  host and reported with the labels defined in `CLAUDE.md`
  (`VERIFIED`, `NOT VERIFIED`, `REQUIRES HARDWARE`, `NOT TESTED`).
- A successful build does not mean the firmware works on hardware.
