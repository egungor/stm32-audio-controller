/**
 * @file    ui.c
 * @brief   Touch UI of the audio controller (LVGL, hardware independent).
 */
#include "ui.h"

#include <string.h>

#include "lvgl.h"

/* Layout for the 480 x 272 panel. */
#define BAR_H        36
#define BOTTOM_H     40
#define PAD          8
#define BTN_RADIUS   12
#define FLIP_W       44

/* Colours, matching the project presentation. */
#define COLOR_BG      0x0E171F
#define COLOR_BAR     0x14202B
#define COLOR_BTN     0x22313F
#define COLOR_ACCENT  0xF0935A
#define COLOR_ON_ACC  0x14202B
#define COLOR_TEXT    0xF4F2ED
#define COLOR_MUTED   0xB9C4CE
#define COLOR_LINK    0x8FB8E8

/* Font Awesome glyphs included in ui_font_* and ui_icons_40
 * (tools/gen-ui-fonts.sh). */
#define SYM_CHECK      "\xEF\x80\x8C"
#define SYM_WARNING    "\xEF\x81\xB1"
#define SYM_PREV       "\xEF\x81\x93"
#define SYM_NEXT       "\xEF\x81\x94"
#define SYM_PLUG       "\xEF\x87\xA6"
#define SYM_HOURGLASS  "\xEF\x89\x94"
#define SYM_VOLUME     "\xEF\x80\xA8" /* volume-up */
#define SYM_MUTED      "\xEF\x9A\xA9" /* volume-mute */
#define SYM_FLIP       "\xEF\x8B\xB1" /* sync-alt */

/* UI text (Turkish). */
#define TXT_TITLE        "SES \xC3\x87IKI\xC5\x9EI"               /* SES ÇIKIŞI */
#define TXT_CONNECTED    "Bilgisayar ba\xC4\x9Fl\xC4\xB1"           /* Bilgisayar bağlı */
#define TXT_WAITING      "Bilgisayar bekleniyor"
#define TXT_WAITING_HINT "USB kablosunu ve uygulamay\xC4\xB1 kontrol edin"
#define TXT_ACTIVE       SYM_CHECK " Etkin \xC3\xA7\xC4\xB1k\xC4\xB1\xC5\x9F" /* Etkin çıkış */
#define TXT_PENDING      "Ge\xC3\xA7iliyor..."                     /* Geçiliyor... */
#define TXT_EMPTY        "Ses \xC3\xA7\xC4\xB1k\xC4\xB1\xC5\x9F\xC4\xB1 bulunamad\xC4\xB1"
/* Ses çıkışı değiştirilemedi */
#define TXT_SELECT_FAILED "Ses \xC3\xA7\xC4\xB1k\xC4\xB1\xC5\x9F\xC4\xB1 de\xC4\x9Fi\xC5\x9Ftirilemedi"
/* Bilgisayar yanıt vermiyor */
#define TXT_HOST_TIMEOUT  "Bilgisayar yan\xC4\xB1t vermiyor"

LV_FONT_DECLARE(ui_font_16)
LV_FONT_DECLARE(ui_font_22)
LV_FONT_DECLARE(ui_icons_40)

typedef struct
{
    lv_obj_t *btn;
    lv_obj_t *icon;
    lv_obj_t *name;
    lv_obj_t *state;
} DeviceButton;

static struct
{
    UiSelectCallback on_select;
    void            *user;
    UiVolumeCallback on_volume;
    UiMuteCallback   on_mute;
    void            *audio_user;
    UiFlipCallback   on_flip;
    void            *flip_user;
    uint32_t         flip_pressed_at;  /* lv_tick_get() at press */
    bool             flip_armed;       /* held long enough; flip on release */
    uint8_t          volume;  /* 0-100 or UI_VOLUME_UNKNOWN */
    uint8_t          muted;   /* 0, 1 or UI_VOLUME_UNKNOWN */

    UiDevice devices[UI_MAX_DEVICES];
    uint32_t count;
    uint32_t page;
    uint8_t  active;
    uint8_t  pending;

