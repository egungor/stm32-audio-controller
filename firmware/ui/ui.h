/**
 * @file    ui.h
 * @brief   Touch UI of the audio controller (LVGL, hardware independent).
 *
 * The module only depends on LVGL. It knows nothing about the LCD, the
 * touch controller, USB or Windows: the caller creates the LVGL display
 * and input device, feeds in the state reported by the host, and receives
 * the user's choice through a callback. The same code runs in the firmware
 * and in the Linux simulator (firmware/simulator).
 *
 * Screens:
 *   - waiting: no host connected;
 *   - main: up to UI_DEVICES_PER_PAGE device buttons per page, the active
 *     device marked by colour and by the text "Etkin çıkış", page buttons
 *     when there are more devices than fit on one page;
 *   - error notice: shown over any screen for UI_NOTICE_MS;
 *   - flip button (top right corner of both screens): held for
 *     UI_FLIP_HOLD_MS, it asks to turn the image by 180° when released.
 *
 * Call everything from the task that runs lv_timer_handler(). No dynamic
 * allocation besides the LVGL objects created once in Ui_Init().
 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UI_MAX_DEVICES       32U  /**< = PROTO_MAX_DEVICES */
#define UI_MAX_NAME_BYTES    48U  /**< = PROTO_MAX_NAME_BYTES, UTF-8 */
#define UI_DEVICES_PER_PAGE  4U
#define UI_NO_DEVICE         0xFFU
#define UI_NOTICE_MS         3000U
#define UI_FLIP_HOLD_MS      1000U

/** Device icon; values match ProtoIcon in protocol/include/protocol.h. */
typedef enum
{
    UI_ICON_GENERIC    = 0,
    UI_ICON_SPEAKERS   = 1,
    UI_ICON_HEADPHONES = 2,
    UI_ICON_HEADSET    = 3,
    UI_ICON_DISPLAY    = 4,
    UI_ICON_DIGITAL    = 5
} UiIcon;

typedef struct
{
    uint8_t id;                            /**< host's device_index */
    uint8_t icon;                          /**< UiIcon */
    char    name[UI_MAX_NAME_BYTES + 1U];  /**< UTF-8, NUL-terminated */
} UiDevice;

/** Called when the user taps a device that is not already active. */
typedef void (*UiSelectCallback)(uint8_t device_id, void *user);

#define UI_VOLUME_UNKNOWN    0xFFU  /**< volume or mute not reported */

/**
 * Called while the user drags the volume slider (0-100), and once more with
 * @p released true when the finger lifts. Rate limiting is the caller's job.
 */
typedef void (*UiVolumeCallback)(uint8_t volume, bool released, void *user);

/** Called when the user taps the mute button: the state wanted. */
typedef void (*UiMuteCallback)(bool muted, void *user);

/**
 * @brief  Build all screens on the default LVGL display and show the
 *         waiting screen. Call once, after the display is created.
 */
void Ui_Init(UiSelectCallback on_select, void *user);

/** Show the waiting screen (host not connected). Keeps the device list. */
void Ui_ShowWaiting(void);

/**
 * @brief  Replace the device list and show the main screen.
 *
 * Copies at most UI_MAX_DEVICES entries. Keeps the current page if it
 * still exists. Clears a pending selection.
 */
void Ui_SetDevices(const UiDevice *devices, size_t count);

/** Mark the device reported as default by the host (UI_NO_DEVICE: none). */
void Ui_SetActive(uint8_t device_id);

/**
 * @brief  Mark a selection that was sent to the host but not confirmed
 *         yet ("Geçiliyor…"). Cleared by Ui_SetActive or Ui_SetDevices.
 */
void Ui_SetPending(uint8_t device_id);

/** Show a short notice with a warning icon for UI_NOTICE_MS. */
void Ui_ShowError(const char *text);

/** Notices with fixed UI text. */
typedef enum
{
    UI_NOTICE_SELECT_FAILED,  /**< the computer could not switch the device */
    UI_NOTICE_HOST_TIMEOUT,   /**< the computer did not answer */
} UiNotice;

/** Show one of the fixed notices (Ui_ShowError with the UI's own text). */
void Ui_ShowNotice(UiNotice notice);

/** Set the volume and mute handlers (optional; without them the controls
 *  do nothing). */
void Ui_SetAudioHandlers(UiVolumeCallback on_volume, UiMuteCallback on_mute, void *user);

/**
 * @brief  Volume reported by the host, 0-100, or UI_VOLUME_UNKNOWN (slider
 *         disabled). Ignored while the user is dragging the slider.
 */
void Ui_SetVolume(uint8_t volume);

/** Mute reported by the host: 0, 1, or UI_VOLUME_UNKNOWN (button disabled). */
void Ui_SetMuted(uint8_t muted);

/**
 * Called when the user asks to turn the image by 180° (long press on the
 * flip button, on release). The UI does not rotate anything itself: the
 * display port does, and the UI's layout stays the same.
 */
typedef void (*UiFlipCallback)(void *user);

/** Set the flip handler (optional; without it the button does nothing). */
void Ui_SetFlipHandler(UiFlipCallback on_flip, void *user);

/** Currently shown page, 0-based (for tests). */
uint32_t Ui_CurrentPage(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
