# Firmware memory review (M7)

Review of the M7 "Performance/memory review" item. Firmware: `main` at
`7c6ee90` (v0.7.0-beta.1), Release preset, GNU Tools for STM32 14.3.1.
All numbers come from the linker output, `arm-none-eabi-nm` and a build
with `-fstack-usage`. They are static numbers: nothing here was measured
on the board.

## Summary

| Region | Used | Size | Use | Status |
|---|---|---|---|---|
| Internal RAM (DTCM + SRAM1/2) | 184 720 B | 320 KB | 56 % | VERIFIED (linker) |
| Flash | 388 588 B | 1 MB | 37 % | VERIFIED (linker) |
| SDRAM, frame buffers | 4 × 256 KB at `0xC0000000` | 8 MB | 12.5 % | VERIFIED (`ui_port.h`) |
| LVGL heap (`LV_MEM_SIZE`) | peak 53 KB | 128 KB | 41 % | simulator only (`firmware/ui/README.md`); on the board NOT VERIFIED |
| Task stacks, FreeRTOS heap at run time | not measured yet; the `MEM` log line reports them (see "Measured on the board") | | | REQUIRES HARDWARE |

There is room in every region. No change is needed for the size of any
buffer.

## Internal RAM

| Symbol | Size | What |
|---|---|---|
| `work_mem_int` | 128 KB | LVGL heap (`lv_conf.h`, #60) |
| `ucHeap` | 32 KB | FreeRTOS heap, `heap_4` (`configTOTAL_HEAP_SIZE`) |
| `s_link` | 5.6 KB | link client state, device list |
| `g_ui` | 3.4 KB | UI state |
| `s_rx_queue_buf` | 2.5 KB | USB receive queue |
| `hpcd_USB_OTG_FS`, `hUsbDeviceFS`, `USBD_StrDesc` | 2.5 KB | USB device stack |
| `xIdleStack`, `s_turn_chunk` | 1 KB each | idle task stack, display flip copy chunk |
| heap + main stack (`_Min_Heap_Size`, `_Min_Stack_Size`) | 0.5 + 1 KB | newlib heap, MSP (interrupts once the scheduler runs) |

About 140 KB of internal RAM is free.

The FreeRTOS heap holds the only application task, `defaultTask`. Its
stack is 4096 words = 16 KB (`Core/Src/freertos.c`). Software timers are
off (`configUSE_TIMERS` 0). How much of the 32 KB stays free at run time
is not measured.

## Flash

`.text` is 262 KB and `.rodata` 117 KB. The four glyph bitmaps of the UI
fonts take about 82 KB of `.rodata`. The largest functions are LVGL's
software blend routines (5 KB each). Flash use is not a concern.

## Stack frames (`-fstack-usage`)

| Code | Largest frame |
|---|---|
| App, link, UI (`firmware/STM32AudioController/App`, `firmware/link`, `firmware/ui`) | 448 B `on_connect` (`ui_demo.c`, demo build only); otherwise ≤ 216 B (`ui_settings_build`) |
| LVGL | 776 B (`lv_table` draw), 688 B (button matrix, bar), 592 B (arc) |
| LVGL redraw recursion | `lv_obj_redraw` 352 B + `lv_obj_draw` 272 B per level of the object tree |

No frame is larger than 1 KB. The redraw recursion grows with the depth
of the object tree. A rough bound for the UI's trees, a few KB, fits in
the 16 KB task stack. The bound is an estimate: the real depth is not
measured (REQUIRES HARDWARE).

## Findings

1. **The overflow and allocation failure hooks do nothing.**
   `configCHECK_FOR_STACK_OVERFLOW` is 2 and `configUSE_MALLOC_FAILED_HOOK`
   is 1, but `vApplicationStackOverflowHook` and
   `vApplicationMallocFailedHook` are CubeMX's empty `__weak` stubs in
   `Core/Src/freertos.c`, and nothing overrides them. A detected overflow
   or a failed `pvPortMalloc` carries on silently. Proposed: strong
   definitions in `App/` that log one line and stop in a known state.
   **Addressed** (`fix/60-rtos-hooks-mem-log`): `App/Src/rtos_diag.c`
   defines both hooks. Each masks all interrupts, writes
   `RTOS STACK OVERFLOW task=<name>` or `RTOS MALLOC FAILED` to USART1 by
   polling the registers (`Log_Panic`, no HAL, no allocation) and loops
   for ever. The CubeMX stubs in `Core/Src/freertos.c` are unchanged; the
   `arm-none-eabi-nm` shows the strong definitions in the image (VERIFIED, build).
   Neither failure has been provoked on the board (REQUIRES HARDWARE).
2. **No run-time watermarks.** The firmware never reports the task stack
   high-water mark (`uxTaskGetStackHighWaterMark`), the smallest free
   FreeRTOS heap (`xPortGetMinimumEverFreeHeapSize`) or the LVGL heap peak
   (`Ui_MemoryUse`, used only in the simulator). Proposed: one log line
   with these three numbers, so a board test can fill in the "not
   measured" rows above. **Addressed** (`fix/60-rtos-hooks-mem-log`): the
   UI loop of `defaultTask` logs
   `MEM stack_free=<words> rtos_heap_min=<bytes> lvgl_used=<bytes> lvgl_peak=<bytes>`
   after `UI INIT OK` and every 60 s (`docs/firmware-log.md`).
   `INCLUDE_uxTaskGetStackHighWaterMark` is set in the `USER CODE Defines`
   section of `FreeRTOSConfig.h`. The numbers are still to be read on the
   board (REQUIRES HARDWARE, `docs/hardware.md`).
3. **Performance.** Only the display flip copy has been measured (5 ms per
   frame, #51). Touch-to-select latency and frame rate while dragging a
   slider are not measured (REQUIRES HARDWARE). The pending check for
   layouts B and C in `docs/hardware.md` covers tearing while dragging.

## Measured on the board

To be filled from the last `MEM` line of the check in `docs/hardware.md`
("Memory watermarks"), with the date and the firmware commit. With the
hooks and the `MEM` line the Release image is 389 332 B of flash (+744 B);
internal RAM is unchanged at 184 720 B.

| Value | Size | Measured | Status |
|---|---|---|---|
| `defaultTask` stack, smallest free (`stack_free`, words) | 4096 words | | REQUIRES HARDWARE |
| FreeRTOS heap, smallest free (`rtos_heap_min`) | 32 768 B | | REQUIRES HARDWARE |
| LVGL heap peak (`lvgl_peak`) | 131 072 B | | REQUIRES HARDWARE |
