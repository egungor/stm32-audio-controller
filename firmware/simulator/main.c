/**
 * @file    main.c
 * @brief   Headless LVGL simulator for firmware/ui.
 *
 * Renders the UI into a 480 x 272 RGB565 framebuffer (the LCD format) and
 * plays a fixed script of UI states and simulated touches. After each step
 * the framebuffer is written as a raw dump, <out>/<name>.bin, which
 * tools/fb2png.py converts to PNG (tools/run-ui-simulator.sh does both).
 *
 * The script also checks the UI behaviour (selection callback, paging) and
 * exits with a non-zero status if a check fails, so CI can run it.
 *
 * Usage: ui_simulator <output directory>
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "ui.h"

#define WIDTH  480
#define HEIGHT 272

static uint16_t s_framebuffer[WIDTH * HEIGHT];
static uint16_t s_turned[WIDTH * HEIGHT];  /* what the panel shows when flipped */
static uint32_t s_now_ms;
static const char *s_out_dir;
static int s_failures;

static struct
{
    bool    pressed;
    int32_t x;
    int32_t y;
} s_touch;

static struct
{
    int     calls;
    uint8_t last_id;
} s_select;

static struct
{
    int     calls;
    int     released;
    uint8_t last;
    int     mute_calls;
    bool    mute_wanted;
} s_audio;

static int s_flip_calls;

/* ---------------------------------------------------------------- platform */

static uint32_t tick_cb(void)
{
    return s_now_ms;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* Direct mode: LVGL draws into s_framebuffer itself. */
    LV_UNUSED(area);
    LV_UNUSED(px_map);
    lv_display_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);
    data->point.x = s_touch.x;
    data->point.y = s_touch.y;
    data->state = s_touch.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

/* Advance simulated time in small steps so timers and input run as on the
 * board (the firmware calls lv_timer_handler about every 5 ms). */
static void run_ms(uint32_t ms)
{
    for (uint32_t t = 0U; t < ms; t += 5U)
    {
        s_now_ms += 5U;
        (void)lv_timer_handler();
    }
}

static void tap(int32_t x, int32_t y)
{
    s_touch.x = x;
    s_touch.y = y;
    s_touch.pressed = true;
    run_ms(100U);
    s_touch.pressed = false;
    run_ms(100U);
}

/* Press at (x0, y), move to x1 in 10 px steps, release. */
static void drag(int32_t x0, int32_t x1, int32_t y)
{
    const int32_t step = (x1 > x0) ? 10 : -10;

    s_touch.x = x0;
    s_touch.y = y;
    s_touch.pressed = true;
    run_ms(60U);
    while ((step > 0) ? (s_touch.x < x1) : (s_touch.x > x1))
    {
        s_touch.x += step;
        run_ms(20U);
    }
    s_touch.x = x1;
    run_ms(60U);
    s_touch.pressed = false;
    run_ms(60U);
}

/* Press at (x, y) for @p ms, release. */
static void hold(int32_t x, int32_t y, uint32_t ms)
{
    s_touch.x = x;
    s_touch.y = y;
    s_touch.pressed = true;
    run_ms(ms);
    s_touch.pressed = false;
    run_ms(100U);
}

/* Write the framebuffer; @p turned: as the board's display port shows it
 * when flipped (pixel order reversed). */
static void snapshot_as(const char *name, bool turned)
{
    char path[512];
    const uint16_t *pixels = s_framebuffer;
    FILE *f;

    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL);
    if (turned)
    {
        for (size_t i = 0U; i < (size_t)(WIDTH * HEIGHT); i++)
        {
            s_turned[(WIDTH * HEIGHT) - 1U - i] = s_framebuffer[i];
        }
        pixels = s_turned;
    }
    (void)snprintf(path, sizeof(path), "%s/%s.bin", s_out_dir, name);
    f = fopen(path, "wb");
    if ((f == NULL) || (fwrite(pixels, sizeof(s_framebuffer), 1U, f) != 1U))
    {
        printf("FAIL cannot write %s\n", path);
        s_failures++;
    }
    else
    {
        printf("wrote %s\n", path);
    }
    if (f != NULL)
    {
        (void)fclose(f);
    }
}

static void snapshot(const char *name)
{
    snapshot_as(name, false);
}

static void check(bool ok, const char *what)
{
    if (!ok)
    {
        printf("FAIL %s\n", what);
        s_failures++;
    }
}

static void on_select(uint8_t device_id, void *user)
{
    LV_UNUSED(user);
    s_select.calls++;
    s_select.last_id = device_id;
}

static void on_volume(uint8_t volume, bool released, void *user)
{
    LV_UNUSED(user);
    s_audio.calls++;
    s_audio.released += released ? 1 : 0;
    s_audio.last = volume;
}

static void on_flip(void *user)
{
    LV_UNUSED(user);
    s_flip_calls++;
}

static void on_mute(bool muted, void *user)
{
    LV_UNUSED(user);
    s_audio.mute_calls++;
    s_audio.mute_wanted = muted;
}

/* ---------------------------------------------------------------- script */

static UiDevice device(uint8_t id, uint8_t icon, const char *name)
{
    UiDevice d;

    memset(&d, 0, sizeof(d));
    d.id = id;
    d.icon = icon;
    (void)snprintf(d.name, sizeof(d.name), "%s", name);
    return d;
}

