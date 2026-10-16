# Touch UI (`firmware/ui`)

Hardware-independent LVGL user interface. It is compiled into the
firmware (`App/Src/ui_port.c`, `App/Src/ui_demo.c`) and into the Linux simulator
(`firmware/simulator`).

## Boundary

- Depends only on LVGL (`lvgl.h`). No HAL, CMSIS, FreeRTOS, USB,
  protocol framing or Windows concepts.
- Input: the state reported by the host, through `Ui_SetDevices`,
  `Ui_SetActive`, `Ui_SetVolume`, `Ui_SetMuted`, `Ui_ShowWaiting`,
  `Ui_ShowError`.
- Output: the `UiSelectCallback` when the user taps a device. The caller
  sends `SELECT_DEVICE` and calls `Ui_SetPending`; the button turns
  active only when the host confirms with `DEVICE_CHANGED`
  (`protocol/protocol.md`, "Exchanges").
- Volume and mute (bottom strip; handlers set with `Ui_SetAudioHandlers`):
  `UiVolumeCallback` while the slider is dragged and once on release;
  `UiMuteCallback` with the state wanted. The UI does not change the mute
  icon itself; it shows what `Ui_SetMuted` reports. `UI_VOLUME_UNKNOWN`
  disables the controls. Rate limiting is the caller's job
  (`firmware/link`).
- Display flip: a button in the top right corner of both screens; held
  for `UI_FLIP_HOLD_MS` (icon turns orange), it calls the
  `UiFlipCallback` (`Ui_SetFlipHandler`) when the finger lifts. The UI
  does not rotate anything: the display port turns the image and mirrors
  touch (`App/Inc/ui_port.h`).
- `UiDevice` mirrors the protocol's device entry (id, icon, UTF-8 name of
  up to 48 bytes, alias of up to 32 bytes shown instead of the name,
  favorite flag); `UiIcon` values equal `ProtoIcon`.
- Settings (`UiSettings`, same fields and values as the protocol's
  settings record, ADR-004): `Ui_ApplySettings` applies theme (dark/light),
  accent colour (6), language (Turkish/English) and the card grid (2x2,
  3x2, 3x3, list); a change of one of them rebuilds the screens. The first
  settings after `Ui_Init` choose the startup tab. The UI reports changes
  it makes itself (the tab, as `last_tab`, and everything on the settings
  page) with the whole record through the `UiSettingsCallback`
  (`Ui_SetSettingsHandler`). `flipped` and `screen_timeout` are for the
  display port, the volume limit for the host.
