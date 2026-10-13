/**
 * @file    ui.c
 * @brief   Touch UI of the audio controller (LVGL, hardware independent).
 */
#include "ui.h"

#include <string.h>

#include "lvgl.h"

/* Layout for the 480 x 272 panel. */
#define BAR_H        36
#define PAGER_H      36
#define PAD          8
#define BTN_RADIUS   12

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

/* UI text (Turkish). */
#define TXT_TITLE        "SES \xC3\x87IKI\xC5\x9EI"               /* SES ÇIKIŞI */
#define TXT_CONNECTED    "Bilgisayar ba\xC4\x9Fl\xC4\xB1"           /* Bilgisayar bağlı */
#define TXT_WAITING      "Bilgisayar bekleniyor"
#define TXT_WAITING_HINT "USB kablosunu ve uygulamay\xC4\xB1 kontrol edin"
#define TXT_ACTIVE       SYM_CHECK " Etkin \xC3\xA7\xC4\xB1k\xC4\xB1\xC5\x9F" /* Etkin çıkış */
#define TXT_PENDING      "Ge\xC3\xA7iliyor..."                     /* Geçiliyor... */
#define TXT_EMPTY        "Ses \xC3\xA7\xC4\xB1k\xC4\xB1\xC5\x9F\xC4\xB1 bulunamad\xC4\xB1"

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
    lv_obj_t    *prev_btn;
    lv_obj_t    *next_btn;
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
        lv_label_set_text_fmt(s_ui.page_label, "Sayfa %u / %u", (unsigned)(s_ui.page + 1U),
                              (unsigned)pages);
        lv_obj_remove_flag(s_ui.pager, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_state(s_ui.prev_btn, LV_STATE_DISABLED, s_ui.page == 0U);
        lv_obj_set_state(s_ui.next_btn, LV_STATE_DISABLED, s_ui.page + 1U >= pages);
    }
    else
    {
        lv_obj_add_flag(s_ui.pager, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---------------------------------------------------------------- events */

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

/* ----------------------------------------------------------------- build */

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
    lv_obj_align(status, LV_ALIGN_RIGHT_MID, 0, 0);
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

static void build_main_screen(void)
{
    static const int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t *grid;

    s_ui.main_screen = make_screen();
    build_status_bar(s_ui.main_screen);

    grid = lv_obj_create(s_ui.main_screen);
    lv_obj_remove_style_all(grid);
    lv_obj_set_pos(grid, 0, BAR_H);
    lv_obj_set_size(grid, LV_PCT(100), 272 - BAR_H - PAGER_H);
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

    s_ui.pager = lv_obj_create(s_ui.main_screen);
    lv_obj_remove_style_all(s_ui.pager);
    lv_obj_set_size(s_ui.pager, LV_PCT(100), PAGER_H);
    lv_obj_align(s_ui.pager, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_flex_flow(s_ui.pager, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_ui.pager, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_ui.pager, 24, 0);

    s_ui.prev_btn = lv_button_create(s_ui.pager);
    s_ui.page_label = make_label(s_ui.pager, &ui_font_16, COLOR_MUTED, "");
    s_ui.next_btn = lv_button_create(s_ui.pager);
    lv_obj_move_to_index(s_ui.page_label, 1);
    for (int i = 0; i < 2; i++)
    {
        lv_obj_t *btn = (i == 0) ? s_ui.prev_btn : s_ui.next_btn;

        lv_obj_set_size(btn, 64, 28);
        lv_obj_set_style_bg_color(btn, lv_color_hex(COLOR_BTN), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        (void)make_label(btn, &ui_font_16, COLOR_TEXT, (i == 0) ? SYM_PREV : SYM_NEXT);
        lv_obj_center(lv_obj_get_child(btn, 0));
        lv_obj_add_event_cb(btn, on_page_clicked, LV_EVENT_CLICKED, (i == 0) ? NULL : (void *)btn);
    }
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

    (void)lv_theme_default_init(lv_display_get_default(), lv_color_hex(COLOR_ACCENT),
                                lv_color_hex(COLOR_LINK), true, &ui_font_16);
    build_waiting_screen();
    build_main_screen();
    build_notice();
    render_main();
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

uint32_t Ui_CurrentPage(void)
{
    return s_ui.page;
}
