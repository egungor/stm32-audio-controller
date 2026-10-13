/**
 * @file    link_client.c
 * @brief   STM32 side of protocol v1 (protocol/protocol.md, "Exchanges").
 */
#include "link_client.h"

#include <string.h>

static bool send(LinkClient *link, ProtoMessage *msg)
{
    uint8_t frame[PROTO_FRAME_SIZE];

    msg->sequence = link->sequence++;
    if ((Proto_Encode(msg, frame) != PROTO_OK) || !link->cb.send_frame(frame, link->cb.ctx))
    {
        link->send_failures++;
        return false;
    }
    return true;
}

static void send_get_list(LinkClient *link, uint32_t now_ms)
{
    ProtoMessage msg;

    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_GET_DEVICE_LIST;
    (void)send(link, &msg);
    link->last_request_ms = now_ms;
}

static void send_error(LinkClient *link, ProtoErrorCode code, uint8_t ref_command, uint8_t ref_sequence)
{
    ProtoMessage msg;

    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_ERROR;
    msg.u.error.code = (uint8_t)code;
    msg.u.error.ref_command = ref_command;
    msg.u.error.ref_sequence = ref_sequence;
    (void)send(link, &msg);
}

static void start_requesting(LinkClient *link, uint32_t now_ms)
{
    link->state = LINK_STATE_REQUESTING;
    link->assembling = false;
    link->pending = PROTO_NO_DEVICE;
    link->cb.show_waiting(link->cb.ctx);
    send_get_list(link, now_ms);
}

static void show_list(LinkClient *link)
{
    UiDevice devices[PROTO_MAX_DEVICES];
    uint8_t default_id = PROTO_NO_DEVICE;

    for (uint8_t i = 0U; i < link->total; i++)
    {
        const ProtoDeviceEntry *e = &link->entries[i];

        memset(&devices[i], 0, sizeof(devices[i]));
        devices[i].id = e->device_index;
        devices[i].icon = e->icon;
        memcpy(devices[i].name, e->name, e->name_len);
        devices[i].name[e->name_len] = '\0';
        if ((e->flags & PROTO_DEVICE_FLAG_DEFAULT) != 0U)
        {
            default_id = e->device_index;
        }
    }

    link->state = LINK_STATE_READY;
    link->pending = PROTO_NO_DEVICE;
    link->cb.show_devices(devices, link->total, link->cb.ctx);
    /* DEVICE_CHANGED follows the list; until then use the default flag. */
    if (link->active == PROTO_NO_DEVICE)
    {
        link->active = default_id;
    }
    link->cb.set_active(link->active, link->cb.ctx);
}

static void on_device_entry(LinkClient *link, const ProtoDeviceEntry *e, uint32_t now_ms)
{
    link->last_entry_ms = now_ms;

    if (!link->assembling || (e->generation != link->generation) || (e->total != link->total))
    {
        link->assembling = true;
        link->generation = e->generation;
        link->total = e->total;
        link->received_mask = 0U;
    }

    if (e->total == 0U)
    {
        link->assembling = false;
        show_list(link);
        return;
    }

    link->entries[e->position] = *e;
    link->received_mask |= (uint32_t)1U << e->position;
    if (link->received_mask == ((e->total == 32U) ? 0xFFFFFFFFU : (((uint32_t)1U << e->total) - 1U)))
    {
        link->assembling = false;
        show_list(link);
    }
}

static void on_device_changed(LinkClient *link, uint8_t device_id)
{
    link->active = device_id;
    link->pending = PROTO_NO_DEVICE;
    if (link->state == LINK_STATE_READY)
    {
        link->cb.set_active(device_id, link->cb.ctx);
    }
}

static void on_error(LinkClient *link, const ProtoError *err)
{
    if ((err->ref_command != PROTO_CMD_SELECT_DEVICE) || (link->pending == PROTO_NO_DEVICE) ||
        (err->ref_sequence != link->pending_seq))
    {
        return;  /* not about the selection in flight */
    }
    link->pending = PROTO_NO_DEVICE;
    link->cb.set_active(link->active, link->cb.ctx);  /* clears the pending mark */
    link->cb.notice(LINK_NOTICE_SELECT_FAILED, link->cb.ctx);
}

