/**
 * @file    protocol.h
 * @brief   STM32 <-> Windows host protocol, version 2 (see protocol/protocol.md).
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
#define PROTO_VERSION      2U
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
/** Largest device alias, in bytes of UTF-8, without terminator. */
#define PROTO_MAX_ALIAS_BYTES  32U

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
    PROTO_CMD_DEVICE_ALIAS    = 0x09, /**< host -> STM32, part of a device list */
    PROTO_CMD_SET_ALIAS       = 0x0A, /**< STM32 -> host */
    PROTO_CMD_SET_FAVORITE    = 0x0B, /**< STM32 -> host */
    PROTO_CMD_MOVE_DEVICE     = 0x0C, /**< STM32 -> host */
    PROTO_CMD_SETTINGS        = 0x0D, /**< host -> STM32 */
    PROTO_CMD_SET_SETTINGS    = 0x0E, /**< STM32 -> host */
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
#define PROTO_DEVICE_FLAG_DEFAULT  0x01U /**< current default playback device */
#define PROTO_DEVICE_FLAG_FAVORITE 0x02U /**< marked as favorite by the user */
#define PROTO_DEVICE_FLAG_ALIAS    0x04U /**< a DEVICE_ALIAS frame follows for this entry */
#define PROTO_DEVICE_FLAGS_MASK    0x07U

/** Settings record field values (SETTINGS, SET_SETTINGS). */
#define PROTO_LAYOUT_COUNT       3U  /**< 0 cards, 1 list, 2 large tile */
#define PROTO_THEME_COUNT        2U  /**< 0 dark, 1 light */
#define PROTO_ACCENT_COUNT       6U  /**< 0 orange .. 5 yellow */
#define PROTO_GRID_COUNT         4U  /**< 0 2x2, 1 2x3, 2 3x3, 3 list */
#define PROTO_STARTUP_TAB_COUNT  3U  /**< 0 last used, 1 all, 2 favorites */
#define PROTO_TAB_COUNT          2U  /**< 0 all, 1 favorites */
#define PROTO_MAX_SCREEN_TIMEOUT 60U /**< minutes; 0 = never */
#define PROTO_LANGUAGE_COUNT     2U  /**< 0 Turkish, 1 English */
#define PROTO_SETTINGS_BYTES     11U
#define PROTO_DEFAULT_VOLUME_LIMIT 50U

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

/** DEVICE_ALIAS payload: the alias of one entry of the list being sent. */
typedef struct
{
    uint8_t generation; /**< generation of the list it belongs to */
    uint8_t position;   /**< entry position in that list */
    uint8_t alias_len;  /**< 1 .. PROTO_MAX_ALIAS_BYTES */
    char    alias[PROTO_MAX_ALIAS_BYTES + 1U]; /**< UTF-8, NUL-terminated after decode */
} ProtoAliasEntry;

/** SELECT_DEVICE, SET_FAVORITE, MOVE_DEVICE, SET_ALIAS: a request about one
 *  device of the list with @p generation. */
typedef struct
{
    uint8_t generation;
    uint8_t device_index;
    uint8_t value;      /**< SET_FAVORITE: 0/1; MOVE_DEVICE: new position
                             (< PROTO_MAX_DEVICES); SELECT_DEVICE: 0 */
    uint8_t alias_len;  /**< SET_ALIAS only: 0 (remove) .. PROTO_MAX_ALIAS_BYTES */
    char    alias[PROTO_MAX_ALIAS_BYTES + 1U]; /**< SET_ALIAS only */
} ProtoDeviceRequest;

/** UI settings record, stored by the host (ADR-004). */
typedef struct
{
    uint8_t layout;          /**< < PROTO_LAYOUT_COUNT */
    uint8_t theme;           /**< < PROTO_THEME_COUNT */
    uint8_t accent;          /**< < PROTO_ACCENT_COUNT */
    uint8_t grid;            /**< < PROTO_GRID_COUNT */
    uint8_t startup_tab;     /**< < PROTO_STARTUP_TAB_COUNT */
    uint8_t last_tab;        /**< < PROTO_TAB_COUNT */
    uint8_t screen_timeout;  /**< minutes, 0 .. PROTO_MAX_SCREEN_TIMEOUT */
    uint8_t flipped;         /**< 0 or 1 */
    uint8_t language;        /**< < PROTO_LANGUAGE_COUNT */
    uint8_t volume_limit_on; /**< 0 or 1 */
    uint8_t volume_limit;    /**< 0 .. PROTO_MAX_VOLUME */
} ProtoSettings;

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
        ProtoDeviceEntry   device;       /**< DEVICE_LIST */
        ProtoAliasEntry    alias;        /**< DEVICE_ALIAS */
        ProtoDeviceRequest request;      /**< SELECT_DEVICE, SET_ALIAS, SET_FAVORITE, MOVE_DEVICE */
        ProtoSettings      settings;     /**< SETTINGS, SET_SETTINGS */
        uint8_t            device_index; /**< DEVICE_CHANGED */
        uint8_t            volume;       /**< SET_VOLUME, VOLUME_CHANGED: 0 .. 100 */
        uint8_t            muted;        /**< SET_MUTE, MUTE_CHANGED: 0 or 1 */
        ProtoError         error;        /**< ERROR */
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

/** The settings a board uses until the host sends its own (ADR-004). */
void Proto_DefaultSettings(ProtoSettings *settings);

/** 1 if every field of @p settings is in range, else 0. */
int Proto_SettingsValid(const ProtoSettings *settings);

#ifdef __cplusplus
}
#endif

#endif /* PROTOCOL_H */
