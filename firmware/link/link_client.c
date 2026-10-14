/**
 * @file    link_client.c
 * @brief   STM32 side of protocol v2 (protocol/protocol.md, "Exchanges").
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

static void show_volume(LinkClient *link, uint8_t volume)
{
    link->shown_volume = volume;
    link->cb.set_volume(volume, link->cb.ctx);
}

/* Volume and mute unknown until the host reports them (again). */
static void forget_volume(LinkClient *link)
{
    link->host_volume = LINK_VOLUME_UNKNOWN;
    link->host_muted = LINK_VOLUME_UNKNOWN;
    link->volume_local = false;
    link->volume_dirty = false;
    show_volume(link, LINK_VOLUME_UNKNOWN);
    link->cb.set_muted(LINK_VOLUME_UNKNOWN, link->cb.ctx);
}

static void send_volume(LinkClient *link, uint32_t now_ms)
{
    ProtoMessage msg;

    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_SET_VOLUME;
    msg.u.volume = link->wanted_volume;
    (void)send(link, &msg);
    link->volume_dirty = false;
    link->volume_ever_sent = true;
    link->volume_sent_ms = now_ms;
}

static void on_volume_changed(LinkClient *link, uint8_t volume)
{
    link->host_volume = volume;
    if (!link->volume_local)
    {
        show_volume(link, volume);
    }
}

static void on_mute_changed(LinkClient *link, uint8_t muted)
{
    link->host_muted = muted;
    link->cb.set_muted(muted, link->cb.ctx);
}

static void apply_settings(LinkClient *link, const ProtoSettings *settings)
{
    link->settings = *settings;
    if (link->cb.apply_settings != NULL)
    {
        link->cb.apply_settings(&link->settings, link->cb.ctx);
    }
}

static void send_settings(LinkClient *link, uint32_t now_ms)
{
    ProtoMessage msg;

    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_SET_SETTINGS;
    msg.u.settings = link->settings;
    link->settings_seq = link->sequence;
    if (send(link, &msg))
    {
        link->settings_inflight = true;
        link->settings_since_ms = now_ms;
    }
    else
    {
        link->settings_offline = true; /* try again when the host's SETTINGS arrive */
    }
}

/* Requests in flight end with the session; a settings change that was not
 * confirmed is sent again when the host's SETTINGS arrive. */
static void end_requests(LinkClient *link)
{
    link->pending = PROTO_NO_DEVICE;
    link->edit_command = 0U;
    if (link->settings_inflight || link->settings_resend)
    {
        link->settings_offline = true;
    }
    link->settings_inflight = false;
    link->settings_resend = false;
}

static void start_requesting(LinkClient *link, uint32_t now_ms)
{
    link->state = LINK_STATE_REQUESTING;
    link->assembling = false;
    end_requests(link);
    link->cb.show_waiting(link->cb.ctx);
    forget_volume(link);
    send_get_list(link, now_ms);
}

static uint32_t full_mask(uint8_t total)
{
    return (total >= 32U) ? 0xFFFFFFFFU : (((uint32_t)1U << total) - 1U);
}

static void show_list(LinkClient *link)
{
    uint8_t default_id = PROTO_NO_DEVICE;

    for (uint8_t i = 0U; i < link->total; i++)
    {
        const ProtoDeviceEntry *e = &link->entries[i];
        UiDevice *d = &link->shown[i];

        memset(d, 0, sizeof(*d));
        d->id = e->device_index;
        d->icon = e->icon;
        memcpy(d->name, e->name, e->name_len);
        d->name[e->name_len] = '\0';
        if ((e->flags & PROTO_DEVICE_FLAG_ALIAS) != 0U)
        {
            (void)memcpy(d->alias, link->aliases[i], sizeof(d->alias));
        }
        d->favorite = ((e->flags & PROTO_DEVICE_FLAG_FAVORITE) != 0U) ? 1U : 0U;
        if ((e->flags & PROTO_DEVICE_FLAG_DEFAULT) != 0U)
        {
            default_id = e->device_index;
        }
    }

    link->shown_count = link->total;
    link->shown_generation = link->generation;
    link->state = LINK_STATE_READY;
    link->pending = PROTO_NO_DEVICE;
    link->edit_command = 0U; /* the host answers an edit with a new list */
    link->cb.show_devices(link->shown, link->shown_count, link->cb.ctx);
    /* DEVICE_CHANGED follows the list; until then use the default flag. */
    if (link->active == PROTO_NO_DEVICE)
    {
        link->active = default_id;
    }
    link->cb.set_active(link->active, link->cb.ctx);
}