- Settings page (`ui_settings.c`, #60, design A of #57): the cog button in
  the main screen's top bar opens it; the left half of its bar goes back.
  A scrolling list of rows: layout, theme, accent colour (6 swatches), grid,
  startup tab, screen timeout (never / 1 / 5 / 15 min), volume limit
  on switch (switch; when on, a slider in 5 % steps, reported on release),
  flip (switch) and language. A tap applies the change at once and reports
  the record; a change of theme, accent, grid or language rebuilds the
  screens on the next timer run (`ui_request_rebuild`, so the tapped
  control is not deleted inside its own event) and the page stays open at
  the same scroll position. Segmented controls react to `CLICKED`, which
  LVGL does not send after a drag that scrolled, so scrolling over a
  control changes nothing. A new device list or settings from the host
  leave the page open; the waiting screen closes it. "Layout" chooses A, B
  or C; the grid row is greyed out (and ignores taps) outside layout A.
- Layouts (`UiSettings.layout`, #60, designs of #57): A cards (`ui.c`,
  tabs in the top bar, volume and pages in the bottom strip, grid
  2x2 / 3x2 / 3x3 / list); B list (`ui_layouts.c`: tab rail on the left
  with all / favorites / edit / settings, five rows with the Windows name
  under an alias and a check mark on the active one, vertical volume on
  the right); C large tile (`ui_layouts.c`: the active device, its volume
  and mute in a panel on the left, the other devices as six tiles, tabs as
  pills). All three fill the same objects (device buttons, tabs, pager,
  slider, mute) so `ui.c` renders them; they differ in `CardKind`. The
  mockups scroll B and show page dots in C; both use the page buttons of A
  here. B and C have no flip button on the main screen: "Ekranı döndür"
  on the settings page does it (the waiting screen keeps its button).
- Edit mode (`ui_edit.c`, #60, design A of #57): the pencil button in
  the top bar (shown once `Ui_SetEditHandlers` is called) opens every
  device, in the user's order, as cards of four per page. Each card has
  four buttons: move one place left, move one place right, favorite (in
  the accent colour when set) and alias. They call `UiEditHandlers`; the
  cards change when the host's new list arrives, as the protocol says. A
  handler that returns false (edit not sent) leaves the tap without
  effect. The alias keyboard is an `lv_keyboard` with a Turkish Q layout
  (ç ğ ı İ ö ş ü; "ABC" / "abc" / "1#" switch modes) and a one-line text
  area that refuses input past 32 bytes of UTF-8; ✓ or "Kaydet" saves
  ("" removes the alias), × closes without saving. A card is three LVGL
  objects (label, `lv_buttonmatrix`, box) with shared styles: about
  2.4 KB, so the edit mode fits next to the main screen. Both screens
  exist only while shown; a rebuild keeps them open and keeps the typed
  text.
- Screen timeout: a 1 s LVGL timer compares LVGL's time since the last
  touch (`lv_display_get_inactive_time`) with `screen_timeout` minutes.
  When it runs out a black, clickable object on the system layer covers
  everything and the `UiBacklightCallback` (`Ui_SetBacklightHandler`) is
  told to switch the backlight off. The next press lands on that object,
  so it only wakes the screen (backlight on) and never reaches a control.
  A new timeout from the host counts from then; "never" wakes a dark
  screen.
- Tabs: "All" and "Favorites" (devices with the favorite flag), in the
  host's order.
- Memory: the device list is a fixed array of 32 entries; LVGL objects are
  created in `Ui_Init` and reused when the list or page changes. A rebuild
  (layout, theme, accent, language, grid) deletes the old screens before
  building the new ones, so the LVGL heap never holds two sets. The
  settings page (about 15 KB), the edit mode (about 10 KB) and the alias
  keyboard (about 6 KB) exist only while shown. The heap is 128 KB
  (`LV_MEM_SIZE`, raised from 64 KB in #60): with the densest main screen,
  the settings page and LVGL's draw buffers, 64 KB ran out in the
  simulator. Simulator (`Ui_MemoryUse`): 30–36 KB in use on the main
  screen in any layout, 47–51 KB with the settings page, 47 KB with the
  edit mode and keyboard; peak 53 KB (41 %). No growth over repeated
  rebuilds, layout changes, or opening and closing these screens. Avoid
  `opa` below 100 % on an object: LVGL then draws it through a layer
  buffer from this heap (up to 24 KB); use `bg_opa` / `text_opa`.
- Text: `ui_text.c`/`ui_text.h` are generated by `tools/gen-ui-text.py`
  (Turkish and English, UTF-8 as escapes); colours are in `ui_theme.c`.

## Files

| File | Content |
|---|---|
| `ui.h`, `ui.c` | Screens and API |
| `ui_settings.c` | Settings page |
| `ui_edit.c` | Edit mode (order, favorites) and alias keyboard |
| `ui_layouts.c` | Main screens of layouts B (list) and C (large tile) |
| `ui_internal.h` | State and helpers shared by the UI files |
| `ui_theme.c` | Dark and light palettes, accent colours |
| `ui_text.c`, `ui_text.h` | UI text, Turkish and English (generated) |
| `lv_conf.h` | LVGL v9.5.0 configuration, shared by firmware and simulator |
| `fonts/` | Generated fonts: Montserrat 14/16/22 px with Turkish letters and Font Awesome icons, 40 px device icons (`tools/gen-ui-fonts.sh`) |

Fonts are generated by `tools/gen-ui-fonts.sh` (`lv_font_conv` 1.5.3,
fonts shipped with LVGL: Montserrat and Font Awesome Free, both SIL Open
Font License 1.1). Edit the script, not the generated `.c` files.

UI text (`TXT_*`) is generated into `ui_text.c` by `tools/gen-ui-text.py`,
in Turkish and English.

## LVGL

LVGL is a git submodule at
`firmware/STM32AudioController/Middlewares/Third_Party/lvgl`, pinned to
tag v9.5.0 (MIT licence). After cloning:

```text
git submodule update --init
```
