# Images

Board photos, LCD screenshots and simulator screenshots used by the
documentation and the project presentation live here.

## LCD screenshots of the UI

Exact captures of the LCD: the framebuffer dumped over SWD and converted
with `tools/fb2png.py` (see `docs/toolchain.md`). Prefer these over phone
photos.

| File | Screen | Firmware |
|---|---|---|
| `ui-main-screen.png` | Device list, active device, volume strip | `main` at `41bfed5`, default build (#48) |
| `ui-muted.png` | Same screen, muted | as above |
| `ui-waiting-for-host.png` | No computer connected | as above |

- Use descriptive lowercase names with the `ui-` prefix, e.g.
  `ui-volume.png`.
- Record the firmware commit the screenshot was taken from in the commit
  message and in the table above.

## Simulator screenshots

Output of the UI simulator (`firmware/simulator`, `tools/run-ui-simulator.sh`),
which runs the same `firmware/ui` code and writes the same RGB565
framebuffer format as the board. They show layout, not the panel: colours
and speed on the real LCD are REQUIRES HARDWARE. Prefix `sim-`.

| File | Screen | Source |
|---|---|---|
| `sim-m7-light-blue.png` | Light theme, blue accent, tabs, favorites | `main` at `aa6ebc1`, `23-light-blue` (#60) |
| `sim-m7-dark-green.png` | Dark theme, green accent | as above, `24-dark-green` |
| `sim-m7-grid-3x2.png` | 3×2 cards | as above, `25-grid-3x2` |
| `sim-m7-list.png` | List layout | as above, `27-grid-list` |
| `sim-m7-settings.png` | Settings page, dark theme | branch `feature/60-settings-page`, `40-settings` (#60) |
| `sim-m7-settings-light.png` | Settings page after choosing the light theme and blue accent | as above, `41-settings-light-blue` |

The light theme with blue accent and the 3×2 grid were also checked on the
board on 2026-10-15 (`docs/hardware.md`, issue #60).

## Project presentation

The presentation (Turkish, 15 slides) is a claude.ai artifact owned by the
project developer:
<https://claude.ai/artifact/1iZGqWiF55fRbjfhNWXViQ>

It uses the three LCD screenshots (cover, main screen, other states) and
the four simulator screenshots (M7 slide, labelled as simulator output).
Last updated 2026-10-15 (#67): status, roadmap, hardware, firmware, host
and protocol slides match `main` at `aa6ebc1`. The owner can export it as
PDF or PowerPoint from the artifact.

A PDF copy for reading without the artifact is in the repo:
[`docs/presentation/stm32-ses-kumandasi.pdf`](../presentation/stm32-ses-kumandasi.pdf)
(15 pages, 1920×1080). It was rendered from the artifact's slide files with
headless Chromium, so small details (line breaks, icon shapes) can differ
from the artifact's own export. The artifact is the source: when it changes,
render or export the PDF again and replace this copy.

When a screen changes, replace the file here first, then the image in the
presentation.
