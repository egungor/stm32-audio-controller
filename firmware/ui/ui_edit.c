/**
 * @file    ui_edit.c
 * @brief   Edit mode and alias keyboard (#60; design A from #57).
 *
 * Edit mode: every device (in the user's order, whatever the tab) as a
 * card with four buttons: move one place left, move one place right,
 * favorite, alias. A button asks the host through UiEditHandlers; the
 * cards change when the host's new list arrives (Ui_SetDevices), as the
 * protocol says.
 *
 * Alias keyboard: lv_textarea + lv_keyboard with a Turkish Q layout. The
 * alias is limited to UI_MAX_ALIAS_BYTES of UTF-8 (the protocol's limit);
 * an empty alias removes it.
 *
 * Both screens exist only while shown (LVGL heap), like the settings page.
 */
#include <string.h>

#include "ui_internal.h"

#define EDIT_GAP     8
#define MINI_W       40
#define MINI_H       32
#define KB_BAR_H     52

/* Buttons of a card, the user data of their events: slot * 4 + action. */
typedef enum
{
    ACT_LEFT,
    ACT_RIGHT,
    ACT_STAR,
    ACT_ALIAS,
    ACT_COUNT
} EditAction;

/* ------------------------------------------------------------- keyboard */

/* Turkish Q layout; "ABC"/"abc"/"1#" switch the keyboard's mode. */
static const char *const k_kb_lower[] = {
    "q", "w", "e", "r", "t", "y", "u", "\xC4\xB1", "o", "p", "\xC4\x9F", "\xC3\xBC", "\n",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", "\xC5\x9F", "i", "\n",
    "ABC", "z", "x", "c", "v", "b", "n", "m", "\xC3\xB6", "\xC3\xA7", LV_SYMBOL_BACKSPACE, "\n",
    "1#", ",", " ", ".", LV_SYMBOL_OK, "",
};

static const char *const k_kb_upper[] = {
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\xC4\x9E", "\xC3\x9C", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", "\xC5\x9E", "\xC4\xB0", "\n",
    "abc", "Z", "X", "C", "V", "B", "N", "M", "\xC3\x96", "\xC3\x87", LV_SYMBOL_BACKSPACE, "\n",
    "1#", ",", " ", ".", LV_SYMBOL_OK, "",
};

static const char *const k_kb_special[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "-", "/", "(", ")", "&", "@", "+", "'", "!", "?", "\n",
    "abc", "#", "%", "*", "=", "_", ";", ":", "\"", LV_SYMBOL_BACKSPACE, "\n",
    "ABC", ",", " ", ".", LV_SYMBOL_OK, "",
};

#define KEY      1U
#define KEY_CTRL (LV_KEYBOARD_CTRL_BUTTON_FLAGS | 3U)
#define KEY_WIDE (LV_BUTTONMATRIX_CTRL_NO_REPEAT | 3U)

/* Widths and flags; mode and OK keys are drawn as "checked" (darker). */
static const lv_buttonmatrix_ctrl_t k_kb_text_ctrl[] = {
    KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
    KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
    KEY_CTRL, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, KEY_CTRL,
    KEY_CTRL, 2U, 10U, 2U, KEY_CTRL,
};

static const lv_buttonmatrix_ctrl_t k_kb_special_ctrl[] = {
    KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
    KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY, KEY,
    KEY_CTRL, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, KEY_CTRL,
    KEY_CTRL, 2U, 10U, 2U, KEY_CTRL,
};

static const UiDevice *find_device(uint8_t id, uint32_t *pos)
{
    for (uint32_t i = 0U; i < g_ui.count; i++)
    {
        if (g_ui.devices[i].id == id)
        {
            if (pos != NULL)
            {
                *pos = i;
            }
            return &g_ui.devices[i];
        }
    }
    return NULL;
}

/* Keep the alias within UI_MAX_ALIAS_BYTES of UTF-8: refuse an insertion
 * that would pass it (the textarea counts characters, not bytes). */
static void on_kb_insert(lv_event_t *e)
{
    const char *const add = (const char *)lv_event_get_param(e);
    const size_t have = strlen(lv_textarea_get_text(g_ui.kb_text));

    if ((add != NULL) && ((have + strlen(add)) > UI_MAX_ALIAS_BYTES))
    {
        lv_textarea_set_insert_replace(g_ui.kb_text, "");
    }
}

static void close_keyboard(void)
{
    if (g_ui.kb_screen != NULL)
    {
        lv_screen_load(g_ui.edit_screen);
        lv_obj_delete_async(g_ui.kb_screen); /* we are inside one of its events */
        g_ui.kb_screen = NULL;
        g_ui.kb_text = NULL;
    }
}

