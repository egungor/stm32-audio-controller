/**
 * @file    protocol.h
 * @brief   STM32 <-> Windows host protocol, version 1 (see protocol/protocol.md).
 *
 * Portable C99 codec shared by the firmware and the host. Every message is
 * one fixed-size frame of PROTO_FRAME_SIZE bytes:
 *
 *     offset  size  field
 *     0       1     magic      (PROTO_MAGIC)
 *     1       1     version    (PROTO_VERSION)
 *     2       1     command    (ProtoCommand)
 *     3       1     sequence   (sender's counter, wraps)
 *     4       1     length     (payload bytes, 0 .. PROTO_MAX_PAYLOAD)
 *     5       len   payload
 *     ..      ..    zero padding
 *     62      2     CRC-16/CCITT-FALSE of bytes 0..61, little endian
 *
 * No dynamic allocation, no platform headers. Decoding never reads or
 * writes outside the frame and the output structure.
 */
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PROTO_FRAME_SIZE   64U
#define PROTO_MAGIC        0xA5U
#define PROTO_VERSION      1U
#define PROTO_HEADER_SIZE  5U
#define PROTO_CRC_OFFSET   62U
#define PROTO_MAX_PAYLOAD  (PROTO_CRC_OFFSET - PROTO_HEADER_SIZE) /* 57 */

/** Largest number of devices in one device list. */
#define PROTO_MAX_DEVICES      32U
/** Largest device name, in bytes of UTF-8, without terminator. */
#define PROTO_MAX_NAME_BYTES   48U
/** device_index value meaning "no device". */
#define PROTO_NO_DEVICE        0xFFU
/** Largest volume value (percent). */
#define PROTO_MAX_VOLUME       100U

typedef enum
{
    PROTO_CMD_GET_DEVICE_LIST = 0x01, /**< STM32 -> host */
    PROTO_CMD_DEVICE_LIST     = 0x02, /**< host -> STM32, one device per frame */
    PROTO_CMD_SELECT_DEVICE   = 0x03, /**< STM32 -> host */
    PROTO_CMD_DEVICE_CHANGED  = 0x04, /**< host -> STM32 */
    PROTO_CMD_SET_VOLUME      = 0x05, /**< STM32 -> host */
    PROTO_CMD_VOLUME_CHANGED  = 0x06, /**< host -> STM32 */
    PROTO_CMD_SET_MUTE        = 0x07, /**< STM32 -> host */
    PROTO_CMD_MUTE_CHANGED    = 0x08, /**< host -> STM32 */
    PROTO_CMD_ERROR           = 0x7F  /**< either direction */
} ProtoCommand;

/** Device icon hint; the firmware picks the picture. */
typedef enum
{
    PROTO_ICON_GENERIC    = 0,
    PROTO_ICON_SPEAKERS   = 1,
    PROTO_ICON_HEADPHONES = 2,
    PROTO_ICON_HEADSET    = 3,
    PROTO_ICON_DISPLAY    = 4,
    PROTO_ICON_DIGITAL    = 5
} ProtoIcon;
#define PROTO_ICON_COUNT 6U

/** DEVICE_LIST flags. */
#define PROTO_DEVICE_FLAG_DEFAULT 0x01U /**< current default playback device */
#define PROTO_DEVICE_FLAGS_MASK   0x01U

/** ERROR codes. */
typedef enum
{
    PROTO_ERROR_BAD_FRAME           = 0x01, /**< magic, CRC or length wrong */
    PROTO_ERROR_UNSUPPORTED_VERSION = 0x02,
    PROTO_ERROR_UNKNOWN_COMMAND     = 0x03,
    PROTO_ERROR_BAD_PAYLOAD         = 0x04,
    PROTO_ERROR_UNKNOWN_DEVICE      = 0x05, /**< device_index not in the current list */
    PROTO_ERROR_OPERATION_FAILED    = 0x06, /**< receiver could not carry out the request */
    PROTO_ERROR_BUSY                = 0x07
} ProtoErrorCode;

/** Result of encoding or decoding. */
typedef enum
{
    PROTO_OK = 0,
    PROTO_ERR_ARG,      /**< NULL pointer or invalid field in the message to encode */
    PROTO_ERR_MAGIC,
    PROTO_ERR_VERSION,
    PROTO_ERR_CRC,
    PROTO_ERR_LENGTH,   /**< length byte above PROTO_MAX_PAYLOAD */
    PROTO_ERR_COMMAND,  /**< unknown command */
    PROTO_ERR_PAYLOAD   /**< payload size or a field value invalid for the command */
} ProtoStatus;

/** One entry of a device list (DEVICE_LIST payload). */
typedef struct
{
    uint8_t generation;   /**< same for every frame of one list; changes per new list */
    uint8_t total;        /**< devices in the list, 0 .. PROTO_MAX_DEVICES */
    uint8_t position;     /**< 0 .. total-1; 0 when total is 0 */
    uint8_t device_index; /**< host-assigned id, valid until the next list */
    uint8_t flags;        /**< PROTO_DEVICE_FLAG_* */
    uint8_t icon;         /**< ProtoIcon */
    uint8_t name_len;     /**< bytes in name, 0 .. PROTO_MAX_NAME_BYTES */
    char    name[PROTO_MAX_NAME_BYTES + 1U]; /**< UTF-8, NUL-terminated after decode */
} ProtoDeviceEntry;

typedef struct
{
    uint8_t code;        /**< ProtoErrorCode */
    uint8_t ref_command; /**< command being answered, 0 if none */
    uint8_t ref_sequence;
} ProtoError;

/** A decoded or to-be-encoded message. */
typedef struct
{
    uint8_t command;  /**< ProtoCommand */
    uint8_t sequence;
    union
    {
        ProtoDeviceEntry device;       /**< DEVICE_LIST */
        uint8_t          device_index; /**< SELECT_DEVICE, DEVICE_CHANGED */
        uint8_t          volume;       /**< SET_VOLUME, VOLUME_CHANGED: 0 .. 100 */
        uint8_t          muted;        /**< SET_MUTE, MUTE_CHANGED: 0 or 1 */
        ProtoError       error;        /**< ERROR */
    } u;
} ProtoMessage;

/** CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout. */
uint16_t Proto_Crc16(const uint8_t *data, size_t len);

/**
 * @brief  Validate @p msg and write it as one frame.
 * @return PROTO_OK, PROTO_ERR_ARG, PROTO_ERR_COMMAND or PROTO_ERR_PAYLOAD.
 *         On error @p frame is left all zero.
 */
ProtoStatus Proto_Encode(const ProtoMessage *msg, uint8_t frame[PROTO_FRAME_SIZE]);

/**
 * @brief  Check and decode one received frame.
 *
 * Checks, in order: magic, version, CRC, length, command, payload. On any
 * error @p msg is cleared except command and sequence, which hold the
 * received bytes when the header could be read (for an ERROR reply).
 */
ProtoStatus Proto_Decode(const uint8_t frame[PROTO_FRAME_SIZE], ProtoMessage *msg);

/** Map a decode failure to the ERROR code to send back. */
ProtoErrorCode Proto_ErrorCodeFor(ProtoStatus status);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
