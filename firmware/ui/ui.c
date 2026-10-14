/**
 * @file    ui.c
 * @brief   Touch UI of the audio controller (LVGL, hardware independent).
 *
 * Screens are built from g_ui.settings (theme, accent, language, grid) and
 * rebuilt when one of these changes; everything else updates the objects
 * in place.
 */
#include "ui.h"

#include <string.h>

#include "ui_internal.h"

UiState g_ui;

/* Grid shapes by UI_GRID_*: columns x rows, or a list of rows. */
typedef struct
{
    uint8_t cols;
    uint8_t rows;
    bool    list;
} GridShape;

static const GridShape k_grids[UI_GRID_COUNT] = {
    { 2U, 2U, false }, /* 2x2: 4 large cards */
    { 3U, 2U, false }, /* 3x2: 6 cards */
    { 3U, 3U, false }, /* 3x3: 9 small cards */
    { 1U, 5U, true },  /* list: 5 rows */
};

/* How much a card shows, by grid. */
typedef enum
{
    CARD_LARGE,  /* 40 px icon, 22 px name on two lines, state line */
    CARD_MEDIUM, /* 22 px icon, 16 px name on two lines, state line */
    CARD_SMALL,  /* 16 px icon, one line */
    CARD_ROW,    /* list row */
} CardKind;

static CardKind card_kind(void)
{
    switch (g_ui.settings.grid)
    {
    case UI_GRID_3X2:
        return CARD_MEDIUM;
    case UI_GRID_3X3:
        return CARD_SMALL;
    case UI_GRID_LIST:
        return CARD_ROW;
    default:
        return CARD_LARGE;
    }
}

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

static const char *display_name(const UiDevice *d)
{
    return (d->alias[0] != '\0') ? d->alias : d->name;
}

static bool settings_valid(const UiSettings *s)
{
    return (s->layout < UI_LAYOUT_COUNT) && (s->theme < UI_THEME_COUNT) && (s->accent < UI_ACCENT_COUNT) &&
           (s->grid < UI_GRID_COUNT) && (s->startup_tab <= UI_STARTUP_FAVORITES) &&
           (s->last_tab <= UI_TAB_FAVORITES) && (s->screen_timeout <= 60U) && (s->flipped <= 1U) &&
           (s->language < UI_LANGUAGE_COUNT) && (s->volume_limit_on <= 1U) && (s->volume_limit <= 100U);
}

/* ------------------------------------------------------------------ view */

/* Devices of the current tab, in the host's order. */
static void update_view(void)
{
    g_ui.view_count = 0U;
    for (uint32_t i = 0U; i < g_ui.count; i++)
    {
        if ((g_ui.tab == UI_TAB_ALL) || (g_ui.devices[i].favorite != 0U))
        {
            g_ui.view[g_ui.view_count++] = (uint8_t)i;
        }
    }
}

static uint32_t page_count(void)
{
    return (g_ui.view_count + g_ui.per_page - 1U) / g_ui.per_page;
}

static void clamp_page(void)
{
    const uint32_t pages = page_count();

    if ((pages == 0U) || (g_ui.page >= pages))
    {
        g_ui.page = 0U;
    }
}

/* ---------------------------------------------------------------- helpers */

static lv_obj_t *make_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(scr, ui_color(g_ui.pal.bg), 0);
    lv_obj_set_style_text_color(scr, ui_color(g_ui.pal.text), 0);
    lv_obj_set_style_text_font(scr, &ui_font_16, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);

    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, ui_color(color), 0);
    lv_label_set_text(label, text);
    return label;
}

/* A plain container without the theme's styles. */
static lv_obj_t *make_box(lv_obj_t *parent)
{
    lv_obj_t *box = lv_obj_create(parent);

    lv_obj_remove_style_all(box);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
    return box;
}

static void style_card(lv_obj_t *obj, int32_t radius)
{
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, ui_color(g_ui.pal.btn), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    /* On the light theme white cards need an outline. */
    lv_obj_set_style_border_color(obj, ui_color(g_ui.pal.line), 0);
    lv_obj_set_style_border_width(obj, g_ui.pal.dark ? 0 : 1, 0);
}