static void save_alias(void)
{
    const char *text = lv_textarea_get_text(g_ui.kb_text);

    if ((g_ui.edit.set_alias != NULL) && (find_device(g_ui.kb_device, NULL) != NULL))
    {
        (void)g_ui.edit.set_alias(g_ui.kb_device, text, g_ui.edit_user);
    }
    close_keyboard();
}

static void on_kb_event(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_READY)
    {
        save_alias();
    }
}

static void on_kb_save(lv_event_t *e)
{
    LV_UNUSED(e);
    save_alias();
}

static void on_kb_cancel(lv_event_t *e)
{
    LV_UNUSED(e);
    close_keyboard();
}

static lv_obj_t *make_bar_button(lv_obj_t *bar, const char *text, bool accent)
{
    lv_obj_t *btn = lv_button_create(bar);
    lv_obj_t *label;

    ui_style_card(btn, 10);
    lv_obj_set_style_bg_color(btn, ui_color(accent ? g_ui.pal.accent : g_ui.pal.btn), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_pad_hor(btn, 12, 0);
    lv_obj_set_height(btn, 40);
    lv_obj_set_width(btn, LV_SIZE_CONTENT);
    label = ui_make_label(btn, &ui_font_16, accent ? g_ui.pal.on_acc : g_ui.pal.text, text);
    lv_obj_center(label);
    return btn;
}

void ui_keyboard_build(void)
{
    const UiDevice *d = find_device(g_ui.kb_device, NULL);
    lv_obj_t *bar;
    lv_obj_t *mid;
    lv_obj_t *btn;
    lv_obj_t *kb;
    lv_obj_t *orig;

    g_ui.kb_screen = ui_make_screen();

    bar = ui_make_box(g_ui.kb_screen);
    lv_obj_set_size(bar, UI_WIDTH, KB_BAR_H);
    lv_obj_set_style_bg_color(bar, ui_color(g_ui.pal.bar), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(bar, 6, 0);
    lv_obj_set_style_pad_column(bar, 8, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    btn = make_bar_button(bar, SYM_CLOSE, false);
    lv_obj_set_width(btn, 44);
    lv_obj_add_event_cb(btn, on_kb_cancel, LV_EVENT_CLICKED, NULL);

    mid = ui_make_box(bar);
    lv_obj_set_flex_grow(mid, 1);
    lv_obj_set_height(mid, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);

    g_ui.kb_text = lv_textarea_create(mid);
    lv_obj_remove_style_all(g_ui.kb_text);
    lv_textarea_set_one_line(g_ui.kb_text, true);
    lv_obj_set_width(g_ui.kb_text, LV_PCT(100));
    lv_obj_set_style_text_font(g_ui.kb_text, &ui_font_22, 0);
    lv_obj_set_style_text_color(g_ui.kb_text, ui_color(g_ui.pal.text), 0);
    lv_obj_set_style_border_color(g_ui.kb_text, ui_color(g_ui.pal.accent), LV_PART_CURSOR);
    lv_obj_set_style_border_width(g_ui.kb_text, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_side(g_ui.kb_text, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_anim_duration(g_ui.kb_text, 0, LV_PART_CURSOR); /* no blinking: no redraws */
    lv_obj_add_state(g_ui.kb_text, LV_STATE_FOCUSED);                /* shows the cursor */
    lv_obj_add_event_cb(g_ui.kb_text, on_kb_insert, LV_EVENT_INSERT, NULL);
    lv_textarea_set_text(g_ui.kb_text, g_ui.kb_draft);

    orig = ui_make_label(mid, &ui_font_14, g_ui.pal.muted, (d != NULL) ? d->name : "");
    lv_obj_set_width(orig, LV_PCT(100));
    lv_label_set_long_mode(orig, LV_LABEL_LONG_MODE_DOTS);

    btn = make_bar_button(bar, "", true);
    lv_label_set_text_fmt(lv_obj_get_child(btn, 0), "%s %s", SYM_CHECK, ui_text(TXT_SAVE));
    lv_obj_add_event_cb(btn, on_kb_save, LV_EVENT_CLICKED, NULL);

    kb = lv_keyboard_create(g_ui.kb_screen);
    lv_obj_set_size(kb, UI_WIDTH, UI_HEIGHT - KB_BAR_H);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER, k_kb_lower, k_kb_text_ctrl);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER, k_kb_upper, k_kb_text_ctrl);
    lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, k_kb_special, k_kb_special_ctrl);
    lv_keyboard_set_mode(kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_keyboard_set_popovers(kb, false);
    lv_obj_set_style_bg_color(kb, ui_color(g_ui.pal.bg), 0);
    lv_obj_set_style_border_width(kb, 0, 0);
    lv_obj_set_style_pad_all(kb, 4, 0);
    lv_obj_set_style_pad_gap(kb, 4, 0);
    lv_obj_set_style_text_font(kb, &ui_font_16, 0);
    lv_obj_set_style_bg_color(kb, ui_color(g_ui.pal.btn), LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, ui_color(g_ui.pal.text), LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 8, LV_PART_ITEMS);
    lv_obj_set_style_shadow_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, g_ui.pal.dark ? 0 : 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, ui_color(g_ui.pal.line), LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, ui_color(g_ui.pal.bar), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(kb, ui_color(g_ui.pal.muted), LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(kb, ui_color(g_ui.pal.accent), LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_keyboard_set_textarea(kb, g_ui.kb_text);
    lv_obj_add_event_cb(kb, on_kb_event, LV_EVENT_READY, NULL);
}

static void open_keyboard(uint8_t device_id)
{
    const UiDevice *d = find_device(device_id, NULL);

    if (d == NULL)
    {
        return;
    }
    g_ui.kb_device = device_id;
    (void)memcpy(g_ui.kb_draft, d->alias, sizeof(g_ui.kb_draft));
    g_ui.kb_draft[UI_MAX_ALIAS_BYTES] = '\0';
    ui_keyboard_build();
    lv_screen_load(g_ui.kb_screen);
}

/* ------------------------------------------------------------- edit mode */

static uint32_t edit_pages(void)
{
    return (g_ui.count + UI_EDIT_PER_PAGE - 1U) / UI_EDIT_PER_PAGE;
}

static void on_edit_button(lv_event_t *e)
{
    const uint32_t slot = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    const uint32_t pos = (g_ui.edit_page * UI_EDIT_PER_PAGE) + slot;
    const uint32_t action = lv_buttonmatrix_get_selected_button(lv_event_get_target_obj(e));
    const UiDevice *d;

    if ((pos >= g_ui.count) || (action >= (uint32_t)ACT_COUNT))
    {
        return;
    }
    d = &g_ui.devices[pos];
    switch ((EditAction)action)
    {
    case ACT_LEFT:
        if ((pos > 0U) && (g_ui.edit.move != NULL))
        {
            (void)g_ui.edit.move(d->id, (uint8_t)(pos - 1U), g_ui.edit_user);
        }
        break;
    case ACT_RIGHT:
        if ((pos + 1U < g_ui.count) && (g_ui.edit.move != NULL))
        {
            (void)g_ui.edit.move(d->id, (uint8_t)(pos + 1U), g_ui.edit_user);
        }
        break;
    case ACT_STAR:
        if (g_ui.edit.set_favorite != NULL)
        {
            (void)g_ui.edit.set_favorite(d->id, d->favorite == 0U, g_ui.edit_user);
        }
        break;
    default:
        open_keyboard(d->id);
        break;
    }
}

static void on_edit_page(lv_event_t *e)
{
    const bool next = lv_event_get_user_data(e) != NULL;

    if (next && (g_ui.edit_page + 1U < edit_pages()))
    {
        g_ui.edit_page++;
    }
    else if (!next && (g_ui.edit_page > 0U))
    {
        g_ui.edit_page--;
    }
    else
    {
        return;
    }
    ui_edit_render();
}

static void on_edit_done(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_show_main();
}

/* Card buttons: one lv_buttonmatrix per card. The star is CHECKED (accent
 * colour) for a favorite; the arrows are DISABLED at the ends. */
static const char *const k_card_map[] = { SYM_PREV, SYM_NEXT, SYM_STAR, SYM_PENCIL, "" };

/* Styles shared by the cards (reset and filled on every build). */
static lv_style_t s_card;
static lv_style_t s_mini;
static lv_style_t s_mini_item;
static lv_style_t s_mini_checked;
static lv_style_t s_mini_pressed;
static lv_style_t s_mini_disabled;
static bool       s_styles_init;

static void init_styles(void)
{
    lv_style_t *const all[] = { &s_card, &s_mini, &s_mini_item, &s_mini_checked, &s_mini_pressed,
                                &s_mini_disabled };

    for (uint32_t i = 0U; i < sizeof(all) / sizeof(all[0]); i++)
    {
        if (s_styles_init)
        {
            lv_style_reset(all[i]);
        }
        lv_style_init(all[i]);
    }
    s_styles_init = true;

    lv_style_set_radius(&s_card, BTN_RADIUS);
    lv_style_set_bg_color(&s_card, ui_color(g_ui.pal.btn));
    lv_style_set_bg_opa(&s_card, LV_OPA_COVER);
    lv_style_set_border_color(&s_card, ui_color(g_ui.pal.line));
    lv_style_set_border_width(&s_card, g_ui.pal.dark ? 0 : 1);
    lv_style_set_pad_all(&s_card, 8);

    lv_style_set_pad_all(&s_mini, 0);
    lv_style_set_pad_gap(&s_mini, 6);
    lv_style_set_text_font(&s_mini, &ui_font_14);

    lv_style_set_radius(&s_mini_item, 8);
    lv_style_set_bg_color(&s_mini_item, ui_color(g_ui.pal.dark ? g_ui.pal.bg : g_ui.pal.track));
    lv_style_set_bg_opa(&s_mini_item, LV_OPA_COVER);
    lv_style_set_text_color(&s_mini_item, ui_color(g_ui.pal.text));

    lv_style_set_text_color(&s_mini_checked, ui_color(g_ui.pal.accent));
    lv_style_set_bg_color(&s_mini_pressed, ui_color(g_ui.pal.accent));
    lv_style_set_text_color(&s_mini_pressed, ui_color(g_ui.pal.on_acc));
    lv_style_set_bg_opa(&s_mini_disabled, LV_OPA_40);
    lv_style_set_text_opa(&s_mini_disabled, LV_OPA_40);
}

static void build_edit_card(lv_obj_t *grid, uint32_t slot)
{
    UiEditCard *c = &g_ui.edit_cards[slot];

    c->card = ui_make_box(grid);
    lv_obj_add_style(c->card, &s_card, 0);
    lv_obj_set_grid_cell(c->card, LV_GRID_ALIGN_STRETCH, (int32_t)(slot % 2U), 1, LV_GRID_ALIGN_STRETCH,
                         (int32_t)(slot / 2U), 1);

    c->name = lv_label_create(c->card);
    lv_obj_set_width(c->name, LV_PCT(100));
    lv_obj_set_height(c->name, lv_font_get_line_height(&ui_font_16));
    lv_label_set_long_mode(c->name, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(c->name, LV_ALIGN_TOP_LEFT, 0, 2);

    c->buttons = lv_buttonmatrix_create(c->card);
    lv_obj_remove_style_all(c->buttons);
    lv_buttonmatrix_set_map(c->buttons, k_card_map);
    lv_obj_set_size(c->buttons, (4 * MINI_W) + (3 * 6), MINI_H);
    lv_obj_align(c->buttons, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_style(c->buttons, &s_mini, 0);
    lv_obj_add_style(c->buttons, &s_mini_item, LV_PART_ITEMS);
    lv_obj_add_style(c->buttons, &s_mini_checked, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_add_style(c->buttons, &s_mini_pressed, LV_PART_ITEMS | LV_STATE_PRESSED);
    lv_obj_add_style(c->buttons, &s_mini_disabled, LV_PART_ITEMS | LV_STATE_DISABLED);
    lv_obj_add_event_cb(c->buttons, on_edit_button, LV_EVENT_CLICKED, (void *)(uintptr_t)slot);
}

void ui_edit_build(void)
{
    static const int32_t cols[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    static const int32_t rows[] = { LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST };
    lv_obj_t *bar;
    lv_obj_t *label;
    lv_obj_t *btn;
    lv_obj_t *grid;
    lv_obj_t *strip;

    init_styles();
    g_ui.edit_screen = ui_make_screen();

    bar = ui_make_box(g_ui.edit_screen);
    lv_obj_set_size(bar, UI_WIDTH, BAR_H);
    lv_obj_set_style_bg_color(bar, ui_color(g_ui.pal.bar), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, ui_color(g_ui.pal.line), 0);
    lv_obj_set_style_border_width(bar, g_ui.pal.dark ? 0 : 1, 0);
    label = ui_make_label(bar, &ui_font_16, g_ui.pal.text, ui_text(TXT_EDIT_TITLE));
    lv_obj_align(label, LV_ALIGN_LEFT_MID, PAD + 4, 0);
    btn = make_bar_button(bar, ui_text(TXT_DONE), true);
    lv_obj_set_height(btn, BAR_H - 6);
    lv_obj_align(btn, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_add_event_cb(btn, on_edit_done, LV_EVENT_CLICKED, NULL);

    grid = ui_make_box(g_ui.edit_screen);
    lv_obj_set_pos(grid, 0, BAR_H);
    lv_obj_set_size(grid, UI_WIDTH, UI_HEIGHT - BAR_H - BOTTOM_H);
    lv_obj_set_style_pad_all(grid, EDIT_GAP, 0);
    lv_obj_set_style_pad_gap(grid, EDIT_GAP, 0);
    lv_obj_set_grid_dsc_array(grid, cols, rows);
    for (uint32_t slot = 0U; slot < UI_EDIT_PER_PAGE; slot++)
    {
        build_edit_card(grid, slot);
    }
    g_ui.edit_empty = ui_make_label(g_ui.edit_screen, &ui_font_22, g_ui.pal.muted, ui_text(TXT_EMPTY));
    lv_obj_center(g_ui.edit_empty);

    strip = ui_make_box(g_ui.edit_screen);
    lv_obj_set_size(strip, UI_WIDTH, BOTTOM_H);
    lv_obj_align(strip, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_pad_hor(strip, PAD, 0);
    lv_obj_set_style_pad_column(strip, 8, 0);
    lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(strip, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    g_ui.edit_prev = lv_button_create(strip);
    g_ui.edit_page_label = ui_make_label(strip, &ui_font_16, g_ui.pal.muted, "");
    g_ui.edit_next = lv_button_create(strip);
    for (uint32_t i = 0U; i < 2U; i++)
    {
        lv_obj_t *b = (i == 0U) ? g_ui.edit_prev : g_ui.edit_next;

        lv_obj_set_size(b, 44, BOTTOM_H - 8);
        ui_style_card(b, 8);
        lv_obj_center(ui_make_label(b, &ui_font_16, g_ui.pal.text, (i == 0U) ? SYM_PREV : SYM_NEXT));
        lv_obj_add_event_cb(b, on_edit_page, LV_EVENT_CLICKED, (i == 0U) ? NULL : (void *)b);
    }
}

void ui_edit_render(void)
{
    const uint32_t pages = edit_pages();

    if (g_ui.edit_screen == NULL)
    {
        return;
    }
    if ((pages == 0U) || (g_ui.edit_page >= pages))
    {
        g_ui.edit_page = 0U;
    }
    for (uint32_t slot = 0U; slot < UI_EDIT_PER_PAGE; slot++)
    {
        const UiEditCard *c = &g_ui.edit_cards[slot];
        const uint32_t pos = (g_ui.edit_page * UI_EDIT_PER_PAGE) + slot;
        const UiDevice *d;

        if (pos >= g_ui.count)
        {
            lv_obj_add_flag(c->card, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        d = &g_ui.devices[pos];
        lv_obj_remove_flag(c->card, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text_fmt(c->name, "%s  %s", ui_icon_symbol(d->icon), ui_display_name(d));
        lv_buttonmatrix_clear_button_ctrl_all(c->buttons,
                                              LV_BUTTONMATRIX_CTRL_CHECKED | LV_BUTTONMATRIX_CTRL_DISABLED);
        if (pos == 0U)
        {
            lv_buttonmatrix_set_button_ctrl(c->buttons, ACT_LEFT, LV_BUTTONMATRIX_CTRL_DISABLED);
        }
        if (pos + 1U >= g_ui.count)
        {
            lv_buttonmatrix_set_button_ctrl(c->buttons, ACT_RIGHT, LV_BUTTONMATRIX_CTRL_DISABLED);
        }
        if (d->favorite != 0U)
        {
            lv_buttonmatrix_set_button_ctrl(c->buttons, ACT_STAR, LV_BUTTONMATRIX_CTRL_CHECKED);
        }
    }
    if (g_ui.count == 0U)
    {
        lv_obj_remove_flag(g_ui.edit_empty, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(g_ui.edit_empty, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text_fmt(g_ui.edit_page_label, "%u/%u", (unsigned)(g_ui.edit_page + 1U),
                          (unsigned)((pages > 0U) ? pages : 1U));
    lv_obj_set_state(g_ui.edit_prev, LV_STATE_DISABLED, g_ui.edit_page == 0U);
    lv_obj_set_state(g_ui.edit_next, LV_STATE_DISABLED, g_ui.edit_page + 1U >= pages);
}

void ui_show_edit(void)
{
    if (g_ui.edit_screen == NULL)
    {
        g_ui.edit_page = 0U;
        ui_edit_build();
    }
    ui_edit_render();
    lv_screen_load(g_ui.edit_screen);
}

void ui_edit_drop(void)
{
    if (g_ui.kb_screen != NULL)
    {
        lv_obj_delete_async(g_ui.kb_screen);
        g_ui.kb_screen = NULL;
        g_ui.kb_text = NULL;
    }
    if (g_ui.edit_screen != NULL)
    {
        lv_obj_delete_async(g_ui.edit_screen);
        g_ui.edit_screen = NULL;
    }
}
