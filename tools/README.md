# Tools

| Tool | Purpose |
|---|---|
| `build-firmware-linux.sh` | Build the firmware on Linux with toolchains from `.toolchain/` (used by CI and Claude's sandbox; the developer build is in `docs/toolchain.md`) |
| `run-ui-simulator.sh` | Build the headless UI simulator and write its screens as PNG |
| `fb2png.py` | Convert a raw RGB565 framebuffer dump (from the board or the simulator) to PNG |
| `gen-ui-fonts.sh` | Regenerate the LVGL fonts in `firmware/ui/fonts` |
| `read-vcp.ps1` | Read the firmware log from the ST-LINK virtual COM port on Windows |
