#!/usr/bin/env bash
# Regenerate the LVGL fonts used by firmware/ui (Turkish letters + icons).
#
# Sources are the fonts shipped with LVGL (scripts/built_in_font/):
#   Montserrat-Medium.ttf                  SIL Open Font License 1.1
#   FontAwesome5-Solid+Brands+Regular.woff Font Awesome Free (SIL OFL 1.1)
# Requires Node.js; lv_font_conv is fetched by npx at the pinned version.
#
# Usage: tools/gen-ui-fonts.sh
set -euo pipefail

# Paths are relative to the repository root so the "Opts:" line that
# lv_font_conv writes into each file does not depend on the checkout path.
cd "$(dirname "${BASH_SOURCE[0]}")/.."
SRC="firmware/STM32AudioController/Middlewares/Third_Party/lvgl/scripts/built_in_font"
OUT="firmware/ui/fonts"
CONV=(npx --yes lv_font_conv@1.5.3)

# Basic Latin, Latin-1 Supplement, Latin Extended-A (covers Turkish).
TEXT_RANGE="0x20-0x7E,0xA0-0x17F"
# check, warning, chevron-left, chevron-right, plug, hourglass, volume-up,
# volume-mute, sync-alt (display flip)
TEXT_SYMBOLS="0xF00C,0xF071,0xF053,0xF054,0xF1E6,0xF254,0xF028,0xF6A9,0xF2F1"
# music (generic), volume-up (speakers), headphones, headset, desktop
# (display), usb (digital output), plug, hourglass, warning
ICON_SYMBOLS="0xF001,0xF028,0xF025,0xF590,0xF108,0xF287,0xF1E6,0xF254,0xF071"

text_font() { # <size>
    "${CONV[@]}" --bpp 4 --size "$1" --no-compress --format lvgl --lv-include lvgl.h \
        --font "$SRC/Montserrat-Medium.ttf" -r "$TEXT_RANGE" \
        --font "$SRC/FontAwesome5-Solid+Brands+Regular.woff" -r "$TEXT_SYMBOLS" \
        -o "$OUT/ui_font_$1.c"
}

text_font 16
text_font 22
"${CONV[@]}" --bpp 4 --size 40 --no-compress --format lvgl --lv-include lvgl.h \
    --font "$SRC/FontAwesome5-Solid+Brands+Regular.woff" -r "$ICON_SYMBOLS" \
    -o "$OUT/ui_icons_40.c"
