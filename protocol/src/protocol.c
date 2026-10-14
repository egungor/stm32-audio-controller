/**
 * @file    protocol.c
 * @brief   STM32 <-> Windows host protocol, version 2 (see protocol/protocol.md).
 */
#include "protocol.h"

#include <string.h>

#define OFF_MAGIC    0U
#define OFF_VERSION  1U
#define OFF_COMMAND  2U
#define OFF_SEQUENCE 3U
#define OFF_LENGTH   4U
#define OFF_PAYLOAD  PROTO_HEADER_SIZE

#define DEVICE_FIXED_BYTES    7U /* generation .. name_len */
#define ALIAS_FIXED_BYTES     3U /* generation, position, alias_len */
#define SET_ALIAS_FIXED_BYTES 3U /* generation, device_index, alias_len */

uint16_t Proto_Crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFFU;

    for (size_t i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (unsigned bit = 0U; bit < 8U; bit++)
        {
            const unsigned msb = (unsigned)crc & 0x8000U;

            crc = (uint16_t)((unsigned)crc << 1);
            if (msb != 0U)
            {
                crc = (uint16_t)((unsigned)crc ^ 0x1021U);
            }
        }
    }
    return crc;
}

static int is_known_command(uint8_t command)
{
    switch (command)
    {
    case PROTO_CMD_GET_DEVICE_LIST:
    case PROTO_CMD_DEVICE_LIST:
    case PROTO_CMD_SELECT_DEVICE:
    case PROTO_CMD_DEVICE_CHANGED:
    case PROTO_CMD_SET_VOLUME:
    case PROTO_CMD_VOLUME_CHANGED:
    case PROTO_CMD_SET_MUTE:
    case PROTO_CMD_MUTE_CHANGED:
    case PROTO_CMD_DEVICE_ALIAS:
    case PROTO_CMD_SET_ALIAS:
    case PROTO_CMD_SET_FAVORITE:
    case PROTO_CMD_MOVE_DEVICE:
    case PROTO_CMD_SETTINGS:
    case PROTO_CMD_SET_SETTINGS:
    case PROTO_CMD_ERROR:
        return 1;
    default:
        return 0;
    }
}

static int is_valid_error_code(uint8_t code)
{
    return (code >= (uint8_t)PROTO_ERROR_BAD_FRAME) && (code <= (uint8_t)PROTO_ERROR_BUSY);
}

/* Field rules shared by encode and decode. Text bytes are checked
 * separately because they live in different buffers. */
static int is_valid_device(const ProtoDeviceEntry *d)
{
    if ((d->total > PROTO_MAX_DEVICES) || (d->name_len > PROTO_MAX_NAME_BYTES))
    {
        return 0;
    }
    if (d->total == 0U)
    {
        /* Empty list: one frame, no device in it. */
        return (d->position == 0U) && (d->device_index == PROTO_NO_DEVICE) &&
               (d->flags == 0U) && (d->icon == 0U) && (d->name_len == 0U);
    }
    return (d->position < d->total) && (d->device_index != PROTO_NO_DEVICE) &&
           (((unsigned)d->flags & ~PROTO_DEVICE_FLAGS_MASK) == 0U) &&
           (d->icon < PROTO_ICON_COUNT);
}

static int is_valid_alias_entry(const ProtoAliasEntry *a)
{
    return (a->position < PROTO_MAX_DEVICES) && (a->alias_len >= 1U) &&
           (a->alias_len <= PROTO_MAX_ALIAS_BYTES);
}

/* SELECT_DEVICE, SET_FAVORITE, MOVE_DEVICE, SET_ALIAS. */
static int is_valid_request(uint8_t command, const ProtoDeviceRequest *r)
{
    if (r->device_index == PROTO_NO_DEVICE)
    {
        return 0;
    }
    switch (command)
    {
    case PROTO_CMD_SET_FAVORITE:
        return r->value <= 1U;
    case PROTO_CMD_MOVE_DEVICE:
        return r->value < PROTO_MAX_DEVICES;
    case PROTO_CMD_SET_ALIAS:
        return r->alias_len <= PROTO_MAX_ALIAS_BYTES;
    default:
        return 1; /* SELECT_DEVICE */
    }
}

static int has_nul(const uint8_t *text, uint8_t len)
{
    return memchr(text, 0, len) != NULL;
}

/* Check the one-byte payload value of a simple command. */
static int is_valid_byte_value(uint8_t command, uint8_t value)
{
    switch (command)
    {
    case PROTO_CMD_DEVICE_CHANGED:
        return 1; /* PROTO_NO_DEVICE: no default device */
    case PROTO_CMD_SET_VOLUME:
    case PROTO_CMD_VOLUME_CHANGED:
        return value <= PROTO_MAX_VOLUME;
    case PROTO_CMD_SET_MUTE:
    case PROTO_CMD_MUTE_CHANGED:
        return value <= 1U;
    default:
        return 0;
    }
}

