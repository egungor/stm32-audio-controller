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
 *     and shown only when all `total` entries are there.
 *   - DEVICE_CHANGED sets the active device (applied once a list is shown).
 *   - Link_Select() sends SELECT_DEVICE. Without DEVICE_CHANGED or ERROR
 *     within LINK_RESPONSE_TIMEOUT_MS the host counts as not responding:
 *     notice, waiting screen, and back to requesting the list.
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

    uint8_t  active;        /**< from DEVICE_CHANGED, PROTO_NO_DEVICE if none */
    uint8_t  pending;       /**< SELECT_DEVICE sent, PROTO_NO_DEVICE if none */
    uint8_t  pending_seq;
    uint32_t pending_since_ms;

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

#ifdef __cplusplus
}
#endif

#endif /* LINK_CLIENT_H */
