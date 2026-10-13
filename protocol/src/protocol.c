/**
 * @file    protocol.c
 * @brief   STM32 <-> Windows host protocol, version 1 (see protocol/protocol.md).
 */
#include "protocol.h"

#include <string.h>

#define OFF_MAGIC    0U
#define OFF_VERSION  1U
#define OFF_COMMAND  2U
#define OFF_SEQUENCE 3U
#define OFF_LENGTH   4U
#define OFF_PAYLOAD  PROTO_HEADER_SIZE

#define DEVICE_FIXED_BYTES 7U /* generation .. name_len */

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

/* Field rules shared by encode and decode. The name bytes are checked
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
           ((d->flags & (uint8_t)~PROTO_DEVICE_FLAGS_MASK) == 0U) &&
           (d->icon < PROTO_ICON_COUNT);
}

static int name_has_nul(const uint8_t *name, uint8_t len)
{
    return memchr(name, 0, len) != NULL;
}

/* Check the one-byte payload value of a simple command. */
static int is_valid_byte_value(uint8_t command, uint8_t value)
{
    switch (command)
    {
    case PROTO_CMD_SELECT_DEVICE:
        return value != PROTO_NO_DEVICE;
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
    case PROTO_CMD_SELECT_DEVICE:
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
    case PROTO_CMD_SELECT_DEVICE:
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

ProtoStatus Proto_Encode(const ProtoMessage *msg, uint8_t frame[PROTO_FRAME_SIZE])
{
    uint8_t *payload;
    uint8_t len = 0U;
    uint16_t crc;

    if (frame == NULL)
    {
        return PROTO_ERR_ARG;
    }
    memset(frame, 0, PROTO_FRAME_SIZE);
    payload = &frame[OFF_PAYLOAD];
    if (msg == NULL)
    {
        return PROTO_ERR_ARG;
    }
    if (!is_known_command(msg->command))
    {
        return PROTO_ERR_COMMAND;
    }

    switch (msg->command)
    {
    case PROTO_CMD_GET_DEVICE_LIST:
        break;

    case PROTO_CMD_DEVICE_LIST:
    {
        const ProtoDeviceEntry *d = &msg->u.device;

        if (!is_valid_device(d) || name_has_nul((const uint8_t *)d->name, d->name_len))
        {
            return PROTO_ERR_PAYLOAD;
        }
        payload[0] = d->generation;
        payload[1] = d->total;
        payload[2] = d->position;
        payload[3] = d->device_index;
        payload[4] = d->flags;
        payload[5] = d->icon;
        payload[6] = d->name_len;
        memcpy(&payload[DEVICE_FIXED_BYTES], d->name, d->name_len);
        len = (uint8_t)(DEVICE_FIXED_BYTES + d->name_len);
        break;
    }

    case PROTO_CMD_ERROR:
        if (!is_valid_error_code(msg->u.error.code))
        {
            return PROTO_ERR_PAYLOAD;
        }
        payload[0] = msg->u.error.code;
        payload[1] = msg->u.error.ref_command;
        payload[2] = msg->u.error.ref_sequence;
        len = 3U;
        break;

    default:
    {
        const uint8_t value = byte_value_of(msg);

        if (!is_valid_byte_value(msg->command, value))
        {
            return PROTO_ERR_PAYLOAD;
        }
        payload[0] = value;
        len = 1U;
        break;
    }
    }

    frame[OFF_MAGIC] = PROTO_MAGIC;
    frame[OFF_VERSION] = PROTO_VERSION;
    frame[OFF_COMMAND] = msg->command;
    frame[OFF_SEQUENCE] = msg->sequence;
    frame[OFF_LENGTH] = len;
    crc = Proto_Crc16(frame, PROTO_CRC_OFFSET);
    frame[PROTO_CRC_OFFSET] = (uint8_t)(crc & 0xFFU);
    frame[PROTO_CRC_OFFSET + 1U] = (uint8_t)(crc >> 8);
    return PROTO_OK;
}

ProtoStatus Proto_Decode(const uint8_t frame[PROTO_FRAME_SIZE], ProtoMessage *msg)
{
    const uint8_t *payload;
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

    payload = &frame[OFF_PAYLOAD];
    switch (msg->command)
    {
    case PROTO_CMD_GET_DEVICE_LIST:
        if (len != 0U)
        {
            return PROTO_ERR_PAYLOAD;
        }
        break;

    case PROTO_CMD_DEVICE_LIST:
    {
        ProtoDeviceEntry *d = &msg->u.device;

        if (len < DEVICE_FIXED_BYTES)
        {
            return PROTO_ERR_PAYLOAD;
        }
        d->generation = payload[0];
        d->total = payload[1];
        d->position = payload[2];
        d->device_index = payload[3];
        d->flags = payload[4];
        d->icon = payload[5];
        d->name_len = payload[6];
        if (!is_valid_device(d) || (len != DEVICE_FIXED_BYTES + d->name_len) ||
            name_has_nul(&payload[DEVICE_FIXED_BYTES], d->name_len))
        {
            ProtoMessage cleared = { 0 };

            cleared.command = msg->command;
            cleared.sequence = msg->sequence;
            *msg = cleared;
            return PROTO_ERR_PAYLOAD;
        }
        memcpy(d->name, &payload[DEVICE_FIXED_BYTES], d->name_len);
        d->name[d->name_len] = '\0';
        break;
    }

    case PROTO_CMD_ERROR:
        if ((len != 3U) || !is_valid_error_code(payload[0]))
        {
            return PROTO_ERR_PAYLOAD;
        }
        msg->u.error.code = payload[0];
        msg->u.error.ref_command = payload[1];
        msg->u.error.ref_sequence = payload[2];
        break;

    default:
        if ((len != 1U) || !is_valid_byte_value(msg->command, payload[0]))
        {
            return PROTO_ERR_PAYLOAD;
        }
        set_byte_value(msg, payload[0]);
        break;
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
