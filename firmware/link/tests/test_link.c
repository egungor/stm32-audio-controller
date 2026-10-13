/**
 * @file    test_link.c
 * @brief   Unit tests for the STM32 protocol client (no test framework).
 */
#include "link_client.h"

#include <stdio.h>
#include <string.h>

static int g_checks;
static int g_failures;

#define CHECK(cond)                                                          \
    do                                                                       \
    {                                                                        \
        g_checks++;                                                          \
        if (!(cond))                                                         \
        {                                                                    \
            g_failures++;                                                    \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);           \
        }                                                                    \
    } while (0)

/* ------------------------------------------------------------- fakes */

#define MAX_SENT 64

static struct
{
    ProtoMessage sent[MAX_SENT];
    int          sent_count;
    bool         fail_send;

    int      waiting_calls;
    int      devices_calls;
    UiDevice devices[PROTO_MAX_DEVICES];
    size_t   device_count;
    int      active_calls;
    uint8_t  active;
    int      pending_calls;
    uint8_t  pending;
    int      notices[2];
} g;

static bool fake_send(const uint8_t frame[PROTO_FRAME_SIZE], void *ctx)
{
    (void)ctx;
    if (g.fail_send)
    {
        return false;
    }
    /* Everything the client sends must be a valid frame. */
    CHECK(Proto_Decode(frame, &g.sent[g.sent_count % MAX_SENT]) == PROTO_OK);
    g.sent_count++;
    return true;
}

static void fake_waiting(void *ctx) { (void)ctx; g.waiting_calls++; }

static void fake_devices(const UiDevice *devices, size_t count, void *ctx)
{
    (void)ctx;
    g.devices_calls++;
    g.device_count = count;
    if (count > 0U)
    {
        memcpy(g.devices, devices, count * sizeof(UiDevice));
    }
}

static void fake_active(uint8_t id, void *ctx) { (void)ctx; g.active_calls++; g.active = id; }
static void fake_pending(uint8_t id, void *ctx) { (void)ctx; g.pending_calls++; g.pending = id; }
static void fake_notice(LinkNotice n, void *ctx) { (void)ctx; g.notices[n]++; }

static const LinkCallbacks k_callbacks = {
    fake_send, fake_waiting, fake_devices, fake_active, fake_pending, fake_notice, NULL,
};

static void reset(LinkClient *link)
{
    memset(&g, 0, sizeof(g));
    Link_Init(link, &k_callbacks);
}

static const ProtoMessage *last_sent(void)
{
    return &g.sent[(g.sent_count - 1) % MAX_SENT];
}

/* Encode a host frame and feed it in. */
static void deliver(LinkClient *link, const ProtoMessage *msg, uint32_t now)
{
    uint8_t frame[PROTO_FRAME_SIZE];
    CHECK(Proto_Encode(msg, frame) == PROTO_OK);
    Link_OnFrame(link, frame, now);
}

static ProtoMessage device_entry(uint8_t gen, uint8_t total, uint8_t pos, uint8_t index, const char *name,
                                 bool is_default)
{
    ProtoMessage m;
    memset(&m, 0, sizeof(m));
    m.command = PROTO_CMD_DEVICE_LIST;
    m.u.device.generation = gen;
    m.u.device.total = total;
    m.u.device.position = pos;
    m.u.device.device_index = index;
    m.u.device.flags = is_default ? PROTO_DEVICE_FLAG_DEFAULT : 0U;
    m.u.device.icon = PROTO_ICON_SPEAKERS;
    m.u.device.name_len = (uint8_t)strlen(name);
    memcpy(m.u.device.name, name, m.u.device.name_len);
    return m;
}

static ProtoMessage simple(uint8_t command, uint8_t value)
{
    ProtoMessage m;
    memset(&m, 0, sizeof(m));
    m.command = command;
    m.u.device_index = value;
    return m;
}

/* Connect and deliver a three-device list (gen 1, indices 10, 11, 12). */
static void connect_with_list(LinkClient *link, uint32_t now)
{
    ProtoMessage e;

    Link_OnConnected(link, now);
    e = device_entry(1, 3, 0, 10, "Hoparl\xC3\xB6rler", false);
    deliver(link, &e, now);
    e = device_entry(1, 3, 1, 11, "Kulakl\xC4\xB1k", true);
    deliver(link, &e, now);
    e = device_entry(1, 3, 2, 12, "Monit\xC3\xB6r", false);
    deliver(link, &e, now);
    e = simple(PROTO_CMD_DEVICE_CHANGED, 11);
    deliver(link, &e, now);
}

/* ------------------------------------------------------------- tests */