static uint8_t byte_value_of(const ProtoMessage *msg)
{
    switch (msg->command)
    {
    case PROTO_CMD_DEVICE_CHANGED:
        return msg->u.device_index;
    case PROTO_CMD_SET_VOLUME:
    case PROTO_CMD_VOLUME_CHANGED:
        return msg->u.volume;
    default:
        return msg->u.muted;
    }
}

static void set_byte_value(ProtoMessage *msg, uint8_t value)
{
    switch (msg->command)
    {
    case PROTO_CMD_DEVICE_CHANGED:
        msg->u.device_index = value;
        break;
    case PROTO_CMD_SET_VOLUME:
    case PROTO_CMD_VOLUME_CHANGED:
        msg->u.volume = value;
        break;
    default:
        msg->u.muted = value;
        break;
    }
}

/* Settings record <-> bytes, in the order of protocol.md. */
static void settings_to_bytes(const ProtoSettings *s, uint8_t *out)
{
    out[0] = s->layout;
    out[1] = s->theme;
    out[2] = s->accent;
    out[3] = s->grid;
    out[4] = s->startup_tab;
    out[5] = s->last_tab;
    out[6] = s->screen_timeout;
    out[7] = s->flipped;
    out[8] = s->language;
    out[9] = s->volume_limit_on;
    out[10] = s->volume_limit;
}

static void settings_from_bytes(const uint8_t *in, ProtoSettings *s)
{
    s->layout = in[0];
    s->theme = in[1];
    s->accent = in[2];
    s->grid = in[3];
    s->startup_tab = in[4];
    s->last_tab = in[5];
    s->screen_timeout = in[6];
    s->flipped = in[7];
    s->language = in[8];
    s->volume_limit_on = in[9];
    s->volume_limit = in[10];
}

void Proto_DefaultSettings(ProtoSettings *settings)
{
    if (settings != NULL)
    {
        memset(settings, 0, sizeof(*settings));
        settings->volume_limit = PROTO_DEFAULT_VOLUME_LIMIT;
    }
}

int Proto_SettingsValid(const ProtoSettings *s)
{
    return (s != NULL) && (s->layout < PROTO_LAYOUT_COUNT) && (s->theme < PROTO_THEME_COUNT) &&
           (s->accent < PROTO_ACCENT_COUNT) && (s->grid < PROTO_GRID_COUNT) &&
           (s->startup_tab < PROTO_STARTUP_TAB_COUNT) && (s->last_tab < PROTO_TAB_COUNT) &&
           (s->screen_timeout <= PROTO_MAX_SCREEN_TIMEOUT) && (s->flipped <= 1U) &&
           (s->language < PROTO_LANGUAGE_COUNT) && (s->volume_limit_on <= 1U) &&
           (s->volume_limit <= PROTO_MAX_VOLUME);
}

/* Write the payload of @p msg; return its length, or -1 if a field is
 * invalid. */
