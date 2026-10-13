/**
 * @file    link_client.h
 * @brief   STM32 side of protocol v1 (protocol/protocol.md, "Exchanges").
 *
 * Hardware independent: frames go out through a callback and come in
 * through Link_OnFrame(); the UI is driven through callbacks. Time is
 * passed in by the caller (milliseconds, wrapping allowed). The same code
 * runs in the firmware and in the Linux unit tests.
 *
 * Behaviour:
 *   - After USB is configured, GET_DEVICE_LIST is sent at once and again
 *     every LINK_RETRY_MS until a complete device list has arrived.
 *   - A list is assembled from the DEVICE_LIST frames of one generation
 *     and shown only when all `total` entries are there. If a new list
 *     stays incomplete for LINK_RETRY_MS after its last entry while an
 *     older list is shown (a frame was lost), GET_DEVICE_LIST is sent
 *     again; the older list stays on screen meanwhile.
 *   - DEVICE_CHANGED sets the active device (applied once a list is shown).
 *   - Link_Select() sends SELECT_DEVICE. Without DEVICE_CHANGED or ERROR
 *     within LINK_RESPONSE_TIMEOUT_MS the host counts as not responding:
 *     notice, waiting screen, and back to requesting the list.
 *   - VOLUME_CHANGED / MUTE_CHANGED are shown; DEVICE_CHANGED "none"
 *     makes them unknown (controls disabled) until the host reports them
 *     again.
 *   - Link_SetVolume() sends SET_VOLUME at most every
 *     LINK_VOLUME_INTERVAL_MS with the latest value (the last value is
 *     always sent). Host volume reports are not shown while a local change
 *     is in progress and for LINK_VOLUME_SETTLE_MS after the last
 *     SET_VOLUME; then the host's value is shown.
 *   - Link_SetMute() sends SET_MUTE; the mute state shown changes only
 *     with MUTE_CHANGED.
 *   - An invalid frame is answered with ERROR (never an ERROR frame).
 *
 * Not thread-safe: call everything from one task.
 */
#ifndef LINK_CLIENT_H
#define LINK_CLIENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "protocol.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_RETRY_MS             1000U
#define LINK_RESPONSE_TIMEOUT_MS  2000U
#define LINK_VOLUME_INTERVAL_MS   100U
#define LINK_VOLUME_SETTLE_MS     300U
#define LINK_VOLUME_UNKNOWN       0xFFU  /**< volume or mute not reported */

typedef enum
{
    LINK_NOTICE_SELECT_FAILED,   /**< host answered ERROR to SELECT_DEVICE */
    LINK_NOTICE_HOST_TIMEOUT,    /**< no answer to SELECT_DEVICE in time */
} LinkNotice;

typedef struct
{
    /** Send one encoded frame; return false if it could not be queued. */
    bool (*send_frame)(const uint8_t frame[PROTO_FRAME_SIZE], void *ctx);
    void (*show_waiting)(void *ctx);
    void (*show_devices)(const UiDevice *devices, size_t count, void *ctx);
    void (*set_active)(uint8_t device_id, void *ctx);
    void (*set_pending)(uint8_t device_id, void *ctx);
    void (*notice)(LinkNotice notice, void *ctx);
    /** Volume 0-100 of the default device, or LINK_VOLUME_UNKNOWN. */
    void (*set_volume)(uint8_t volume, void *ctx);
    /** Mute 0/1 of the default device, or LINK_VOLUME_UNKNOWN. */
    void (*set_muted)(uint8_t muted, void *ctx);
    void *ctx;
} LinkCallbacks;

typedef enum
{
    LINK_STATE_DISCONNECTED,
    LINK_STATE_REQUESTING,  /**< waiting for a complete device list */
    LINK_STATE_READY,
} LinkState;

typedef struct
{
    LinkCallbacks cb;
    LinkState     state;
    uint8_t       sequence;
    uint32_t      last_request_ms;

    /* List being assembled. */
    ProtoDeviceEntry entries[PROTO_MAX_DEVICES];
    uint32_t         received_mask;
    uint8_t          generation;
    uint8_t          total;
    bool             assembling;
    uint32_t         last_entry_ms;  /**< last DEVICE_LIST frame */

    uint8_t  active;        /**< from DEVICE_CHANGED, PROTO_NO_DEVICE if none */
    uint8_t  pending;       /**< SELECT_DEVICE sent, PROTO_NO_DEVICE if none */
    uint8_t  pending_seq;
    uint32_t pending_since_ms;

    /* Volume and mute. */
    uint8_t  host_volume;      /**< last VOLUME_CHANGED, or LINK_VOLUME_UNKNOWN */
    uint8_t  host_muted;       /**< last MUTE_CHANGED, or LINK_VOLUME_UNKNOWN */
    uint8_t  shown_volume;     /**< last value passed to set_volume */
    uint8_t  wanted_volume;    /**< last Link_SetVolume() value */
    bool     volume_local;     /**< local change in progress or settling */
    bool     volume_dirty;     /**< wanted_volume not sent yet */
    bool     volume_ever_sent;
    uint32_t volume_sent_ms;   /**< last SET_VOLUME */

    uint32_t frames_rejected;
    uint32_t send_failures;
} LinkClient;

void Link_Init(LinkClient *link, const LinkCallbacks *callbacks);

/** USB configured by the host. */
void Link_OnConnected(LinkClient *link, uint32_t now_ms);

/** USB unplugged, reset or suspended. */
void Link_OnDisconnected(LinkClient *link);

/** A frame arrived from the host. */
void Link_OnFrame(LinkClient *link, const uint8_t frame[PROTO_FRAME_SIZE], uint32_t now_ms);

/** Call regularly (e.g. every UI loop); handles retries and timeouts. */
void Link_Poll(LinkClient *link, uint32_t now_ms);

/** The user chose a device in the UI. Ignored unless READY. */
void Link_Select(LinkClient *link, uint8_t device_id, uint32_t now_ms);

/** The user moved the volume slider (0-100). Ignored unless READY with a
 *  known volume. */
void Link_SetVolume(LinkClient *link, uint8_t volume, uint32_t now_ms);

/** The user toggled mute. Ignored unless READY with a known mute state. */
void Link_SetMute(LinkClient *link, bool muted);

#ifdef __cplusplus
}
#endif

#endif /* LINK_CLIENT_H */
