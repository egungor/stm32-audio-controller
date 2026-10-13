# UI Simulator

Headless Linux build of the touch UI in `firmware/ui/`, using the same
LVGL version and `lv_conf.h` as the firmware. It renders into a
480 × 272 RGB565 framebuffer (the LCD format), plays a fixed script of
UI states and taps, and writes one screenshot per step.

It needs no display, SDL or board, so it runs in CI and in the cloud
development container.

## Run

```text
git submodule update --init          # once: fetches LVGL
tools/run-ui-simulator.sh            # build, run, convert to PNG
```

Screenshots land in `build/ui-sim/` (or the directory given as the first
argument):

| File | State |
|---|---|
| `01-waiting.png` | No computer connected |
| `02-main.png` | Device list, "Kulaklık" active, page 1 of 2 |
| `03-pending.png` | "Hoparlörler" tapped, waiting for the host to confirm |
| `04-switched.png` | Host confirmed: "Hoparlörler" active |
| `05-page2.png` | Next page button tapped |
| `06-error.png` | "Bilgisayar yanıt vermiyor" notice over the main screen |
| `07-error-gone.png` | Same screen after the notice timed out |
| `08-empty.png` | Host reports no playback devices |

The script also checks behaviour: a tap calls the selection callback with
the right device, a tap on the active device does nothing, and the page
button changes the page. The simulator exits non-zero if a check fails.

## How it works

- `main.c` creates the LVGL display in direct mode on a static
  framebuffer and a pointer input device driven by the script.
- Time is simulated (`lv_tick_set_cb`), so timers such as the 3 s error
  notice are deterministic.
- Each step writes the raw framebuffer (`<name>.bin`), which
  `tools/fb2png.py` converts. The same converter is used for SDRAM
  framebuffer dumps from the board, so simulator and hardware
  screenshots can be compared directly.

The simulator is a preview of layout and behaviour, not a hardware test.
Rendering speed, touch accuracy and colours on the real panel are
REQUIRES HARDWARE.
