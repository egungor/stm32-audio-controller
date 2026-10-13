/**
 * @file    link_port.c
 * @brief   Connects the protocol client to the USB HID interface and the UI.
 */
#include "link_port.h"

#include <stdbool.h>

#include "link_client.h"
#include "log.h"
#include "stm32f7xx_hal.h"
#include "ui.h"
#include "usb_link.h"

#define SEND_TIMEOUT_MS  20U
#define MAX_FRAMES_PER_RUN 8U /* bounds the time spent per UI loop */

static LinkClient s_link;
static bool s_usb_configured;

static bool cb_send_frame(const uint8_t frame[PROTO_FRAME_SIZE], void *ctx)
{
    (void)ctx;
    return UsbLink_Send(frame, SEND_TIMEOUT_MS);
}

static void cb_show_waiting(void *ctx)
{
    (void)ctx;
    Ui_ShowWaiting();
}

static void cb_show_devices(const UiDevice *devices, size_t count, void *ctx)
{
    (void)ctx;
    Log_Line("LINK LIST OK n=%u gen=%u", (unsigned)count, (unsigned)s_link.generation);
    Ui_SetDevices(devices, count);
}

static void cb_set_active(uint8_t device_id, void *ctx)
{
    (void)ctx;
    Ui_SetActive(device_id);
}

static void cb_set_pending(uint8_t device_id, void *ctx)
{
    (void)ctx;
    Ui_SetPending(device_id);
}

static void cb_notice(LinkNotice notice, void *ctx)
{
    (void)ctx;
    if (notice == LINK_NOTICE_SELECT_FAILED)
    {
        Log_Line("LINK SELECT FAIL reason=host_error");
        Ui_ShowNotice(UI_NOTICE_SELECT_FAILED);
    }
    else
    {
        Log_Line("LINK SELECT FAIL reason=timeout");
        Ui_ShowNotice(UI_NOTICE_HOST_TIMEOUT);
    }
}

static void on_ui_select(uint8_t device_id, void *user)
{
    (void)user;
    Log_Line("UI SELECT id=%u", (unsigned)device_id);
    Link_Select(&s_link, device_id, HAL_GetTick());
}

void LinkPort_Init(void)
{
    static const LinkCallbacks callbacks = {
        cb_send_frame, cb_show_waiting, cb_show_devices, cb_set_active, cb_set_pending, cb_notice, NULL,
    };

    Ui_Init(on_ui_select, NULL);
    Link_Init(&s_link, &callbacks);
}

void LinkPort_Run(void)
{
    const bool configured = UsbLink_IsConfigured();
    const uint32_t now = HAL_GetTick();
    uint8_t frame[PROTO_FRAME_SIZE];

    if (configured != s_usb_configured)
    {
        s_usb_configured = configured;
        if (configured)
        {
            Log_Line("USB CONFIGURED");
            Link_OnConnected(&s_link, now);
        }
        else
        {
            Log_Line("USB DISCONNECTED");
            Link_OnDisconnected(&s_link);
        }
    }

    for (uint32_t i = 0U; (i < MAX_FRAMES_PER_RUN) && UsbLink_Receive(frame, 0U); i++)
    {
        const uint8_t active_before = s_link.active;

        Link_OnFrame(&s_link, frame, now);
        if (s_link.active != active_before)
        {
            Log_Line("LINK ACTIVE id=%u", (unsigned)s_link.active);
        }
    }
    Link_Poll(&s_link, now);
}
