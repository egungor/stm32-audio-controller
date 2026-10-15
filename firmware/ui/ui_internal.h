/**
 * @file    ui_internal.h
 * @brief   State and helpers shared by the files of the UI module. Not part
 *          of the API (ui.h).
 */
#ifndef UI_INTERNAL_H
#define UI_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"
#include "ui.h"
#include "ui_text.h"

/* Layout for the 480 x 272 panel. */
#define UI_WIDTH     480
#define UI_HEIGHT    272
#define BAR_H        36
#define BOTTOM_H     40
#define PAD          8
#define BTN_RADIUS   12
#define FLIP_W       44
#define TAB_W        116

/* Font Awesome glyphs in ui_font_14/16/22 (tools/gen-ui-fonts.sh). */
#define SYM_CHECK      "\xEF\x80\x8C"
#define SYM_WARNING    "\xEF\x81\xB1"
#define SYM_PREV       "\xEF\x81\x93"
#define SYM_NEXT       "\xEF\x81\x94"
#define SYM_PLUG       "\xEF\x87\xA6"
#define SYM_HOURGLASS  "\xEF\x89\x94"
#define SYM_VOLUME     "\xEF\x80\xA8" /* volume-up */
#define SYM_MUTED      "\xEF\x9A\xA9" /* volume-mute */
#define SYM_FLIP       "\xEF\x8B\xB1" /* sync-alt */
#define SYM_STAR       "\xEF\x80\x85"
#define SYM_COG        "\xEF\x80\x93"
#define SYM_PENCIL     "\xEF\x8C\x83" /* pencil-alt */
#define SYM_LIST       "\xEF\x80\xBA"
#define SYM_GRID       "\xEF\x80\x89" /* th-large */
#define SYM_UP         "\xEF\x81\xA2" /* arrow-up */
#define SYM_DOWN       "\xEF\x81\xA3" /* arrow-down */
#define SYM_BACKSPACE  "\xEF\x95\x9A"
#define SYM_CLOSE      "\xEF\x80\x8D" /* times */

LV_FONT_DECLARE(ui_font_14)
LV_FONT_DECLARE(ui_font_16)
LV_FONT_DECLARE(ui_font_22)
LV_FONT_DECLARE(ui_icons_40)

/** Colours of the current theme and accent (ui_theme.c). */
typedef struct
{
    uint32_t bg;       /**< screen */
    uint32_t bar;      /**< bars and panels */
    uint32_t btn;      /**< cards and buttons */
    uint32_t line;     /**< card outline (light theme) */
    uint32_t track;    /**< slider track */
    uint32_t accent;   /**< active device, selection, knob */
    uint32_t on_acc;   /**< text on the accent */
    uint32_t text;
    uint32_t muted;    /**< secondary text, icons */
    uint32_t link;     /**< connection state */
    bool     dark;
} UiPalette;

/** One device card or list row. */
typedef struct
{
    lv_obj_t *btn;
    lv_obj_t *icon;
    lv_obj_t *name;
    lv_obj_t *state;  /**< "Active output" / "Switching..."; NULL if none */
    lv_obj_t *star;   /**< favorite mark; NULL if none */
} UiDeviceButton;

typedef struct
{
    /* Handlers. */
    UiSelectCallback   on_select;
    void              *user;
    UiVolumeCallback   on_volume;
    UiMuteCallback     on_mute;
    void              *audio_user;
    UiFlipCallback     on_flip;
    void              *flip_user;
    UiSettingsCallback on_settings;
    void              *settings_user;

    /* State reported by the host. */
    UiDevice devices[UI_MAX_DEVICES];
    uint32_t count;
    uint8_t  active;
    uint8_t  pending;
    uint8_t  volume;  /**< 0-100 or UI_VOLUME_UNKNOWN */
    uint8_t  muted;   /**< 0, 1 or UI_VOLUME_UNKNOWN */

    /* Settings and view. */
    UiSettings settings;
    bool       settings_seen;      /**< Ui_ApplySettings called since Ui_Init */
    UiPalette  pal;
    uint32_t   tab;                /**< UI_TAB_* */
    uint32_t   page;
    uint8_t    view[UI_MAX_DEVICES]; /**< device indices shown in the tab */
    uint32_t   view_count;

    /* Flip button. */
    uint32_t flip_pressed_at;  /**< lv_tick_get() at press */
    bool     flip_armed;       /**< held long enough; flip on release */

    /* Objects (rebuilt when the appearance changes). */
    lv_obj_t      *waiting_screen;
    lv_obj_t      *main_screen;
    UiDeviceButton buttons[UI_MAX_PER_PAGE];
    uint32_t       per_page;
    lv_obj_t      *tab_btns[2];
    lv_obj_t      *tab_labels[2];
    lv_obj_t      *empty_label;
    lv_obj_t      *pager;
    lv_obj_t      *page_label;
    lv_obj_t      *mute_btn;
    lv_obj_t      *mute_icon;
    lv_obj_t      *slider;
    lv_obj_t      *volume_label;
    lv_obj_t      *prev_btn;
    lv_obj_t      *next_btn;
    lv_obj_t      *flip_icons[2];  /**< waiting screen, main screen */
    lv_obj_t      *notice;
    lv_obj_t      *notice_label;
    lv_timer_t    *notice_timer;   /**< created once */

    /* Settings page (ui_settings.c), rebuilt with the other screens. */
    lv_obj_t *settings_screen;
    lv_obj_t *settings_list;       /**< the scrolling list of rows */
    int32_t   settings_scroll;     /**< list scroll position kept over a rebuild */
    lv_obj_t *set_theme;           /**< segmented controls (lv_buttonmatrix) */
    lv_obj_t *set_grid;
    lv_obj_t *set_startup;
    lv_obj_t *set_timeout;
    lv_obj_t *set_language;
    lv_obj_t *set_swatches[UI_ACCENT_COUNT];
    lv_obj_t *set_limit_switch;
    lv_obj_t *set_limit_box;       /**< hint and slider, hidden while the limit is off */
    lv_obj_t *set_limit_slider;
    lv_obj_t *set_limit_label;
    lv_obj_t *set_flip_switch;
    bool      rebuild_queued;      /**< ui_request_rebuild() pending */
} UiState;

extern UiState g_ui;

/** Fill g_ui.pal from g_ui.settings (theme, accent). */
void ui_theme_update(void);

/** Accent colour @p accent on the current theme (for the colour swatches). */
uint32_t ui_theme_accent(uint8_t accent);

/* Building blocks shared by the screens (ui.c). */
lv_obj_t *ui_make_screen(void);
lv_obj_t *ui_make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text);
lv_obj_t *ui_make_box(lv_obj_t *parent);
void      ui_style_card(lv_obj_t *obj, int32_t radius);

/** Report g_ui.settings to the settings handler. */
void ui_notify_settings(void);

/** Rebuild every screen with the current settings on the next LVGL timer
 *  run: a control must not delete itself from its own event handler. */
void ui_request_rebuild(void);

/** Show the main screen / the settings page. */
void ui_show_main(void);
void ui_show_settings(void);

/* Settings page (ui_settings.c). */
void ui_settings_build(void);   /**< create g_ui.settings_screen */
void ui_settings_render(void);  /**< show g_ui.settings in the controls */

/** lv_color_t of a palette value. */
static inline lv_color_t ui_color(uint32_t rgb)
{
    return lv_color_hex(rgb);
}

#endif /* UI_INTERNAL_H */