static void check_complete(LinkClient *link)
{
    if ((link->received_mask == full_mask(link->total)) &&
        ((link->alias_received_mask & link->alias_expected_mask) == link->alias_expected_mask))
    {
        link->assembling = false;
        show_list(link);
    }
}

static void on_device_entry(LinkClient *link, const ProtoDeviceEntry *e, uint32_t now_ms)
{
    const uint32_t bit = (uint32_t)1U << e->position;

    link->last_entry_ms = now_ms;

    if (!link->assembling || (e->generation != link->generation) || (e->total != link->total))
    {
        link->assembling = true;
        link->generation = e->generation;
        link->total = e->total;
        link->received_mask = 0U;
        link->alias_expected_mask = 0U;
        link->alias_received_mask = 0U;
    }

    if (e->total == 0U)
    {
        link->assembling = false;
        show_list(link);
        return;
    }

    link->entries[e->position] = *e;
    link->received_mask |= bit;
    if ((e->flags & PROTO_DEVICE_FLAG_ALIAS) != 0U)
    {
        link->alias_expected_mask |= bit;
    }
    else
    {
        link->alias_expected_mask &= ~bit;
    }
    check_complete(link);
}

static void on_alias_entry(LinkClient *link, const ProtoAliasEntry *a, uint32_t now_ms)
{
    /* An alias belongs to the list being assembled; others are stale. */
    if (!link->assembling || (a->generation != link->generation) || (a->position >= link->total))
    {
        return;
    }
    link->last_entry_ms = now_ms;
    memcpy(link->aliases[a->position], a->alias, a->alias_len);
    link->aliases[a->position][a->alias_len] = '\0';
    link->alias_received_mask |= (uint32_t)1U << a->position;
    check_complete(link);
}

static void on_device_changed(LinkClient *link, uint8_t device_id)
{
    link->active = device_id;
    link->pending = PROTO_NO_DEVICE;
    if (link->state == LINK_STATE_READY)
    {
        link->cb.set_active(device_id, link->cb.ctx);
    }
    if (device_id == PROTO_NO_DEVICE)
    {
        forget_volume(link);  /* nothing to control */
    }
}

static void on_settings(LinkClient *link, const ProtoSettings *settings, uint32_t now_ms)
{
    if (link->settings_offline)
    {
        /* The user changed something while no host was there: theirs wins. */
        link->settings_offline = false;
        send_settings(link, now_ms);
        return;
    }
    if (link->settings_inflight)
    {
        link->settings_inflight = false;
        if (link->settings_resend)
        {
            link->settings_resend = false;
            send_settings(link, now_ms);
            return;
        }
    }
    if (memcmp(&link->settings, settings, sizeof(*settings)) != 0)
    {
        apply_settings(link, settings);
    }
}