    lv_obj_t    *waiting_screen;
    lv_obj_t    *main_screen;
    DeviceButton buttons[UI_DEVICES_PER_PAGE];
    lv_obj_t    *empty_label;
    lv_obj_t    *pager;
    lv_obj_t    *page_label;
    lv_obj_t    *mute_btn;
    lv_obj_t    *mute_icon;
    lv_obj_t    *slider;
    lv_obj_t    *volume_label;
    lv_obj_t    *prev_btn;
    lv_obj_t    *next_btn;
    lv_obj_t    *flip_icons[2];  /* waiting screen, main screen */
    lv_obj_t    *notice;
    lv_obj_t    *notice_label;
    lv_timer_t  *notice_timer;
} s_ui;

static const char *icon_symbol(uint8_t icon)
{
    static const char *const symbols[] = {
        "\xEF\x80\x81", /* generic: music */
        "\xEF\x80\xA8", /* speakers: volume-up */
        "\xEF\x80\xA5", /* headphones */
        "\xEF\x96\x90", /* headset */
        "\xEF\x84\x88", /* display: desktop */
        "\xEF\x8A\x87", /* digital output: usb */
    };

    return (icon < sizeof(symbols) / sizeof(symbols[0])) ? symbols[icon] : symbols[0];
}

static uint32_t page_count(void)
{
    return (s_ui.count + UI_DEVICES_PER_PAGE - 1U) / UI_DEVICES_PER_PAGE;
}

static lv_obj_t *make_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_font(scr, &ui_font_16, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_text(label, text);
    return label;
}

/* ---------------------------------------------------------------- render */