/* ---------------------------------------------------------------- render */

static void render_button(uint32_t slot)
{
    UiDeviceButton *b = &g_ui.buttons[slot];
    const uint32_t pos = (g_ui.page * g_ui.per_page) + slot;
    const UiDevice *d;
    bool active;
    bool pending;
    uint32_t fg;

    if (pos >= g_ui.view_count)
    {
        lv_obj_add_flag(b->btn, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    d = &g_ui.devices[g_ui.view[pos]];
    active = (d->id == g_ui.active);
    pending = !active && (d->id == g_ui.pending);
    fg = active ? g_ui.pal.on_acc : g_ui.pal.text;

    lv_obj_remove_flag(b->btn, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(b->btn, ui_color(active ? g_ui.pal.accent : g_ui.pal.btn), 0);
    if (pending)
    {
        lv_obj_set_style_border_color(b->btn, ui_color(g_ui.pal.accent), 0);
        lv_obj_set_style_border_width(b->btn, 2, 0);
    }
    else
    {
        lv_obj_set_style_border_color(b->btn, ui_color(g_ui.pal.line), 0);
        lv_obj_set_style_border_width(b->btn, (g_ui.pal.dark || active) ? 0 : 1, 0);
    }
    lv_label_set_text_static(b->icon, icon_symbol(d->icon));
    lv_obj_set_style_text_color(b->icon, ui_color(active ? g_ui.pal.on_acc : g_ui.pal.muted), 0);
    lv_label_set_text(b->name, display_name(d));
    lv_obj_set_style_text_color(b->name, ui_color(fg), 0);

    if (b->star != NULL)
    {
        lv_obj_set_style_text_color(b->star, ui_color(active ? g_ui.pal.on_acc : g_ui.pal.accent), 0);
        if (d->favorite != 0U)
        {
            lv_obj_remove_flag(b->star, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(b->star, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (b->state != NULL)
    {
        if (active || pending)
        {
            lv_label_set_text_fmt(b->state, "%s%s", active ? SYM_CHECK " " : "",
                                  ui_text(active ? TXT_ACTIVE : TXT_PENDING));
            lv_obj_set_style_text_color(b->state, ui_color(active ? g_ui.pal.on_acc : g_ui.pal.accent), 0);
            lv_obj_remove_flag(b->state, LV_OBJ_FLAG_HIDDEN);
        }
        else
        {
            lv_obj_add_flag(b->state, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void render_tabs(void)
{
    for (uint32_t t = 0U; t < 2U; t++)
    {
        const bool on = (g_ui.tab == t);

        lv_obj_set_style_text_color(g_ui.tab_labels[t], ui_color(on ? g_ui.pal.text : g_ui.pal.muted), 0);
        lv_obj_set_style_border_width(g_ui.tab_btns[t], on ? 3 : 0, 0);
    }
}

static void render_main(void)
{
    const uint32_t pages = page_count();

    for (uint32_t slot = 0U; slot < g_ui.per_page; slot++)
    {
        render_button(slot);
    }
    render_tabs();

    if (g_ui.view_count == 0U)
    {
        lv_label_set_text(g_ui.empty_label,
                          ui_text(((g_ui.tab == UI_TAB_FAVORITES) && (g_ui.count > 0U)) ? TXT_FAV_EMPTY : TXT_EMPTY));
        lv_obj_remove_flag(g_ui.empty_label, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(g_ui.empty_label, LV_OBJ_FLAG_HIDDEN);
    }

    if (pages > 1U)
    {
        lv_label_set_text_fmt(g_ui.page_label, "%u/%u", (unsigned)(g_ui.page + 1U), (unsigned)pages);
        lv_obj_remove_flag(g_ui.pager, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_state(g_ui.prev_btn, LV_STATE_DISABLED, g_ui.page == 0U);
        lv_obj_set_state(g_ui.next_btn, LV_STATE_DISABLED, g_ui.page + 1U >= pages);
    }
    else
    {
        lv_obj_add_flag(g_ui.pager, LV_OBJ_FLAG_HIDDEN);
    }
}

static void render_audio(void)
{
    const bool known = (g_ui.volume != UI_VOLUME_UNKNOWN);
    const bool muted = (g_ui.muted == 1U);
    const uint32_t fill = muted ? g_ui.pal.muted : g_ui.pal.accent;

    lv_obj_set_state(g_ui.slider, LV_STATE_DISABLED, !known);
    if (known && !lv_obj_has_state(g_ui.slider, LV_STATE_PRESSED))
    {
        lv_slider_set_value(g_ui.slider, g_ui.volume, LV_ANIM_OFF);
    }
    if (known)
    {
        lv_label_set_text_fmt(g_ui.volume_label, "%u%%", (unsigned)lv_slider_get_value(g_ui.slider));
    }
    else
    {
        lv_label_set_text_static(g_ui.volume_label, "--");
    }
    /* Muted: grey fill. Unknown: empty track, no knob. */
    lv_obj_set_style_bg_color(g_ui.slider, ui_color(fill), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(g_ui.slider, ui_color(fill), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(g_ui.slider, known ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(g_ui.slider, known ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_KNOB);

    lv_obj_set_state(g_ui.mute_btn, LV_STATE_DISABLED, g_ui.muted == UI_VOLUME_UNKNOWN);
    lv_label_set_text_static(g_ui.mute_icon, muted ? SYM_MUTED : SYM_VOLUME);
    lv_obj_set_style_bg_color(g_ui.mute_btn, ui_color(muted ? g_ui.pal.accent : g_ui.pal.btn), 0);
    lv_obj_set_style_text_color(g_ui.mute_icon, ui_color(muted ? g_ui.pal.on_acc : g_ui.pal.text), 0);
}

/* ---------------------------------------------------------------- events */

static void on_slider_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);
    const uint8_t value = (uint8_t)lv_slider_get_value(g_ui.slider);

    if ((code != LV_EVENT_VALUE_CHANGED) && (code != LV_EVENT_RELEASED))
    {
        return;
    }
    lv_label_set_text_fmt(g_ui.volume_label, "%u%%", (unsigned)value);
    if (g_ui.on_volume != NULL)
    {
        g_ui.on_volume(value, code == LV_EVENT_RELEASED, g_ui.audio_user);
    }
}

static void on_mute_clicked(lv_event_t *e)
{
    LV_UNUSED(e);
    if ((g_ui.on_mute != NULL) && (g_ui.muted != UI_VOLUME_UNKNOWN))
    {
        g_ui.on_mute(g_ui.muted == 0U, g_ui.audio_user);
    }
}

static void on_device_clicked(lv_event_t *e)
{
    const uint32_t slot = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    const uint32_t pos = (g_ui.page * g_ui.per_page) + slot;
    uint8_t id;

    if (pos >= g_ui.view_count)
    {
        return;
    }
    id = g_ui.devices[g_ui.view[pos]].id;
    if ((id != g_ui.active) && (g_ui.on_select != NULL))
    {
        g_ui.on_select(id, g_ui.user);
    }
}

static void on_page_clicked(lv_event_t *e)
{
    const bool next = lv_event_get_user_data(e) != NULL;
    const uint32_t pages = page_count();

    if (next && (g_ui.page + 1U < pages))
    {
        g_ui.page++;
    }
    else if (!next && (g_ui.page > 0U))
    {
        g_ui.page--;
    }
    else
    {
        return;
    }
    render_main();
}

static void notify_settings(void)
{
    if (g_ui.on_settings != NULL)
    {
        g_ui.on_settings(&g_ui.settings, g_ui.settings_user);
    }
}

static void on_tab_clicked(lv_event_t *e)
{
    const uint32_t tab = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    if (tab == g_ui.tab)
    {
        return;
    }
    g_ui.tab = tab;
    g_ui.page = 0U;
    update_view();
    render_main();
    g_ui.settings.last_tab = (uint8_t)tab;
    notify_settings();
}

static void on_notice_timer(lv_timer_t *timer)
{
    lv_timer_pause(timer);
    if (g_ui.notice != NULL)
    {
        lv_obj_add_flag(g_ui.notice, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_flip_armed(bool armed)
{
    g_ui.flip_armed = armed;
    for (uint32_t i = 0U; i < 2U; i++)
    {
        lv_obj_set_style_text_color(g_ui.flip_icons[i], ui_color(armed ? g_ui.pal.accent : g_ui.pal.muted), 0);
    }
}

/* A long press arms the flip (the icon turns to the accent colour); it
 * happens when the finger lifts, so the rest of the press is not read in
 * the new orientation. Sliding off the button cancels. */
static void on_flip_event(lv_event_t *e)
{
    const lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_PRESSED)
    {
        g_ui.flip_pressed_at = lv_tick_get();
    }
    else if (code == LV_EVENT_PRESSING)
    {
        if (!g_ui.flip_armed && (lv_tick_elaps(g_ui.flip_pressed_at) >= UI_FLIP_HOLD_MS))
        {
            set_flip_armed(true);
        }
    }
    else if (code == LV_EVENT_RELEASED)
    {
        const bool armed = g_ui.flip_armed;

        set_flip_armed(false);
        if (armed && (g_ui.on_flip != NULL))
        {
            g_ui.on_flip(g_ui.flip_user);
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

/* Flip button in the top right corner of a screen, over the bar area. */
static void build_flip_button(lv_obj_t *scr, uint32_t index)
{
    lv_obj_t *btn = make_box(scr);

    lv_obj_set_size(btn, FLIP_W, BAR_H);
    lv_obj_align(btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    g_ui.flip_icons[index] = make_label(btn, &ui_font_16, g_ui.pal.muted, SYM_FLIP);
    lv_obj_center(g_ui.flip_icons[index]);
    lv_obj_add_event_cb(btn, on_flip_event, LV_EVENT_ALL, NULL);
}

static void build_waiting_screen(void)
{
    lv_obj_t *box;

    g_ui.waiting_screen = make_screen();
    box = make_box(g_ui.waiting_screen);
    lv_obj_set_size(box, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_center(box);
    lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(box, 10, 0);

    (void)make_label(box, &ui_icons_40, g_ui.pal.link, SYM_HOURGLASS);
    (void)make_label(box, &ui_font_22, g_ui.pal.text, ui_text(TXT_WAITING));
    (void)make_label(box, &ui_font_16, g_ui.pal.muted, ui_text(TXT_WAITING_HINT));
    build_flip_button(g_ui.waiting_screen, 0U);
}

/* Top bar: the two tabs on the left, the connection mark and the flip
 * button on the right. */
static void build_top_bar(lv_obj_t *scr)
{
    static const char *const tab_symbols[2] = { SYM_GRID, SYM_STAR };
    static const UiTextId tab_texts[2] = { TXT_TAB_ALL, TXT_TAB_FAV };
    lv_obj_t *bar = make_box(scr);
    lv_obj_t *plug;

    lv_obj_set_size(bar, LV_PCT(100), BAR_H);
    lv_obj_set_style_bg_color(bar, ui_color(g_ui.pal.bar), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, ui_color(g_ui.pal.line), 0);
    lv_obj_set_style_border_width(bar, g_ui.pal.dark ? 0 : 1, 0);

    for (uint32_t t = 0U; t < 2U; t++)
    {
        lv_obj_t *btn = make_box(bar);

        lv_obj_set_size(btn, TAB_W, BAR_H);
        lv_obj_set_pos(btn, (int32_t)(PAD + (t * TAB_W)), 0);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        /* Underline of the selected tab. */
        lv_obj_set_style_border_side(btn, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_set_style_border_color(btn, ui_color(g_ui.pal.accent), 0);
        lv_obj_add_event_cb(btn, on_tab_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)t);
        g_ui.tab_btns[t] = btn;
        g_ui.tab_labels[t] = make_label(btn, &ui_font_16, g_ui.pal.muted, "");
        lv_label_set_text_fmt(g_ui.tab_labels[t], "%s  %s", tab_symbols[t], ui_text(tab_texts[t]));
        lv_obj_center(g_ui.tab_labels[t]);
    }

    plug = make_label(bar, &ui_font_16, g_ui.pal.link, SYM_PLUG);
    lv_obj_align(plug, LV_ALIGN_RIGHT_MID, -(FLIP_W + 4), 0);
}

static void build_device_button(lv_obj_t *parent, uint32_t slot, const GridShape *shape, CardKind kind)
{
    UiDeviceButton *b = &g_ui.buttons[slot];
    lv_obj_t *text;

    memset(b, 0, sizeof(*b));
    b->btn = lv_button_create(parent);
    style_card(b->btn, (kind == CARD_ROW) ? 10 : BTN_RADIUS);
    if (kind == CARD_ROW)
    {
        lv_obj_set_size(b->btn, LV_PCT(100), LV_PCT(100));
        lv_obj_set_grid_cell(b->btn, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, (int32_t)slot, 1);
    }
    else
    {
        lv_obj_set_grid_cell(b->btn, LV_GRID_ALIGN_STRETCH, (int32_t)(slot % shape->cols), 1,
                             LV_GRID_ALIGN_STRETCH, (int32_t)(slot / shape->cols), 1);
    }
    lv_obj_set_style_pad_hor(b->btn, (kind == CARD_SMALL) ? 8 : 10, 0);
    lv_obj_set_style_pad_ver(b->btn, (kind == CARD_ROW) ? 0 : 6, 0);
    lv_obj_set_style_pad_column(b->btn, (kind == CARD_LARGE) ? 12 : 8, 0);
    lv_obj_set_flex_flow(b->btn, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(b->btn, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(b->btn, on_device_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)slot);

    switch (kind)
    {
    case CARD_LARGE:
        b->icon = make_label(b->btn, &ui_icons_40, g_ui.pal.muted, "");
        lv_obj_set_width(b->icon, 48);
        break;
    case CARD_MEDIUM:
        b->icon = make_label(b->btn, &ui_font_22, g_ui.pal.muted, "");
        lv_obj_set_width(b->icon, 28);
        break;
    default:
        b->icon = make_label(b->btn, &ui_font_16, g_ui.pal.muted, "");
        lv_obj_set_width(b->icon, 22);
        break;
    }
    lv_obj_set_style_text_align(b->icon, LV_TEXT_ALIGN_CENTER, 0);

    text = make_box(b->btn);
    lv_obj_set_flex_grow(text, 1);
    lv_obj_set_height(text, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(text, 2, 0);

    if ((kind == CARD_LARGE) || (kind == CARD_MEDIUM))
    {
        const lv_font_t *font = (kind == CARD_LARGE) ? &ui_font_22 : &ui_font_16;

        b->name = make_label(text, font, g_ui.pal.text, "");
        lv_obj_set_width(b->name, LV_PCT(100));
        lv_obj_set_height(b->name, 2 * lv_font_get_line_height(font));
        lv_label_set_long_mode(b->name, LV_LABEL_LONG_MODE_DOTS);
        b->state = make_label(text, (kind == CARD_LARGE) ? &ui_font_16 : &ui_font_14, g_ui.pal.on_acc, "");

        /* Favorite mark in the top right corner, outside the flex flow. */
        b->star = make_label(b->btn, &ui_font_14, g_ui.pal.accent, SYM_STAR);
        lv_obj_add_flag(b->star, LV_OBJ_FLAG_IGNORE_LAYOUT);
        lv_obj_align(b->star, LV_ALIGN_TOP_RIGHT, 2, -2);
    }
    else
    {
        b->name = make_label(text, &ui_font_16, g_ui.pal.text, "");
        lv_obj_set_width(b->name, LV_PCT(100));
        /* One line; a fixed height makes LVGL cut long names with dots. */
        lv_obj_set_height(b->name, lv_font_get_line_height(&ui_font_16));
        lv_label_set_long_mode(b->name, LV_LABEL_LONG_MODE_DOTS);
        if (kind == CARD_ROW)
        {
            b->star = make_label(b->btn, &ui_font_14, g_ui.pal.accent, SYM_STAR);
        }
    }
}

static lv_obj_t *make_small_button(lv_obj_t *parent, int32_t w, const char *symbol, lv_obj_t **label)
{
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_t *l;

    lv_obj_set_size(btn, w, BOTTOM_H - 8);
    style_card(btn, 8);
    l = make_label(btn, &ui_font_16, g_ui.pal.text, symbol);
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
    lv_obj_t *strip = make_box(scr);

    lv_obj_set_size(strip, LV_PCT(100), BOTTOM_H);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_hor(strip, PAD, 0);
    lv_obj_set_style_pad_column(strip, 12, 0);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_ui.mute_btn = make_small_button(strip, 52, SYM_VOLUME, &g_ui.mute_icon);
    lv_obj_add_event_cb(g_ui.mute_btn, on_mute_clicked, LV_EVENT_CLICKED, NULL);

    g_ui.slider = lv_slider_create(strip);
    lv_slider_set_range(g_ui.slider, 0, 100);
    lv_obj_set_flex_grow(g_ui.slider, 1);
    lv_obj_set_height(g_ui.slider, 8);
    lv_obj_set_ext_click_area(g_ui.slider, 14);
    lv_obj_set_style_bg_color(g_ui.slider, ui_color(g_ui.pal.track), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_ui.slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(g_ui.slider, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(g_ui.slider, on_slider_event, LV_EVENT_ALL, NULL);

    g_ui.volume_label = make_label(strip, &ui_font_16, g_ui.pal.text, "--");
    lv_obj_set_width(g_ui.volume_label, 44);

    g_ui.pager = make_box(strip);
    lv_obj_set_size(g_ui.pager, LV_SIZE_CONTENT, LV_PCT(100));
    lv_obj_set_flex_flow(g_ui.pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_ui.pager, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(g_ui.pager, 8, 0);
    lv_obj_set_style_pad_left(g_ui.pager, 4, 0);

    g_ui.prev_btn = make_small_button(g_ui.pager, 44, SYM_PREV, NULL);
    g_ui.page_label = make_label(g_ui.pager, &ui_font_16, g_ui.pal.muted, "");
    g_ui.next_btn = make_small_button(g_ui.pager, 44, SYM_NEXT, NULL);
    lv_obj_add_event_cb(g_ui.prev_btn, on_page_clicked, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(g_ui.next_btn, on_page_clicked, LV_EVENT_CLICKED, (void *)g_ui.next_btn);
}

static void build_main_screen(void)
{
    static int32_t cols[4];
    static int32_t rows[6];
    const GridShape *shape = &k_grids[(g_ui.settings.grid < UI_GRID_COUNT) ? g_ui.settings.grid : 0U];
    const CardKind kind = card_kind();
    lv_obj_t *grid;

    g_ui.per_page = (uint32_t)shape->cols * shape->rows;
    for (uint32_t c = 0U; c < shape->cols; c++)
    {
        cols[c] = LV_GRID_FR(1);
    }
    cols[shape->cols] = LV_GRID_TEMPLATE_LAST;
    for (uint32_t r = 0U; r < shape->rows; r++)
    {
        rows[r] = LV_GRID_FR(1);
    }
    rows[shape->rows] = LV_GRID_TEMPLATE_LAST;

    g_ui.main_screen = make_screen();
    build_top_bar(g_ui.main_screen);
    build_flip_button(g_ui.main_screen, 1U);

    grid = make_box(g_ui.main_screen);
    lv_obj_set_pos(grid, 0, BAR_H);
    lv_obj_set_size(grid, LV_PCT(100), UI_HEIGHT - BAR_H - BOTTOM_H);
    lv_obj_set_style_pad_all(grid, PAD, 0);
    lv_obj_set_style_pad_gap(grid, shape->list ? 4 : PAD, 0);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (uint32_t slot = 0U; slot < g_ui.per_page; slot++)
    {
        build_device_button(grid, slot, shape, kind);
    }

    g_ui.empty_label = make_label(g_ui.main_screen, &ui_font_22, g_ui.pal.muted, "");
    lv_obj_center(g_ui.empty_label);

    build_bottom_strip(g_ui.main_screen);
}

static void build_notice(void)
{
    g_ui.notice = lv_obj_create(lv_layer_top());
    lv_obj_set_size(g_ui.notice, 440, LV_SIZE_CONTENT);
    lv_obj_align(g_ui.notice, LV_ALIGN_BOTTOM_MID, 0, -PAD);
    lv_obj_set_style_bg_color(g_ui.notice, ui_color(g_ui.pal.bar), 0);
    lv_obj_set_style_border_color(g_ui.notice, ui_color(g_ui.pal.accent), 0);
    lv_obj_set_style_border_width(g_ui.notice, 2, 0);
    lv_obj_set_style_radius(g_ui.notice, BTN_RADIUS, 0);
    lv_obj_set_style_pad_all(g_ui.notice, 10, 0);
    lv_obj_set_style_pad_column(g_ui.notice, 10, 0);
    lv_obj_set_flex_flow(g_ui.notice, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_ui.notice, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(g_ui.notice, LV_OBJ_FLAG_SCROLLABLE);

    (void)make_label(g_ui.notice, &ui_font_22, g_ui.pal.accent, SYM_WARNING);
    g_ui.notice_label = make_label(g_ui.notice, &ui_font_16, g_ui.pal.text, "");
    lv_obj_set_flex_grow(g_ui.notice_label, 1);
    lv_label_set_long_mode(g_ui.notice_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_add_flag(g_ui.notice, LV_OBJ_FLAG_HIDDEN);
}

/* Build every screen for the current settings. The previous screens are
 * deleted first so that LVGL never holds two sets at once (the 64 KB heap
 * does not fit both); an empty screen is shown meanwhile, because LVGL
 * must not delete the active screen. Nothing is drawn in between: the
 * whole rebuild runs in one call. */
static void build_all(void)
{
    const bool show_main = (g_ui.main_screen != NULL) && (lv_screen_active() == g_ui.main_screen);
    lv_obj_t *const placeholder = lv_obj_create(NULL);

    lv_screen_load(placeholder);
    if (g_ui.notice != NULL)
    {
        lv_obj_delete(g_ui.notice);
        g_ui.notice = NULL;
        lv_timer_pause(g_ui.notice_timer);
    }
    if (g_ui.waiting_screen != NULL)
    {
        lv_obj_delete(g_ui.waiting_screen);
        g_ui.waiting_screen = NULL;
    }
    if (g_ui.main_screen != NULL)
    {
        lv_obj_delete(g_ui.main_screen);
        g_ui.main_screen = NULL;
    }
    ui_theme_update();
    (void)lv_theme_default_init(lv_display_get_default(), ui_color(g_ui.pal.accent), ui_color(g_ui.pal.link),
                                g_ui.pal.dark, &ui_font_16);
    build_waiting_screen();
    build_main_screen();
    build_notice();

    clamp_page();
    render_main();
    render_audio();
    lv_screen_load(show_main ? g_ui.main_screen : g_ui.waiting_screen);
    lv_obj_delete(placeholder);
}

/* ------------------------------------------------------------------- API */

void Ui_Init(UiSelectCallback on_select, void *user)
{
    memset(&g_ui, 0, sizeof(g_ui));
    g_ui.on_select = on_select;
    g_ui.user = user;
    g_ui.active = UI_NO_DEVICE;
    g_ui.pending = UI_NO_DEVICE;
    g_ui.volume = UI_VOLUME_UNKNOWN;
    g_ui.muted = UI_VOLUME_UNKNOWN;
    g_ui.settings.volume_limit = 50U; /* defaults of the settings record */
    g_ui.notice_timer = lv_timer_create(on_notice_timer, UI_NOTICE_MS, NULL);
    lv_timer_pause(g_ui.notice_timer);

    build_all();
    lv_screen_load(g_ui.waiting_screen);
}

void Ui_ShowWaiting(void)
{
    lv_screen_load(g_ui.waiting_screen);
}

void Ui_SetDevices(const UiDevice *devices, size_t count)
{
    if ((devices == NULL) || (count > UI_MAX_DEVICES))
    {
        count = (devices == NULL) ? 0U : UI_MAX_DEVICES;
    }
    for (size_t i = 0U; i < count; i++)
    {
        g_ui.devices[i] = devices[i];
        g_ui.devices[i].name[UI_MAX_NAME_BYTES] = '\0';
        g_ui.devices[i].alias[UI_MAX_ALIAS_BYTES] = '\0';
    }
    g_ui.count = (uint32_t)count;
    g_ui.pending = UI_NO_DEVICE;
    update_view();
    clamp_page();
    render_main();
    lv_screen_load(g_ui.main_screen);
}

void Ui_SetActive(uint8_t device_id)
{
    g_ui.active = device_id;
    g_ui.pending = UI_NO_DEVICE;
    render_main();
}

void Ui_SetPending(uint8_t device_id)
{
    g_ui.pending = device_id;
    render_main();
}

void Ui_ShowError(const char *text)
{
    lv_label_set_text(g_ui.notice_label, (text != NULL) ? text : "");
    lv_obj_remove_flag(g_ui.notice, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(g_ui.notice_timer);
    lv_timer_resume(g_ui.notice_timer);
}

void Ui_ShowNotice(UiNotice notice)
{
    static const UiTextId texts[] = { TXT_SELECT_FAILED, TXT_HOST_TIMEOUT, TXT_EDIT_FAILED };

    Ui_ShowError(ui_text(((uint32_t)notice < 3U) ? texts[notice] : TXT_HOST_TIMEOUT));
}

void Ui_SetAudioHandlers(UiVolumeCallback on_volume, UiMuteCallback on_mute, void *user)
{
    g_ui.on_volume = on_volume;
    g_ui.on_mute = on_mute;
    g_ui.audio_user = user;
}

void Ui_SetVolume(uint8_t volume)
{
    g_ui.volume = (volume > 100U) ? UI_VOLUME_UNKNOWN : volume;
    render_audio();
}

void Ui_SetMuted(uint8_t muted)
{
    g_ui.muted = (muted > 1U) ? UI_VOLUME_UNKNOWN : muted;
    render_audio();
}

void Ui_SetFlipHandler(UiFlipCallback on_flip, void *user)
{
    g_ui.on_flip = on_flip;
    g_ui.flip_user = user;
}

void Ui_ApplySettings(const UiSettings *settings)
{
    const UiSettings old = g_ui.settings;
    bool rebuild;

    if ((settings == NULL) || !settings_valid(settings))
    {
        return;
    }
    g_ui.settings = *settings;
    rebuild = (old.theme != settings->theme) || (old.accent != settings->accent) ||
              (old.language != settings->language) || (old.layout != settings->layout) ||
              (old.grid != settings->grid);

    if (!g_ui.settings_seen)
    {
        /* The first settings after start choose the tab. */
        g_ui.settings_seen = true;
        if (settings->startup_tab == UI_STARTUP_LAST)
        {
            g_ui.tab = settings->last_tab;
        }
        else
        {
            g_ui.tab = (settings->startup_tab == UI_STARTUP_FAVORITES) ? UI_TAB_FAVORITES : UI_TAB_ALL;
        }
        g_ui.page = 0U;
    }
    g_ui.settings.last_tab = (uint8_t)g_ui.tab;
    update_view();
    if (rebuild)
    {
        build_all();
    }
    else
    {
        clamp_page();
        render_main();
    }
}

const UiSettings *Ui_Settings(void)
{
    return &g_ui.settings;
}

void Ui_SetSettingsHandler(UiSettingsCallback on_settings, void *user)
{
    g_ui.on_settings = on_settings;
    g_ui.settings_user = user;
}

uint32_t Ui_CurrentPage(void)
{
    return g_ui.page;
}

uint32_t Ui_CurrentTab(void)
{
    return g_ui.tab;
}

void Ui_MemoryUse(uint32_t *used, uint32_t *peak)
{
    lv_mem_monitor_t mon;

    lv_mem_monitor(&mon);
    if (used != NULL)
    {
        *used = (uint32_t)(mon.total_size - mon.free_size);
    }
    if (peak != NULL)
    {
        *peak = (uint32_t)mon.max_used;
    }
}