static int encode_payload(const ProtoMessage *msg, uint8_t *payload)
{
    switch (msg->command)
    {
    case PROTO_CMD_GET_DEVICE_LIST:
        return 0;

    case PROTO_CMD_DEVICE_LIST:
    {
        const ProtoDeviceEntry *d = &msg->u.device;

        if (!is_valid_device(d) || has_nul((const uint8_t *)d->name, d->name_len))
        {
            return -1;
        }
        payload[0] = d->generation;
        payload[1] = d->total;
        payload[2] = d->position;
        payload[3] = d->device_index;
        payload[4] = d->flags;
        payload[5] = d->icon;
        payload[6] = d->name_len;
        memcpy(&payload[DEVICE_FIXED_BYTES], d->name, d->name_len);
        return (int)(DEVICE_FIXED_BYTES + d->name_len);
    }

    case PROTO_CMD_DEVICE_ALIAS:
    {
        const ProtoAliasEntry *a = &msg->u.alias;

        if (!is_valid_alias_entry(a) || has_nul((const uint8_t *)a->alias, a->alias_len))
        {
            return -1;
        }
        payload[0] = a->generation;
        payload[1] = a->position;
        payload[2] = a->alias_len;
        memcpy(&payload[ALIAS_FIXED_BYTES], a->alias, a->alias_len);
        return (int)(ALIAS_FIXED_BYTES + a->alias_len);
    }

    case PROTO_CMD_SELECT_DEVICE:
    case PROTO_CMD_SET_FAVORITE:
    case PROTO_CMD_MOVE_DEVICE:
    {
        const ProtoDeviceRequest *r = &msg->u.request;

        if (!is_valid_request(msg->command, r))
        {
            return -1;
        }
        payload[0] = r->generation;
        payload[1] = r->device_index;
        if (msg->command == PROTO_CMD_SELECT_DEVICE)
        {
            return 2;
        }
        payload[2] = r->value;
        return 3;
    }

    case PROTO_CMD_SET_ALIAS:
    {
        const ProtoDeviceRequest *r = &msg->u.request;

        if (!is_valid_request(msg->command, r) || has_nul((const uint8_t *)r->alias, r->alias_len))
        {
            return -1;
        }
        payload[0] = r->generation;
        payload[1] = r->device_index;
        payload[2] = r->alias_len;
        memcpy(&payload[SET_ALIAS_FIXED_BYTES], r->alias, r->alias_len);
        return (int)(SET_ALIAS_FIXED_BYTES + r->alias_len);
    }

    case PROTO_CMD_SETTINGS:
    case PROTO_CMD_SET_SETTINGS:
        if (!Proto_SettingsValid(&msg->u.settings))
        {
            return -1;
        }
        settings_to_bytes(&msg->u.settings, payload);
        return (int)PROTO_SETTINGS_BYTES;

    case PROTO_CMD_ERROR:
        if (!is_valid_error_code(msg->u.error.code))
        {
            return -1;
        }
        payload[0] = msg->u.error.code;
        payload[1] = msg->u.error.ref_command;
        payload[2] = msg->u.error.ref_sequence;
        return 3;

    default:
    {
        const uint8_t value = byte_value_of(msg);

        if (!is_valid_byte_value(msg->command, value))
        {
            return -1;
        }
        payload[0] = value;
        return 1;
    }
    }
}

ProtoStatus Proto_Encode(const ProtoMessage *msg, uint8_t frame[PROTO_FRAME_SIZE])
{
    int len;
    uint16_t crc;

    if (frame == NULL)
    {
        return PROTO_ERR_ARG;
    }
    memset(frame, 0, PROTO_FRAME_SIZE);
    if (msg == NULL)
    {
        return PROTO_ERR_ARG;
    }
    if (!is_known_command(msg->command))
    {
        return PROTO_ERR_COMMAND;
    }
    len = encode_payload(msg, &frame[OFF_PAYLOAD]);
    if (len < 0)
    {
        memset(frame, 0, PROTO_FRAME_SIZE);
        return PROTO_ERR_PAYLOAD;
    }

    frame[OFF_MAGIC] = PROTO_MAGIC;
    frame[OFF_VERSION] = PROTO_VERSION;
    frame[OFF_COMMAND] = msg->command;
    frame[OFF_SEQUENCE] = msg->sequence;
    frame[OFF_LENGTH] = (uint8_t)len;
    crc = Proto_Crc16(frame, PROTO_CRC_OFFSET);
    frame[PROTO_CRC_OFFSET] = (uint8_t)(crc & 0xFFU);
    frame[PROTO_CRC_OFFSET + 1U] = (uint8_t)(crc >> 8);
    return PROTO_OK;
}

/* Read the payload of a frame whose header passed; return 1 if it is
 * valid for msg->command. */