void Link_Init(LinkClient *link, const LinkCallbacks *callbacks)
{
    memset(link, 0, sizeof(*link));
    link->cb = *callbacks;
    link->state = LINK_STATE_DISCONNECTED;
    link->active = PROTO_NO_DEVICE;
    link->pending = PROTO_NO_DEVICE;
}

void Link_OnConnected(LinkClient *link, uint32_t now_ms)
{
    link->active = PROTO_NO_DEVICE;
    start_requesting(link, now_ms);
}

void Link_OnDisconnected(LinkClient *link)
{
    link->state = LINK_STATE_DISCONNECTED;
    link->assembling = false;
    link->pending = PROTO_NO_DEVICE;
    link->cb.show_waiting(link->cb.ctx);
}

void Link_OnFrame(LinkClient *link, const uint8_t frame[PROTO_FRAME_SIZE], uint32_t now_ms)
{
    ProtoMessage msg;
    const ProtoStatus status = Proto_Decode(frame, &msg);

    if (link->state == LINK_STATE_DISCONNECTED)
    {
        return;
    }
    if (status != PROTO_OK)
    {
        link->frames_rejected++;
        if (msg.command != PROTO_CMD_ERROR)
        {
            send_error(link, Proto_ErrorCodeFor(status), msg.command, msg.sequence);
        }
        return;
    }

    switch (msg.command)
    {
    case PROTO_CMD_DEVICE_LIST:
        on_device_entry(link, &msg.u.device, now_ms);
        break;
    case PROTO_CMD_DEVICE_CHANGED:
        on_device_changed(link, msg.u.device_index);
        break;
    case PROTO_CMD_ERROR:
        if (msg.u.error.code == PROTO_ERROR_UNKNOWN_DEVICE)
        {
            /* Our list is stale: fetch it again after reporting. */
            on_error(link, &msg.u.error);
            start_requesting(link, now_ms);
        }
        else
        {
            on_error(link, &msg.u.error);
        }
        break;
    case PROTO_CMD_VOLUME_CHANGED:
    case PROTO_CMD_MUTE_CHANGED:
        break;  /* volume UI is M6 */
    default:
        break;  /* STM32 -> host commands coming the wrong way: drop */
    }
}

void Link_Poll(LinkClient *link, uint32_t now_ms)
{
    /* A new list stopped arriving half way (a frame was lost, e.g. the
     * receive queue was full): ask for it again. While requesting, the
     * retry below already covers this. */
    if ((link->state == LINK_STATE_READY) && link->assembling &&
        ((now_ms - link->last_entry_ms) >= LINK_RETRY_MS))
    {
        link->assembling = false;
        send_get_list(link, now_ms);
    }
    if ((link->state == LINK_STATE_REQUESTING) && ((now_ms - link->last_request_ms) >= LINK_RETRY_MS))
    {
        send_get_list(link, now_ms);
    }
    if ((link->state == LINK_STATE_READY) && (link->pending != PROTO_NO_DEVICE) &&
        ((now_ms - link->pending_since_ms) >= LINK_RESPONSE_TIMEOUT_MS))
    {
        link->cb.notice(LINK_NOTICE_HOST_TIMEOUT, link->cb.ctx);
        start_requesting(link, now_ms);
    }
}

void Link_Select(LinkClient *link, uint8_t device_id, uint32_t now_ms)
{
    ProtoMessage msg;

    if ((link->state != LINK_STATE_READY) || (device_id == PROTO_NO_DEVICE))
    {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_SELECT_DEVICE;
    msg.u.device_index = device_id;
    link->pending_seq = link->sequence;
    if (send(link, &msg))
    {
        link->pending = device_id;
        link->pending_since_ms = now_ms;
        link->cb.set_pending(device_id, link->cb.ctx);
    }
}
