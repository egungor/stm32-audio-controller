/**
 * @file    ui_demo.c
 * @brief   Stand-in for the Windows host (build option APP_UI_DEMO): a
 *          fixed device list, confirmed selections, volume and mute.
 */
#include "ui_demo.h"

#include <string.h>

#include "log.h"
#include "lvgl.h"
#include "ui.h"
#include "ui_port.h"

#define WAITING_MS 1500U /* show the waiting screen before "connecting" */
#define CONFIRM_MS 400U  /* delay before the "host" confirms a selection */

static lv_timer_t *s_confirm_timer;
static uint8_t s_requested = UI_NO_DEVICE;

static UiDevice device(uint8_t id, uint8_t icon, const char *name)
{
    UiDevice d;

    memset(&d, 0, sizeof(d));
    d.id = id;
    d.icon = icon;
    (void)strncpy(d.name, name, UI_MAX_NAME_BYTES);
    return d;
}

static void on_confirm(lv_timer_t *timer)
{
    lv_timer_pause(timer);
    Log_Line("UI DEMO ACTIVE id=%u", (unsigned)s_requested);
    Ui_SetActive(s_requested);
}

static void on_select(uint8_t device_id, void *user)
{
    LV_UNUSED(user);
    Log_Line("UI SELECT id=%u", (unsigned)device_id);
    s_requested = device_id;
    Ui_SetPending(device_id);
    lv_timer_reset(s_confirm_timer);
    lv_timer_resume(s_confirm_timer);
}

/* The "host" accepts every volume and mute change at once. */
static void on_volume(uint8_t volume, bool released, void *user)
{
    LV_UNUSED(user);
    if (released)
    {
        Log_Line("UI VOLUME value=%u", (unsigned)volume);
    }
    Ui_SetVolume(volume);
}

static void on_mute(bool muted, void *user)
{
    LV_UNUSED(user);
    Log_Line("UI MUTE %s", muted ? "on" : "off");
    Ui_SetMuted(muted ? 1U : 0U);
}

/* Settings changed in the UI (tab or settings page): the UI applied what it
 * shows; the demo turns the display itself and has no host to store the
 * record. */
static void on_settings(const UiSettings *settings, void *user)
{
    LV_UNUSED(user);
    Log_Line("UI SETTINGS layout=%u theme=%u accent=%u grid=%u tab=%u/%u timeout=%u flipped=%u lang=%u "
             "limit=%u/%u",
             (unsigned)settings->layout, (unsigned)settings->theme, (unsigned)settings->accent,
             (unsigned)settings->grid, (unsigned)settings->startup_tab, (unsigned)settings->last_tab,
             (unsigned)settings->screen_timeout, (unsigned)settings->flipped, (unsigned)settings->language,
             (unsigned)settings->volume_limit_on, (unsigned)settings->volume_limit);
    UiPort_SetFlipped(settings->flipped != 0U);
}

void UiDemo_ToggleFlip(void)
{
    UiSettings s = *Ui_Settings();

    s.flipped = (s.flipped != 0U) ? 0U : 1U;
    Ui_ApplySettings(&s);
    on_settings(&s, NULL);
}

static void on_connect(lv_timer_t *timer)
{
    UiDevice devices[] = {
        device(0U, UI_ICON_SPEAKERS, "Hoparl\xC3\xB6rler"),
        device(1U, UI_ICON_HEADPHONES, "Kulakl\xC4\xB1k"),
        device(2U, UI_ICON_DISPLAY, "Monit\xC3\xB6r (HDMI)"),
        device(3U, UI_ICON_HEADSET, "Kablosuz kulakl\xC4\xB1k"),
        device(4U, UI_ICON_DIGITAL, "USB DAC"),
    };

    /* Two favorites so the favorites tab has something to show. */
    devices[0].favorite = 1U;
    devices[3].favorite = 1U;
    lv_timer_delete(timer);
    Ui_SetDevices(devices, sizeof(devices) / sizeof(devices[0]));
    Ui_SetActive(1U);
    Ui_SetVolume(45U);
    Ui_SetMuted(0U);
    Log_Line("UI DEMO DEVICES n=%u active=1", (unsigned)(sizeof(devices) / sizeof(devices[0])));
}

void UiDemo_Start(void)
{
    Ui_Init(on_select, NULL);
    Ui_SetAudioHandlers(on_volume, on_mute, NULL);
    Ui_SetSettingsHandler(on_settings, NULL);
    s_confirm_timer = lv_timer_create(on_confirm, CONFIRM_MS, NULL);
    lv_timer_pause(s_confirm_timer);
    (void)lv_timer_create(on_connect, WAITING_MS, NULL);
    Log_Line("UI DEMO START");
}