static int decode_payload(const uint8_t *payload, uint8_t len, ProtoMessage *msg)
{
    switch (msg->command)
    {
    case PROTO_CMD_GET_DEVICE_LIST:
        return len == 0U;

    case PROTO_CMD_DEVICE_LIST:
    {
        ProtoDeviceEntry *d = &msg->u.device;

        if (len < DEVICE_FIXED_BYTES)
        {
            return 0;
        }
        d->generation = payload[0];
        d->total = payload[1];
        d->position = payload[2];
        d->device_index = payload[3];
        d->flags = payload[4];
        d->icon = payload[5];
        d->name_len = payload[6];
        if (!is_valid_device(d) || (len != DEVICE_FIXED_BYTES + d->name_len) ||
            has_nul(&payload[DEVICE_FIXED_BYTES], d->name_len))
        {
            return 0;
        }
        memcpy(d->name, &payload[DEVICE_FIXED_BYTES], d->name_len);
        d->name[d->name_len] = '\0';
        return 1;
    }

    case PROTO_CMD_DEVICE_ALIAS:
    {
        ProtoAliasEntry *a = &msg->u.alias;

        if (len < ALIAS_FIXED_BYTES)
        {
            return 0;
        }
        a->generation = payload[0];
        a->position = payload[1];
        a->alias_len = payload[2];
        if (!is_valid_alias_entry(a) || (len != ALIAS_FIXED_BYTES + a->alias_len) ||
            has_nul(&payload[ALIAS_FIXED_BYTES], a->alias_len))
        {
            return 0;
        }
        memcpy(a->alias, &payload[ALIAS_FIXED_BYTES], a->alias_len);
        a->alias[a->alias_len] = '\0';
        return 1;
    }

    case PROTO_CMD_SELECT_DEVICE:
    case PROTO_CMD_SET_FAVORITE:
    case PROTO_CMD_MOVE_DEVICE:
    {
        ProtoDeviceRequest *r = &msg->u.request;
        const uint8_t want = (msg->command == PROTO_CMD_SELECT_DEVICE) ? 2U : 3U;

        if (len != want)
        {
            return 0;
        }
        r->generation = payload[0];
        r->device_index = payload[1];
        r->value = (want == 3U) ? payload[2] : 0U;
        return is_valid_request(msg->command, r);
    }

    case PROTO_CMD_SET_ALIAS:
    {
        ProtoDeviceRequest *r = &msg->u.request;

        if (len < SET_ALIAS_FIXED_BYTES)
        {
            return 0;
        }
        r->generation = payload[0];
        r->device_index = payload[1];
        r->alias_len = payload[2];
        if (!is_valid_request(msg->command, r) || (len != SET_ALIAS_FIXED_BYTES + r->alias_len) ||
            has_nul(&payload[SET_ALIAS_FIXED_BYTES], r->alias_len))
        {
            return 0;
        }
        memcpy(r->alias, &payload[SET_ALIAS_FIXED_BYTES], r->alias_len);
        r->alias[r->alias_len] = '\0';
        return 1;
    }

    case PROTO_CMD_SETTINGS:
    case PROTO_CMD_SET_SETTINGS:
        if (len != PROTO_SETTINGS_BYTES)
        {
            return 0;
        }
        settings_from_bytes(payload, &msg->u.settings);
        return Proto_SettingsValid(&msg->u.settings);

    case PROTO_CMD_ERROR:
        if ((len != 3U) || !is_valid_error_code(payload[0]))
        {
            return 0;
        }
        msg->u.error.code = payload[0];
        msg->u.error.ref_command = payload[1];
        msg->u.error.ref_sequence = payload[2];
        return 1;

    default:
        if ((len != 1U) || !is_valid_byte_value(msg->command, payload[0]))
        {
            return 0;
        }
        set_byte_value(msg, payload[0]);
        return 1;
    }
}

ProtoStatus Proto_Decode(const uint8_t frame[PROTO_FRAME_SIZE], ProtoMessage *msg)
{
    uint8_t len;
    uint16_t crc;

    if (msg == NULL)
    {
        return PROTO_ERR_ARG;
    }
    memset(msg, 0, sizeof(*msg));
    if (frame == NULL)
    {
        return PROTO_ERR_ARG;
    }

    /* Header bytes are reported even for a rejected frame so the receiver
     * can reference them in an ERROR reply. */
    msg->command = frame[OFF_COMMAND];
    msg->sequence = frame[OFF_SEQUENCE];

    if (frame[OFF_MAGIC] != PROTO_MAGIC)
    {
        return PROTO_ERR_MAGIC;
    }
    if (frame[OFF_VERSION] != PROTO_VERSION)
    {
        return PROTO_ERR_VERSION;
    }
    crc = (uint16_t)(frame[PROTO_CRC_OFFSET] | ((uint16_t)frame[PROTO_CRC_OFFSET + 1U] << 8));
    if (crc != Proto_Crc16(frame, PROTO_CRC_OFFSET))
    {
        return PROTO_ERR_CRC;
    }
    len = frame[OFF_LENGTH];
    if (len > PROTO_MAX_PAYLOAD)
    {
        return PROTO_ERR_LENGTH;
    }
    if (!is_known_command(msg->command))
    {
        return PROTO_ERR_COMMAND;
    }
    if (!decode_payload(&frame[OFF_PAYLOAD], len, msg))
    {
        const uint8_t command = msg->command;
        const uint8_t sequence = msg->sequence;

        memset(msg, 0, sizeof(*msg));
        msg->command = command;
        msg->sequence = sequence;
        return PROTO_ERR_PAYLOAD;
    }
    return PROTO_OK;
}

ProtoErrorCode Proto_ErrorCodeFor(ProtoStatus status)
{
    switch (status)
    {
    case PROTO_ERR_VERSION:
        return PROTO_ERROR_UNSUPPORTED_VERSION;
    case PROTO_ERR_COMMAND:
        return PROTO_ERROR_UNKNOWN_COMMAND;
    case PROTO_ERR_PAYLOAD:
        return PROTO_ERROR_BAD_PAYLOAD;
    default:
        return PROTO_ERROR_BAD_FRAME;
    }
}