static void run_script(void)
{
    /* Example names, including Turkish letters and a long name. */
    const UiDevice devices[] = {
        device(0U, UI_ICON_SPEAKERS, "Hoparl\xC3\xB6rler"),
        device(1U, UI_ICON_HEADPHONES, "Kulakl\xC4\xB1k"),
        device(2U, UI_ICON_DISPLAY, "Monit\xC3\xB6r (HDMI)"),
        device(3U, UI_ICON_HEADSET, "Kablosuz kulakl\xC4\xB1k (Bluetooth)"),
        device(4U, UI_ICON_DIGITAL, "USB DAC"),
        device(5U, UI_ICON_GENERIC, "Dijital \xC3\xA7\xC4\xB1k\xC4\xB1\xC5\x9F (S/PDIF) - "
                                    "\xC3\x87ok uzun bir cihaz ad\xC4\xB1"),
    };

    snapshot("01-waiting");

    Ui_SetDevices(devices, sizeof(devices) / sizeof(devices[0]));
    Ui_SetActive(1U);
    Ui_SetVolume(45U);
    Ui_SetMuted(0U);
    run_ms(50U);
    snapshot("02-main");

    /* Tap "Hoparlörler" (top-left button). */
    tap(120, 85);
    check(s_select.calls == 1, "tap on a device calls the select callback once");
    check(s_select.last_id == 0U, "tap on the top-left device selects id 0");
    Ui_SetPending(0U);
    run_ms(50U);
    snapshot("03-pending");

    Ui_SetActive(0U);
    run_ms(50U);
    snapshot("04-switched");

    /* Tapping the active device does nothing. */
    tap(120, 85);
    check(s_select.calls == 1, "tap on the active device does not call the callback");

    /* Next page button (bottom right). */
    tap(452, 252);
    check(Ui_CurrentPage() == 1U, "next page button shows page 2");
    snapshot("05-page2");

    Ui_ShowNotice(UI_NOTICE_HOST_TIMEOUT);
    run_ms(50U);
    snapshot("06-error");
    run_ms(UI_NOTICE_MS + 100U);
    snapshot("07-error-gone");

    /* Drag the volume slider to the right; the host's report while the
     * finger is down does not move the knob. */
    drag(140, 230, 252);
    check(s_audio.calls > 2, "dragging the slider reports several values");
    check(s_audio.released == 1, "releasing the slider reports once with released");
    check(s_audio.last > 45U, "the slider moved up");
    Ui_SetVolume(s_audio.last);
    run_ms(50U);
    snapshot("08-volume");

    /* Mute: the tap asks; the host's MUTE_CHANGED changes the icon. */
    tap(34, 252);
    check((s_audio.mute_calls == 1) && s_audio.mute_wanted, "mute button asks to mute");
    Ui_SetMuted(1U);
    run_ms(50U);
    snapshot("09-muted");

    /* No default device: volume and mute unknown, controls disabled. */
    Ui_SetActive(UI_NO_DEVICE);
    Ui_SetVolume(UI_VOLUME_UNKNOWN);
    Ui_SetMuted(UI_VOLUME_UNKNOWN);
    run_ms(50U);
    tap(34, 252);
    check(s_audio.mute_calls == 1, "disabled mute button does nothing");
    snapshot("10-no-volume");

    Ui_SetDevices(NULL, 0U);
    run_ms(50U);
    snapshot("11-empty");

    /* Flip button, top right corner: a tap does nothing, a long press asks
     * once, when the finger lifts. */
    Ui_SetDevices(devices, sizeof(devices) / sizeof(devices[0]));
    Ui_SetActive(1U);
    Ui_SetVolume(45U);
    Ui_SetMuted(0U);
    run_ms(50U);
    tap(458, 18);
    check(s_flip_calls == 0, "a short tap on the flip button does nothing");
    s_touch.x = 458;
    s_touch.y = 18;
    s_touch.pressed = true;
    run_ms(UI_FLIP_HOLD_MS + 100U);
    check(s_flip_calls == 0, "the flip waits for the finger to lift");
    snapshot("12-flip-armed");
    s_touch.pressed = false;
    run_ms(100U);
    check(s_flip_calls == 1, "a long press on the flip button asks to flip once");
    snapshot_as("13-flipped", true);

    /* Same button on the waiting screen. */
    Ui_ShowWaiting();
    run_ms(50U);
    hold(458, 18, UI_FLIP_HOLD_MS + 100U);
    check(s_flip_calls == 2, "the flip button works on the waiting screen");
}

int main(int argc, char **argv)
{
    lv_display_t *disp;
    lv_indev_t *indev;

    if (argc != 2)
    {
        printf("usage: %s <output directory>\n", argv[0]);
        return 2;
    }
    s_out_dir = argv[1];

    lv_init();
    lv_tick_set_cb(tick_cb);

    disp = lv_display_create(WIDTH, HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, s_framebuffer, NULL, sizeof(s_framebuffer),
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, flush_cb);

    indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);

    Ui_Init(on_select, NULL);
    Ui_SetAudioHandlers(on_volume, on_mute, NULL);
    Ui_SetFlipHandler(on_flip, NULL);
    run_ms(50U);
    run_script();

    printf("%d failures\n", s_failures);
    return (s_failures == 0) ? 0 : 1;
}