static void test_connect_requests_list(void)
{
    LinkClient link;
    reset(&link);

    Link_Poll(&link, 5000);
    CHECK(g.sent_count == 0); /* nothing while disconnected */

    Link_OnConnected(&link, 100);
    CHECK(link.state == LINK_STATE_REQUESTING);
    CHECK(g.waiting_calls == 1);
    CHECK(g.sent_count == 1 && last_sent()->command == PROTO_CMD_GET_DEVICE_LIST);

    Link_Poll(&link, 100 + LINK_RETRY_MS - 1);
    CHECK(g.sent_count == 1);
    Link_Poll(&link, 100 + LINK_RETRY_MS);
    CHECK(g.sent_count == 2 && last_sent()->command == PROTO_CMD_GET_DEVICE_LIST);
    CHECK(g.sent[1].sequence == (uint8_t)(g.sent[0].sequence + 1U));
}

static void test_list_assembly(void)
{
    LinkClient link;
    ProtoMessage e;
    reset(&link);
    Link_OnConnected(&link, 0);

    /* Out of order, with a duplicate; shown only when complete. */
    e = device_entry(1, 3, 2, 12, "C", false);
    deliver(&link, &e, 0);
    e = device_entry(1, 3, 0, 10, "A", false);
    deliver(&link, &e, 0);
    deliver(&link, &e, 0);
    CHECK(g.devices_calls == 0);
    e = device_entry(1, 3, 1, 11, "B", true);
    deliver(&link, &e, 0);
    CHECK(g.devices_calls == 1);
    CHECK(g.device_count == 3);
    CHECK(g.devices[0].id == 10 && strcmp(g.devices[0].name, "A") == 0);
    CHECK(g.devices[2].id == 12 && strcmp(g.devices[2].name, "C") == 0);
    CHECK(g.devices[0].icon == PROTO_ICON_SPEAKERS);
    CHECK(link.state == LINK_STATE_READY);
    /* Default flag used until DEVICE_CHANGED arrives. */
    CHECK(g.active == 11);

    /* No more retries once ready. */
    Link_Poll(&link, 10 * LINK_RETRY_MS);
    CHECK(g.sent_count == 1);
}

static void test_generation_change_restarts_assembly(void)
{
    LinkClient link;
    ProtoMessage e;
    reset(&link);
    Link_OnConnected(&link, 0);

    e = device_entry(1, 2, 0, 10, "Old", false);
    deliver(&link, &e, 0);
    /* New list starts before the old one completed. */
    e = device_entry(2, 2, 1, 21, "New B", false);
    deliver(&link, &e, 0);
    CHECK(g.devices_calls == 0);
    e = device_entry(2, 2, 0, 20, "New A", false);
    deliver(&link, &e, 0);
    CHECK(g.devices_calls == 1);
    CHECK(g.devices[0].id == 20 && g.devices[1].id == 21);
}

static void test_empty_list_and_full_list(void)
{
    LinkClient link;
    ProtoMessage e;
    reset(&link);
    Link_OnConnected(&link, 0);

    e = device_entry(1, 0, 0, PROTO_NO_DEVICE, "", false);
    e.u.device.icon = PROTO_ICON_GENERIC; /* required for an empty list */
    deliver(&link, &e, 0);
    CHECK(g.devices_calls == 1 && g.device_count == 0);
    CHECK(link.state == LINK_STATE_READY);

    /* 32 devices: the full bit mask. */
    for (uint8_t i = 0; i < PROTO_MAX_DEVICES; i++)
    {
        e = device_entry(2, PROTO_MAX_DEVICES, i, i, "D", false);
        deliver(&link, &e, 0);
    }
    CHECK(g.devices_calls == 2 && g.device_count == PROTO_MAX_DEVICES);
}

static void test_device_changed(void)
{
    LinkClient link;
    ProtoMessage e;
    reset(&link);
    connect_with_list(&link, 0);
    CHECK(g.active == 11);

    e = simple(PROTO_CMD_DEVICE_CHANGED, 12);
    deliver(&link, &e, 0);
    CHECK(g.active == 12);

    /* Before a list is shown, DEVICE_CHANGED is remembered, not applied. */
    reset(&link);
    Link_OnConnected(&link, 0);
    e = simple(PROTO_CMD_DEVICE_CHANGED, 30);
    deliver(&link, &e, 0);
    CHECK(g.active_calls == 0);
    e = device_entry(1, 1, 0, 30, "X", false);
    deliver(&link, &e, 0);
    CHECK(g.active == 30);
}

static void test_select_confirmed(void)
{
    LinkClient link;
    ProtoMessage e;
    reset(&link);
    connect_with_list(&link, 0);
    const int before = g.sent_count;

    Link_Select(&link, 10, 500);
    CHECK(g.sent_count == before + 1);
    CHECK(last_sent()->command == PROTO_CMD_SELECT_DEVICE && last_sent()->u.device_index == 10);
    CHECK(g.pending == 10);

    e = simple(PROTO_CMD_DEVICE_CHANGED, 10);
    deliver(&link, &e, 600);
    CHECK(g.active == 10);
    CHECK(link.pending == PROTO_NO_DEVICE);

    /* No timeout later. */
    Link_Poll(&link, 600 + 10U * LINK_RESPONSE_TIMEOUT_MS);
    CHECK(g.notices[LINK_NOTICE_HOST_TIMEOUT] == 0);
}