static void render_button(uint32_t slot)
{
    DeviceButton *b = &s_ui.buttons[slot];
    const uint32_t index = (s_ui.page * UI_DEVICES_PER_PAGE) + slot;
    const UiDevice *d;
    bool active;
    bool pending;
    uint32_t fg;

    if (index >= s_ui.count)
    {
        lv_obj_add_flag(b->btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    d = &s_ui.devices[index];
    active = (d->id == s_ui.active);
    pending = !active && (d->id == s_ui.pending);
    fg = active ? COLOR_ON_ACC : COLOR_TEXT;

    lv_obj_remove_flag(b->btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(b->btn, lv_color_hex(active ? COLOR_ACCENT : COLOR_BTN), 0);
    lv_obj_set_style_border_width(b->btn, pending ? 2 : 0, 0);
    lv_label_set_text_static(b->icon, icon_symbol(d->icon));
    lv_obj_set_style_text_color(b->icon, lv_color_hex(active ? COLOR_ON_ACC : COLOR_MUTED), 0);
    lv_label_set_text(b->name, d->name);
    lv_obj_set_style_text_color(b->name, lv_color_hex(fg), 0);

    if (active || pending)
    {
        lv_label_set_text_static(b->state, active ? TXT_ACTIVE : TXT_PENDING);
        lv_obj_set_style_text_color(b->state, lv_color_hex(active ? COLOR_ON_ACC : COLOR_ACCENT), 0);
        lv_obj_remove_flag(b->state, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(b->state, LV_OBJ_FLAG_HIDDEN);
    }
}

static void render_main(void)
{
    const uint32_t pages = page_count();

    for (uint32_t slot = 0U; slot < UI_DEVICES_PER_PAGE; slot++)
    {
        render_button(slot);
    }

    if (s_ui.count == 0U)
    {
        lv_obj_remove_flag(s_ui.empty_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(s_ui.empty_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (pages > 1U)
    {
        lv_label_set_text_fmt(s_ui.page_label, "%u/%u", (unsigned)(s_ui.page + 1U), (unsigned)pages);
        lv_obj_remove_flag(s_ui.pager, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_state(s_ui.prev_btn, LV_STATE_DISABLED, s_ui.page == 0U);
        lv_obj_set_state(s_ui.next_btn, LV_STATE_DISABLED, s_ui.page + 1U >= pages);
    }
    else
    {
        lv_obj_add_flag(s_ui.pager, LV_OBJ_FLAG_HIDDEN);
    }
}

static void render_audio(void)
{
    const bool known = (s_ui.volume != UI_VOLUME_UNKNOWN);
    const bool muted = (s_ui.muted == 1U);

    lv_obj_set_state(s_ui.slider, LV_STATE_DISABLED, !known);
    if (known && !lv_obj_has_state(s_ui.slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(s_ui.slider, s_ui.volume, LV_ANIM_OFF);
    }
    if (known)
    {
        lv_label_set_text_fmt(s_ui.volume_label, "%u%%", (unsigned)lv_slider_get_value(s_ui.slider));
    }
    else
    {
        lv_label_set_text_static(s_ui.volume_label, "--");
    }
    /* Muted: grey fill. Unknown: empty track, no knob. */
    lv_obj_set_style_bg_color(s_ui.slider, lv_color_hex(muted ? COLOR_MUTED : COLOR_ACCENT), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_ui.slider, lv_color_hex(muted ? COLOR_MUTED : COLOR_ACCENT), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(s_ui.slider, known ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ui.slider, known ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_KNOB);

    lv_obj_set_state(s_ui.mute_btn, LV_STATE_DISABLED, s_ui.muted == UI_VOLUME_UNKNOWN);
    lv_label_set_text_static(s_ui.mute_icon, muted ? SYM_MUTED : SYM_VOLUME);
    lv_obj_set_style_bg_color(s_ui.mute_btn, lv_color_hex(muted ? COLOR_ACCENT : COLOR_BTN), 0);
    lv_obj_set_style_text_color(s_ui.mute_icon, lv_color_hex(muted ? COLOR_ON_ACC : COLOR_TEXT), 0);
}

/* ---------------------------------------------------------------- events */

static void on_slider_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const uint8_t value = (uint8_t)lv_slider_get_value(s_ui.slider);

    if ((code != LV_EVENT_VALUE_CHANGED) && (code != LV_EVENT_RELEASED))
    {
        return;
    }
    lv_label_set_text_fmt(s_ui.volume_label, "%u%%", (unsigned)value);
    if (s_ui.on_volume != NULL)
    {
        s_ui.on_volume(value, code == LV_EVENT_RELEASED, s_ui.audio_user);
    }
}

static void on_mute_clicked(lv_event_t *e)
{
    LV_UNUSED(e);
    if ((s_ui.on_mute != NULL) && (s_ui.muted != UI_VOLUME_UNKNOWN))
    {
        s_ui.on_mute(s_ui.muted == 0U, s_ui.audio_user);
    }
}

static void on_device_clicked(lv_event_t *e)
{
    const uint32_t slot = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    const uint32_t index = (s_ui.page * UI_DEVICES_PER_PAGE) + slot;
    uint8_t id;

    if (index >= s_ui.count)
    {
        return;
    }
    id = s_ui.devices[index].id;
    if ((id != s_ui.active) && (s_ui.on_select != NULL))
    {
        s_ui.on_select(id, s_ui.user);
    }
}

static void on_page_clicked(lv_event_t *e)
{
    const bool next = lv_event_get_user_data(e) != NULL;
    const uint32_t pages = page_count();

    if (next && (s_ui.page + 1U < pages))
    {
        s_ui.page++;
    }
    else if (!next && (s_ui.page > 0U))
    {
        s_ui.page--;
    }
    render_main();
}

static void on_notice_timer(lv_timer_t *timer)
{
    lv_timer_pause(timer);
    lv_obj_add_flag(s_ui.notice, LV_OBJ_FLAG_HIDDEN);
}

static void set_flip_armed(bool armed)
{
    s_ui.flip_armed = armed;
    for (uint32_t i = 0U; i < 2U; i++)
    {
        lv_obj_set_style_text_color(s_ui.flip_icons[i],
                                    lv_color_hex(armed ? COLOR_ACCENT : COLOR_MUTED), 0);
    }
}

/* A long press arms the flip (the icon turns orange); it happens when the
 * finger lifts, so the rest of the press is not read in the new
 * orientation. Sliding off the button cancels. */
static void on_flip_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED)
    {
        s_ui.flip_pressed_at = lv_tick_get();
    }
    else if (code == LV_EVENT_PRESSING)
    {
        if (!s_ui.flip_armed && (lv_tick_elaps(s_ui.flip_pressed_at) >= UI_FLIP_HOLD_MS))
        {
            set_flip_armed(true);
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        const bool armed = s_ui.flip_armed;

        set_flip_armed(false);
        if (armed && (s_ui.on_flip != NULL))
        {
            s_ui.on_flip(s_ui.flip_user);
        }
    }
    else if (code == LV_EVENT_PRESS_LOST)
    {
        set_flip_armed(false);
    }
    else
    {
        /* other events: nothing to do */
    }
}

/* ----------------------------------------------------------------- build */

/* Flip button in the top right corner of a screen, over the status bar
 * area; the touch area stops above the device buttons. */
static void build_flip_button(lv_obj_t *scr, uint32_t index)
{
    lv_obj_t *btn = lv_obj_create(scr);

    lv_obj_remove_style_all(btn);
    lv_obj_set_size(btn, FLIP_W, BAR_H);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    s_ui.flip_icons[index] = make_label(btn, &ui_font_16, COLOR_MUTED, SYM_FLIP);
    lv_obj_center(s_ui.flip_icons[index]);
    lv_obj_add_event_cb(btn, on_flip_event, LV_EVENT_ALL, NULL);
}

static void build_waiting_screen(void)
{
    lv_obj_t *box;

    s_ui.waiting_screen = make_screen();
    box = lv_obj_create(s_ui.waiting_screen);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_center(box);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 10, 0);

    (void)make_label(box, &ui_icons_40, COLOR_LINK, SYM_HOURGLASS);
    (void)make_label(box, &ui_font_22, COLOR_TEXT, TXT_WAITING);
    (void)make_label(box, &ui_font_16, COLOR_MUTED, TXT_WAITING_HINT);
    build_flip_button(s_ui.waiting_screen, 0U);
}

static void build_status_bar(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_t *title;
    lv_obj_t *status;

    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, LV_PCT(100), BAR_H);
    lv_obj_set_style_bg_color(bar, lv_color_hex(COLOR_BAR), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(bar, 12, 0);

    title = make_label(bar, &ui_font_16, COLOR_TEXT, TXT_TITLE);
    lv_obj_set_style_text_letter_space(title, 2, 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    status = make_label(bar, &ui_font_16, COLOR_LINK, SYM_PLUG "  " TXT_CONNECTED);
    /* Room for the flip button on the right. */
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, -(FLIP_W - 12), 0);
}

static void build_device_button(lv_obj_t *grid, uint32_t slot)
{
    DeviceButton *b = &s_ui.buttons[slot];
    lv_obj_t *text;

    b->btn = lv_button_create(grid);
    lv_obj_set_grid_cell(b->btn, LV_GRID_ALIGN_STRETCH, (int32_t)(slot % 2U), 1,
                         LV_GRID_ALIGN_STRETCH, (int32_t)(slot / 2U), 1);
    lv_obj_set_style_radius(b->btn, BTN_RADIUS, 0);
    lv_obj_set_style_shadow_width(b->btn, 0, 0);
    lv_obj_set_style_border_color(b->btn, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_pad_all(b->btn, 10, 0);
    lv_obj_set_style_pad_column(b->btn, 12, 0);
    lv_obj_set_flex_flow(b->btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b->btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(b->btn, on_device_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)slot);

    b->icon = make_label(b->btn, &ui_icons_40, COLOR_MUTED, "");
    lv_obj_set_width(b->icon, 48);
    lv_obj_set_style_text_align(b->icon, LV_TEXT_ALIGN_CENTER, 0);

    text = lv_obj_create(b->btn);
    lv_obj_remove_style_all(text);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_height(text, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 2, 0);
    lv_obj_remove_flag(text, LV_OBJ_FLAG_CLICKABLE);

    b->name = make_label(text, &ui_font_22, COLOR_TEXT, "");
    lv_obj_set_width(b->name, LV_PCT(100));
    lv_obj_set_height(b->name, 2 * lv_font_get_line_height(&ui_font_22));
    lv_label_set_long_mode(b->name, LV_LABEL_LONG_MODE_DOTS);

    b->state = make_label(text, &ui_font_16, COLOR_ON_ACC, "");
}

static lv_obj_t *make_small_button(lv_obj_t *parent, int32_t w, const char *symbol, lv_obj_t **label)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *l;

    lv_obj_set_size(btn, w, BOTTOM_H - 8);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_BTN), 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    l = make_label(btn, &ui_font_16, COLOR_TEXT, symbol);
    lv_obj_center(l);
    if (label != NULL)
    {
        *label = l;
    }
    return btn;
}

/* Bottom strip: mute button, volume slider and percentage; page arrows on
 * the right when there is more than one page. */
static void build_bottom_strip(lv_obj_t *scr)
{
    lv_obj_t *strip = lv_obj_create(scr);

    lv_obj_remove_style_all(strip);
    lv_obj_set_size(strip, LV_PCT(100), BOTTOM_H);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_hor(strip, PAD, 0);
    lv_obj_set_style_pad_column(strip, 12, 0);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

    s_ui.mute_btn = make_small_button(strip, 52, SYM_VOLUME, &s_ui.mute_icon);
    lv_obj_add_event_cb(s_ui.mute_btn, on_mute_clicked, LV_EVENT_CLICKED, NULL);

    s_ui.slider = lv_slider_create(strip);
    lv_slider_set_range(s_ui.slider, 0, 100);
    lv_obj_set_flex_grow(s_ui.slider, 1);
    lv_obj_set_height(s_ui.slider, 8);
    lv_obj_set_ext_click_area(s_ui.slider, 14);
    lv_obj_set_style_bg_color(s_ui.slider, lv_color_hex(COLOR_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ui.slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_ui.slider, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(s_ui.slider, on_slider_event, LV_EVENT_ALL, NULL);

    s_ui.volume_label = make_label(strip, &ui_font_16, COLOR_TEXT, "--");
    lv_obj_set_width(s_ui.volume_label, 44);
    lv_obj_set_style_text_align(s_ui.volume_label, LV_TEXT_ALIGN_LEFT, 0);

    s_ui.pager = lv_obj_create(strip);
    lv_obj_remove_style_all(s_ui.pager);
    lv_obj_set_size(s_ui.pager, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(s_ui.pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_ui.pager, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_ui.pager, 8, 0);
    lv_obj_set_style_pad_left(s_ui.pager, 4, 0);

    s_ui.prev_btn = make_small_button(s_ui.pager, 44, SYM_PREV, NULL);
    s_ui.page_label = make_label(s_ui.pager, &ui_font_16, COLOR_MUTED, "");
    s_ui.next_btn = make_small_button(s_ui.pager, 44, SYM_NEXT, NULL);
    lv_obj_add_event_cb(s_ui.prev_btn, on_page_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_ui.next_btn, on_page_clicked, LV_EVENT_CLICKED, (void *)s_ui.next_btn);
}

static void build_main_screen(void)
{
    static const int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t *grid;

    s_ui.main_screen = make_screen();
    build_status_bar(s_ui.main_screen);
    build_flip_button(s_ui.main_screen, 1U);

    grid = lv_obj_create(s_ui.main_screen);
    lv_obj_remove_style_all(grid);
    lv_obj_set_pos(grid, 0, BAR_H);
    lv_obj_set_size(grid, LV_PCT(100), 272 - BAR_H - BOTTOM_H);
    lv_obj_set_style_pad_all(grid, PAD, 0);
    lv_obj_set_style_pad_gap(grid, PAD, 0);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    for (uint32_t slot = 0U; slot < UI_DEVICES_PER_PAGE; slot++)
    {
        build_device_button(grid, slot);
    }

    s_ui.empty_label = make_label(s_ui.main_screen, &ui_font_22, COLOR_MUTED, TXT_EMPTY);
    lv_obj_center(s_ui.empty_label);

    build_bottom_strip(s_ui.main_screen);
}

static void build_notice(void)
{
    lv_obj_t *icon;

    s_ui.notice = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_ui.notice, 440, LV_SIZE_CONTENT);
    lv_obj_align(s_ui.notice, LV_ALIGN_BOTTOM_MID, 0, -PAD);
    lv_obj_set_style_bg_color(s_ui.notice, lv_color_hex(COLOR_BAR), 0);
    lv_obj_set_style_border_color(s_ui.notice, lv_color_hex(COLOR_ACCENT), 0);
    lv_obj_set_style_border_width(s_ui.notice, 2, 0);
    lv_obj_set_style_radius(s_ui.notice, BTN_RADIUS, 0);
    lv_obj_set_style_pad_all(s_ui.notice, 10, 0);
    lv_obj_set_style_pad_column(s_ui.notice, 10, 0);
    lv_obj_set_flex_flow(s_ui.notice, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_ui.notice, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(s_ui.notice, LV_OBJ_FLAG_SCROLLABLE);

    icon = make_label(s_ui.notice, &ui_font_22, COLOR_ACCENT, SYM_WARNING);
    LV_UNUSED(icon);
    s_ui.notice_label = make_label(s_ui.notice, &ui_font_16, COLOR_TEXT, "");
    lv_obj_set_flex_grow(s_ui.notice_label, 1);
    lv_label_set_long_mode(s_ui.notice_label, LV_LABEL_LONG_MODE_WRAP);

    lv_obj_add_flag(s_ui.notice, LV_OBJ_FLAG_HIDDEN);
    s_ui.notice_timer = lv_timer_create(on_notice_timer, UI_NOTICE_MS, NULL);
    lv_timer_pause(s_ui.notice_timer);
}

/* ------------------------------------------------------------------- API */

void Ui_Init(UiSelectCallback on_select, void *user)
{
    memset(&s_ui, 0, sizeof(s_ui));
    s_ui.on_select = on_select;
    s_ui.user = user;
    s_ui.active = UI_NO_DEVICE;
    s_ui.pending = UI_NO_DEVICE;
    s_ui.volume = UI_VOLUME_UNKNOWN;
    s_ui.muted = UI_VOLUME_UNKNOWN;

    (void)lv_theme_default_init(lv_display_get_default(), lv_color_hex(COLOR_ACCENT),
                                lv_color_hex(COLOR_LINK), true, &ui_font_16);
    build_waiting_screen();
    build_main_screen();
    build_notice();
    render_main();
    render_audio();
    lv_screen_load(s_ui.waiting_screen);
}

void Ui_ShowWaiting(void)
{
    lv_screen_load(s_ui.waiting_screen);
}

void Ui_SetDevices(const UiDevice *devices, size_t count)
{
    if ((devices == NULL) || (count > UI_MAX_DEVICES))
    {
        count = (devices == NULL) ? 0U : UI_MAX_DEVICES;
    }
    for (size_t i = 0U; i < count; i++)
    {
        s_ui.devices[i] = devices[i];
        s_ui.devices[i].name[UI_MAX_NAME_BYTES] = '\0';
    }
    s_ui.count = (uint32_t)count;
    s_ui.pending = UI_NO_DEVICE;
    if (s_ui.page >= page_count())
    {
        s_ui.page = 0U;
    }
    render_main();
    lv_screen_load(s_ui.main_screen);
}

void Ui_SetActive(uint8_t device_id)
{
    s_ui.active = device_id;
    s_ui.pending = UI_NO_DEVICE;
    render_main();
}

void Ui_SetPending(uint8_t device_id)
{
    s_ui.pending = device_id;
    render_main();
}

void Ui_ShowError(const char *text)
{
    lv_label_set_text(s_ui.notice_label, (text != NULL) ? text : "");
    lv_obj_remove_flag(s_ui.notice, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(s_ui.notice_timer);
    lv_timer_resume(s_ui.notice_timer);
}

void Ui_ShowNotice(UiNotice notice)
{
    Ui_ShowError((notice == UI_NOTICE_SELECT_FAILED) ? TXT_SELECT_FAILED : TXT_HOST_TIMEOUT);
}

void Ui_SetAudioHandlers(UiVolumeCallback on_volume, UiMuteCallback on_mute, void *user)
{
    s_ui.on_volume = on_volume;
    s_ui.on_mute = on_mute;
    s_ui.audio_user = user;
}

void Ui_SetVolume(uint8_t volume)
{
    s_ui.volume = (volume > 100U) ? UI_VOLUME_UNKNOWN : volume;
    render_audio();
}

void Ui_SetMuted(uint8_t muted)
{
    s_ui.muted = (muted > 1U) ? UI_VOLUME_UNKNOWN : muted;
    render_audio();
}

void Ui_SetFlipHandler(UiFlipCallback on_flip, void *user)
{
    s_ui.on_flip = on_flip;
    s_ui.flip_user = user;
}

uint32_t Ui_CurrentPage(void)
{
    return s_ui.page;
}
