/**
 * @file    ui_theme.c
 * @brief   Colours of the dark and light themes and the accent colours
 *          (designs: issue #57).
 */
#include "ui_internal.h"

typedef struct
{
    uint32_t dark;   /**< on the dark theme */
    uint32_t light;  /**< on the light theme (a little darker) */
} AccentPair;

/* UI_ACCENT_* order: orange (default), blue, green, purple, pink, yellow.
 * Text on the accent stays dark (#14202B) in every colour. */
static const AccentPair k_accents[UI_ACCENT_COUNT] = {
    { 0xF0935AU, 0xE8833FU },
    { 0x6FA8F0U, 0x4A8BDBU },
    { 0x5CC48AU, 0x34A86AU },
    { 0xB08CF0U, 0x9672DEU },
    { 0xF07FA8U, 0xE0628EU },
    { 0xF2C14EU, 0xE0AA2AU },
};

static const UiPalette k_dark = {
    0x0E171FU, /* bg */
    0x14202BU, /* bar */
    0x22313FU, /* btn */
    0x2A3A49U, /* line */
    0x22313FU, /* track */
    0U,        /* accent: set below */
    0x14202BU, /* on_acc */
    0xF4F2EDU, /* text */
    0xB9C4CEU, /* muted */
    0x8FB8E8U, /* link */
    true,
};

static const UiPalette k_light = {
    0xEEF1F4U, /* bg */
    0xFFFFFFU, /* bar */
    0xFFFFFFU, /* btn */
    0xD3DBE2U, /* line */
    0xCDD6DEU, /* track */
    0U,        /* accent: set below */
    0x14202BU, /* on_acc */
    0x16212BU, /* text */
    0x5C6B78U, /* muted */
    0x2F6DB5U, /* link */
    false,
};

void ui_theme_update(void)
{
    const uint8_t accent = (g_ui.settings.accent < UI_ACCENT_COUNT) ? g_ui.settings.accent : 0U;
    const bool light = (g_ui.settings.theme == UI_THEME_LIGHT);

    g_ui.pal = light ? k_light : k_dark;
    g_ui.pal.accent = light ? k_accents[accent].light : k_accents[accent].dark;
}