static void test_select_error(void)
{
    LinkClient link;
    ProtoMessage e;
    reset(&link);
    connect_with_list(&link, 0);

    Link_Select(&link, 12, 0);
    const uint8_t select_seq = last_sent()->sequence;

    /* An ERROR about something else is ignored. */
    memset(&e, 0, sizeof(e));
    e.command = PROTO_CMD_ERROR;
    e.u.error.code = PROTO_ERROR_OPERATION_FAILED;
    e.u.error.ref_command = PROTO_CMD_SELECT_DEVICE;
    e.u.error.ref_sequence = (uint8_t)(select_seq + 5U);
    deliver(&link, &e, 10);
    CHECK(g.notices[LINK_NOTICE_SELECT_FAILED] == 0);

    e.u.error.ref_sequence = select_seq;
    deliver(&link, &e, 10);
    CHECK(g.notices[LINK_NOTICE_SELECT_FAILED] == 1);
    CHECK(g.active == 11); /* pending cleared, previous active kept */
    CHECK(link.state == LINK_STATE_READY);

    /* UNKNOWN_DEVICE: also re-requests the list. */
    Link_Select(&link, 12, 20);
    e.u.error.code = PROTO_ERROR_UNKNOWN_DEVICE;
    e.u.error.ref_sequence = last_sent()->sequence;
    deliver(&link, &e, 30);
    CHECK(g.notices[LINK_NOTICE_SELECT_FAILED] == 2);
    CHECK(link.state == LINK_STATE_REQUESTING);
    CHECK(last_sent()->command == PROTO_CMD_GET_DEVICE_LIST);
}

static void test_select_timeout(void)
{
    LinkClient link;
    reset(&link);
    connect_with_list(&link, 0);

    Link_Select(&link, 10, 1000);
    Link_Poll(&link, 1000 + LINK_RESPONSE_TIMEOUT_MS - 1);
    CHECK(g.notices[LINK_NOTICE_HOST_TIMEOUT] == 0);
    Link_Poll(&link, 1000 + LINK_RESPONSE_TIMEOUT_MS);
    CHECK(g.notices[LINK_NOTICE_HOST_TIMEOUT] == 1);
    CHECK(link.state == LINK_STATE_REQUESTING);
    CHECK(g.waiting_calls == 2);
    CHECK(last_sent()->command == PROTO_CMD_GET_DEVICE_LIST);
}

static void test_select_ignored_when_not_ready(void)
{
    LinkClient link;
    reset(&link);
    Link_Select(&link, 1, 0);
    CHECK(g.sent_count == 0);
    Link_OnConnected(&link, 0);
    Link_Select(&link, 1, 0);
    CHECK(g.sent_count == 1); /* only the GET_DEVICE_LIST */

    /* Failed send: nothing pending. */
    reset(&link);
    connect_with_list(&link, 0);
    g.fail_send = true;
    Link_Select(&link, 10, 0);
    CHECK(g.pending_calls == 0);
    CHECK(link.send_failures == 1);
}

static void test_bad_frames(void)
{
    LinkClient link;
    uint8_t frame[PROTO_FRAME_SIZE];
    ProtoMessage e = simple(PROTO_CMD_DEVICE_CHANGED, 3);
    reset(&link);
    connect_with_list(&link, 0);
    const int before = g.sent_count;

    e.sequence = 9;
    CHECK(Proto_Encode(&e, frame) == PROTO_OK);
    frame[20] ^= 0x10U;
    Link_OnFrame(&link, frame, 0);
    CHECK(g.sent_count == before + 1);
    CHECK(last_sent()->command == PROTO_CMD_ERROR);
    CHECK(last_sent()->u.error.code == PROTO_ERROR_BAD_FRAME);
    CHECK(last_sent()->u.error.ref_command == PROTO_CMD_DEVICE_CHANGED);
    CHECK(last_sent()->u.error.ref_sequence == 9);
    CHECK(g.active == 11);

    /* A damaged ERROR frame is not answered. */
    memset(&e, 0, sizeof(e));
    e.command = PROTO_CMD_ERROR;
    e.u.error.code = PROTO_ERROR_BUSY;
    CHECK(Proto_Encode(&e, frame) == PROTO_OK);
    frame[30] ^= 0x01U;
    Link_OnFrame(&link, frame, 0);
    CHECK(g.sent_count == before + 1);
    CHECK(link.frames_rejected == 2);

    /* Frames are ignored while disconnected. */
    Link_OnDisconnected(&link);
    CHECK(link.state == LINK_STATE_DISCONNECTED);
    e = simple(PROTO_CMD_DEVICE_CHANGED, 10);
    deliver(&link, &e, 0);
    CHECK(g.active == 11);
}

int main(void)
{
    test_connect_requests_list();
    test_list_assembly();
    test_generation_change_restarts_assembly();
    test_empty_list_and_full_list();
    test_device_changed();
    test_select_confirmed();
    test_select_error();
    test_select_timeout();
    test_select_ignored_when_not_ready();
    test_bad_frames();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