static void on_error(LinkClient *link, const ProtoError *err)
{
    if ((err->ref_command == PROTO_CMD_SELECT_DEVICE) && (link->pending != PROTO_NO_DEVICE) &&
        (err->ref_sequence == link->pending_seq))
    {
        link->pending = PROTO_NO_DEVICE;
        link->cb.set_active(link->active, link->cb.ctx);  /* clears the pending mark */
        link->cb.notice(LINK_NOTICE_SELECT_FAILED, link->cb.ctx);
    }
    else if ((link->edit_command != 0U) && (err->ref_command == link->edit_command) &&
             (err->ref_sequence == link->edit_seq))
    {
        link->edit_command = 0U;
        link->cb.notice(LINK_NOTICE_EDIT_FAILED, link->cb.ctx);
    }
    else if ((err->ref_command == PROTO_CMD_SET_SETTINGS) && link->settings_inflight &&
             (err->ref_sequence == link->settings_seq))
    {
        link->settings_inflight = false;
        link->settings_resend = false;
        link->cb.notice(LINK_NOTICE_EDIT_FAILED, link->cb.ctx);
    }
    else
    {
        /* not about a request in flight */
    }
}

void Link_Init(LinkClient *link, const LinkCallbacks *callbacks)
{
    memset(link, 0, sizeof(*link));
    link->cb = *callbacks;
    link->state = LINK_STATE_DISCONNECTED;
    link->active = PROTO_NO_DEVICE;
    link->pending = PROTO_NO_DEVICE;
    link->host_volume = LINK_VOLUME_UNKNOWN;
    link->host_muted = LINK_VOLUME_UNKNOWN;
    link->shown_volume = LINK_VOLUME_UNKNOWN;
    Proto_DefaultSettings(&link->settings);
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
    end_requests(link);
    link->cb.show_waiting(link->cb.ctx);
    forget_volume(link);
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
    case PROTO_CMD_DEVICE_ALIAS:
        on_alias_entry(link, &msg.u.alias, now_ms);
        break;
    case PROTO_CMD_DEVICE_CHANGED:
        on_device_changed(link, msg.u.device_index);
        break;
    case PROTO_CMD_SETTINGS:
        on_settings(link, &msg.u.settings, now_ms);
        break;
    case PROTO_CMD_ERROR:
        on_error(link, &msg.u.error);
        if (msg.u.error.code == PROTO_ERROR_UNKNOWN_DEVICE)
        {
            /* Our list is stale: fetch it again. */
            start_requesting(link, now_ms);
        }
        break;
    case PROTO_CMD_VOLUME_CHANGED:
        on_volume_changed(link, msg.u.volume);
        break;
    case PROTO_CMD_MUTE_CHANGED:
        on_mute_changed(link, msg.u.muted);
        break;
    default:
        break;  /* STM32 -> host commands coming the wrong way: drop */
    }
}

static bool timed_out(uint32_t now_ms, uint32_t since_ms)
{
    return (now_ms - since_ms) >= LINK_RESPONSE_TIMEOUT_MS;
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
    if ((link->state == LINK_STATE_READY) && link->volume_dirty &&
        ((now_ms - link->volume_sent_ms) >= LINK_VOLUME_INTERVAL_MS))
    {
        send_volume(link, now_ms);
    }
    if (link->volume_local && !link->volume_dirty &&
        ((now_ms - link->volume_sent_ms) >= LINK_VOLUME_SETTLE_MS))
    {
        /* Local change over: show what the host reports. */
        link->volume_local = false;
        if (link->host_volume != link->shown_volume)
        {
            show_volume(link, link->host_volume);
        }
    }
    if ((link->state == LINK_STATE_READY) &&
        (((link->pending != PROTO_NO_DEVICE) && timed_out(now_ms, link->pending_since_ms)) ||
         ((link->edit_command != 0U) && timed_out(now_ms, link->edit_since_ms)) ||
         (link->settings_inflight && timed_out(now_ms, link->settings_since_ms))))
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
    msg.u.request.generation = link->shown_generation;
    msg.u.request.device_index = device_id;
    link->pending_seq = link->sequence;
    if (send(link, &msg))
    {
        link->pending = device_id;
        link->pending_since_ms = now_ms;
        link->cb.set_pending(device_id, link->cb.ctx);
    }
}

static bool in_shown_list(const LinkClient *link, uint8_t device_id)
{
    for (uint8_t i = 0U; i < link->shown_count; i++)
    {
        if (link->shown[i].id == device_id)
        {
            return true;
        }
    }
    return false;
}

