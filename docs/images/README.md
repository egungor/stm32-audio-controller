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
| `sim-m7-edit.png` | Edit mode after a star and a move (light theme, green accent) | branch `feature/60-edit-mode`, `62-edit-moved` (#60) |
| `sim-m7-keyboard.png` | Alias keyboard, Turkish layout, "Oyunı" typed | as above, `64-keyboard-typed` |
| `sim-m7-layout-list.png` | Layout B (list), dark theme | branch `feature/60-layouts`, `70-list` (#60) |
| `sim-m7-layout-tiles.png` | Layout C (large tile), dark theme | as above, `73-tiles` |

The light theme with blue accent and the 3×2 grid were also checked on the
board on 2026-10-15 (`docs/hardware.md`, issue #60).

### Presentation screens

The screens in the product presentation. `firmware/ui` at `main` `c1342cf`
(#73), driven through `ui.h` by a small headless program like the simulator
(same LVGL build, same RGB565 framebuffer), with demo outputs: aliases
"Masaüstü hoparlör", "Oyun kulaklığı" (active), "Monitör", "Kablosuz
kulaklık", three outputs without alias, volume 62 %. Not captured on the
board: REQUIRES HARDWARE.

| File | Screen |
|---|---|
| `sim-pitch-a-dark.png` | Layout A, 2×2 cards, dark theme, orange accent |
| `sim-pitch-a-muted.png` | As above, muted |
| `sim-pitch-a-light-3x2.png` | Layout A, 3×2 cards, light theme, blue accent |
| `sim-pitch-a-english.png` | Layout A in English |
| `sim-pitch-acc-dark-0.png` … `-5.png` | The six accent colours, dark theme |
| `sim-pitch-b-dark.png` | Layout B (list), dark theme |
| `sim-pitch-c-dark.png` | Layout C (large tile), dark theme, purple accent |
| `sim-pitch-edit.png` | Edit mode |
| `sim-pitch-keyboard.png` | Alias keyboard |
| `sim-pitch-settings.png` | Settings page |
| `sim-pitch-waiting.png` | Waiting for the computer |

How close the simulator is to the board: the same method, run on the UI
code of `41bfed5`, gives the three LCD screenshots above pixel for pixel
(0 of 130,560 pixels differ; checked 2026-10-16). That compares the
framebuffer, not the panel.

## Project presentation

The presentation (Turkish, 12 slides) is a claude.ai artifact owned by the
project developer:
<https://claude.ai/artifact/1iZGqWiF55fRbjfhNWXViQ>

Since 2026-10-16 (#67) it is a product presentation: what the controller
does for the user (problem, solution, setup, main screen, three layouts,
colours, aliases and order, volume, details, specifications, status), not
how it is built. Every screen in it is one of the "Presentation screens"
above, labelled on the slides as the device software's own output, not a
photo. The closing slide separates what was verified on the board and on
Windows (M0–M6) from what is still being tested (M7, installer). The owner
can export it as PDF or PowerPoint from the artifact.

A PDF copy for reading without the artifact is in the repo:
[`docs/presentation/stm32-ses-kumandasi.pdf`](../presentation/stm32-ses-kumandasi.pdf)
(12 pages, 1920×1080). It was rendered from the artifact's slide files with
headless Chromium, so small details (line breaks, icon shapes) can differ
from the artifact's own export. The artifact is the source: when it changes,
render or export the PDF again and replace this copy.

When a screen changes, replace the file here first, then the image in the
presentation.