static bool can_edit(const LinkClient *link, uint8_t device_id)
{
    return (link->state == LINK_STATE_READY) && (link->edit_command == 0U) &&
           (device_id != PROTO_NO_DEVICE) && in_shown_list(link, device_id);
}

static bool send_edit(LinkClient *link, ProtoMessage *msg, uint32_t now_ms)
{
    const uint8_t seq = link->sequence;

    if (!send(link, msg))
    {
        return false;
    }
    link->edit_command = msg->command;
    link->edit_seq = seq;
    link->edit_since_ms = now_ms;
    return true;
}

bool Link_SetAlias(LinkClient *link, uint8_t device_id, const char *alias, uint32_t now_ms)
{
    ProtoMessage msg;
    size_t len;

    if ((alias == NULL) || !can_edit(link, device_id))
    {
        return false;
    }
    len = strlen(alias);
    if (len > PROTO_MAX_ALIAS_BYTES)
    {
        return false;
    }
    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_SET_ALIAS;
    msg.u.request.generation = link->shown_generation;
    msg.u.request.device_index = device_id;
    msg.u.request.alias_len = (uint8_t)len;
    memcpy(msg.u.request.alias, alias, len);
    return send_edit(link, &msg, now_ms);
}

bool Link_SetFavorite(LinkClient *link, uint8_t device_id, bool favorite, uint32_t now_ms)
{
    ProtoMessage msg;

    if (!can_edit(link, device_id))
    {
        return false;
    }
    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_SET_FAVORITE;
    msg.u.request.generation = link->shown_generation;
    msg.u.request.device_index = device_id;
    msg.u.request.value = favorite ? 1U : 0U;
    return send_edit(link, &msg, now_ms);
}

bool Link_MoveDevice(LinkClient *link, uint8_t device_id, uint8_t new_position, uint32_t now_ms)
{
    ProtoMessage msg;

    if (!can_edit(link, device_id) || (new_position >= link->shown_count))
    {
        return false;
    }
    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_MOVE_DEVICE;
    msg.u.request.generation = link->shown_generation;
    msg.u.request.device_index = device_id;
    msg.u.request.value = new_position;
    return send_edit(link, &msg, now_ms);
}

bool Link_EditPending(const LinkClient *link)
{
    return link->edit_command != 0U;
}

const ProtoSettings *Link_Settings(const LinkClient *link)
{
    return &link->settings;
}

void Link_SetSettings(LinkClient *link, const ProtoSettings *settings, uint32_t now_ms)
{
    if (!Proto_SettingsValid(settings))
    {
        return;
    }
    apply_settings(link, settings);
    if (link->state != LINK_STATE_READY)
    {
        link->settings_offline = true;
    }
    else if (link->settings_inflight)
    {
        link->settings_resend = true;
    }
    else
    {
        send_settings(link, now_ms);
    }
}

void Link_SetVolume(LinkClient *link, uint8_t volume, uint32_t now_ms)
{
    if ((link->state != LINK_STATE_READY) || (link->host_volume == LINK_VOLUME_UNKNOWN))
    {
        return;
    }
    link->wanted_volume = (volume > 100U) ? 100U : volume;
    link->shown_volume = link->wanted_volume;  /* the slider already shows it */
    link->volume_local = true;
    if (!link->volume_ever_sent || ((now_ms - link->volume_sent_ms) >= LINK_VOLUME_INTERVAL_MS))
    {
        send_volume(link, now_ms);
    }
    else
    {
        link->volume_dirty = true;  /* Link_Poll sends the latest value */
    }
}

void Link_SetMute(LinkClient *link, bool muted)
{
    ProtoMessage msg;

    if ((link->state != LINK_STATE_READY) || (link->host_muted == LINK_VOLUME_UNKNOWN))
    {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    msg.command = PROTO_CMD_SET_MUTE;
    msg.u.muted = muted ? 1U : 0U;
    (void)send(link, &msg);
}
