/**
 * @file    test_protocol.c
 * @brief   Unit tests for the protocol v1 codec (no test framework).
 */
#include "protocol.h"

#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_checks;

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

static ProtoMessage simple(uint8_t command, uint8_t sequence)
{
    ProtoMessage m;

    memset(&m, 0, sizeof(m));
    m.command = command;
    m.sequence = sequence;
    return m;
}

static ProtoMessage device(uint8_t total, uint8_t position, uint8_t index, const char *name)
{
    ProtoMessage m = simple(PROTO_CMD_DEVICE_LIST, 7U);

    m.u.device.generation = 3U;
    m.u.device.total = total;
    m.u.device.position = position;
    m.u.device.device_index = index;
    m.u.device.flags = PROTO_DEVICE_FLAG_DEFAULT;
    m.u.device.icon = PROTO_ICON_HEADPHONES;
    m.u.device.name_len = (uint8_t)strlen(name);
    memcpy(m.u.device.name, name, m.u.device.name_len);
    return m;
}

/* Recompute the CRC after a test modified a frame on purpose. */
static void reseal(uint8_t frame[PROTO_FRAME_SIZE])
{
    const uint16_t crc = Proto_Crc16(frame, PROTO_CRC_OFFSET);

    frame[PROTO_CRC_OFFSET] = (uint8_t)(crc & 0xFFU);
    frame[PROTO_CRC_OFFSET + 1U] = (uint8_t)(crc >> 8);
}

static void test_crc_check_value(void)
{
    /* Standard check value of CRC-16/CCITT-FALSE. */
    const uint8_t text[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };

    CHECK(Proto_Crc16(text, sizeof(text)) == 0x29B1U);
    CHECK(Proto_Crc16(text, 0U) == 0xFFFFU);
}

static void test_header_and_golden_frame(void)
{
    /* Byte-exact example documented in protocol.md. */
    static const uint8_t expected_head[] = { 0xA5, 0x01, 0x03, 0x2A, 0x01, 0x02 };
    ProtoMessage m = simple(PROTO_CMD_SELECT_DEVICE, 0x2AU);
    uint8_t frame[PROTO_FRAME_SIZE];

    m.u.device_index = 2U;
    CHECK(Proto_Encode(&m, frame) == PROTO_OK);
    CHECK(memcmp(frame, expected_head, sizeof(expected_head)) == 0);
    for (size_t i = sizeof(expected_head); i < PROTO_CRC_OFFSET; i++)
    {
        CHECK(frame[i] == 0U);
    }
    CHECK(frame[62] == 0x87U);
    CHECK(frame[63] == 0x2BU);
}

static void test_round_trip_simple(void)
{
    static const struct
    {
        uint8_t command;
        uint8_t value;
    } cases[] = {
        { PROTO_CMD_SELECT_DEVICE, 0U },     { PROTO_CMD_SELECT_DEVICE, 254U },
        { PROTO_CMD_DEVICE_CHANGED, 5U },    { PROTO_CMD_DEVICE_CHANGED, PROTO_NO_DEVICE },
        { PROTO_CMD_SET_VOLUME, 0U },        { PROTO_CMD_SET_VOLUME, 100U },
        { PROTO_CMD_VOLUME_CHANGED, 42U },   { PROTO_CMD_SET_MUTE, 1U },
        { PROTO_CMD_MUTE_CHANGED, 0U },
    };

    for (size_t i = 0U; i < sizeof(cases) / sizeof(cases[0]); i++)
    {
        ProtoMessage in = simple(cases[i].command, (uint8_t)i);
        ProtoMessage out;
        uint8_t frame[PROTO_FRAME_SIZE];

        /* All one-byte payloads share the union's first byte. */
        in.u.volume = cases[i].value;
        CHECK(Proto_Encode(&in, frame) == PROTO_OK);
        CHECK(frame[4] == 1U);
        CHECK(Proto_Decode(frame, &out) == PROTO_OK);
        CHECK(out.command == cases[i].command);
        CHECK(out.sequence == (uint8_t)i);
        CHECK(out.u.volume == cases[i].value);
    }
}

static void test_round_trip_get_list_and_error(void)
{
    ProtoMessage in = simple(PROTO_CMD_GET_DEVICE_LIST, 255U);
    ProtoMessage out;
    uint8_t frame[PROTO_FRAME_SIZE];

    CHECK(Proto_Encode(&in, frame) == PROTO_OK);
    CHECK(frame[4] == 0U);
    CHECK(Proto_Decode(frame, &out) == PROTO_OK);
    CHECK(out.command == PROTO_CMD_GET_DEVICE_LIST);
    CHECK(out.sequence == 255U);

    in = simple(PROTO_CMD_ERROR, 9U);
    in.u.error.code = PROTO_ERROR_UNKNOWN_DEVICE;
    in.u.error.ref_command = PROTO_CMD_SELECT_DEVICE;
    in.u.error.ref_sequence = 8U;
    CHECK(Proto_Encode(&in, frame) == PROTO_OK);
    CHECK(Proto_Decode(frame, &out) == PROTO_OK);
    CHECK(out.u.error.code == PROTO_ERROR_UNKNOWN_DEVICE);
    CHECK(out.u.error.ref_command == PROTO_CMD_SELECT_DEVICE);
    CHECK(out.u.error.ref_sequence == 8U);
}

static void test_round_trip_device(void)
{
    /* 48 bytes of UTF-8 including two-byte Turkish letters. */
    const char *long_name = "Hoparl\xC3\xB6rler (Realtek(R) Audio) \xC3\xA7\xC4\xB1k\xC4\xB1\xC5\x9F 123456";
    ProtoMessage in = device(4U, 3U, 17U, long_name);
    ProtoMessage out;
    uint8_t frame[PROTO_FRAME_SIZE];

    CHECK(strlen(long_name) == PROTO_MAX_NAME_BYTES);
    CHECK(Proto_Encode(&in, frame) == PROTO_OK);
    CHECK(frame[4] == 7U + PROTO_MAX_NAME_BYTES);
    CHECK(Proto_Decode(frame, &out) == PROTO_OK);
    CHECK(out.u.device.generation == 3U);
    CHECK(out.u.device.total == 4U);
    CHECK(out.u.device.position == 3U);
    CHECK(out.u.device.device_index == 17U);
    CHECK(out.u.device.flags == PROTO_DEVICE_FLAG_DEFAULT);
    CHECK(out.u.device.icon == PROTO_ICON_HEADPHONES);
    CHECK(out.u.device.name_len == PROTO_MAX_NAME_BYTES);
    CHECK(strcmp(out.u.device.name, long_name) == 0);

    /* Empty list. */
    in = device(0U, 0U, PROTO_NO_DEVICE, "");
    in.u.device.flags = 0U;
    in.u.device.icon = PROTO_ICON_GENERIC;
    CHECK(Proto_Encode(&in, frame) == PROTO_OK);
    CHECK(Proto_Decode(frame, &out) == PROTO_OK);
    CHECK(out.u.device.total == 0U);
    CHECK(out.u.device.name[0] == '\0');
}

static void test_encode_rejects(void)
{
    uint8_t frame[PROTO_FRAME_SIZE];
    ProtoMessage m = simple(0x09U, 0U);

    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_COMMAND);
    CHECK(frame[0] == 0U); /* frame cleared on error */
    CHECK(Proto_Encode(NULL, frame) == PROTO_ERR_ARG);
    CHECK(Proto_Encode(&m, NULL) == PROTO_ERR_ARG);

    m = simple(PROTO_CMD_SET_VOLUME, 0U);
    m.u.volume = 101U;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = simple(PROTO_CMD_SET_MUTE, 0U);
    m.u.muted = 2U;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = simple(PROTO_CMD_SELECT_DEVICE, 0U);
    m.u.device_index = PROTO_NO_DEVICE;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = simple(PROTO_CMD_ERROR, 0U);
    m.u.error.code = 0U;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);

    m = device(4U, 4U, 1U, "x"); /* position == total */
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = device(PROTO_MAX_DEVICES + 1U, 0U, 1U, "x");
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = device(1U, 0U, PROTO_NO_DEVICE, "x");
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = device(1U, 0U, 1U, "x");
    m.u.device.icon = PROTO_ICON_COUNT;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = device(1U, 0U, 1U, "x");
    m.u.device.flags = 0x02U;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = device(1U, 0U, 1U, "x");
    m.u.device.name_len = PROTO_MAX_NAME_BYTES + 1U;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = device(1U, 0U, 1U, "ab");
    m.u.device.name[0] = '\0'; /* embedded NUL */
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
    m = device(0U, 0U, 3U, ""); /* empty list must use PROTO_NO_DEVICE */
    m.u.device.flags = 0U;
    m.u.device.icon = 0U;
    CHECK(Proto_Encode(&m, frame) == PROTO_ERR_PAYLOAD);
}

static void test_decode_rejects_frame(void)
{
    ProtoMessage m = simple(PROTO_CMD_SET_VOLUME, 0x11U);
    ProtoMessage out;
    uint8_t good[PROTO_FRAME_SIZE];
    uint8_t frame[PROTO_FRAME_SIZE];

    m.u.volume = 50U;
    CHECK(Proto_Encode(&m, good) == PROTO_OK);

    memcpy(frame, good, sizeof(frame));
    frame[0] = 0x5AU;
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_MAGIC);
    CHECK(out.sequence == 0x11U); /* header still reported */

    memcpy(frame, good, sizeof(frame));
    frame[1] = 2U;
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_VERSION);
    CHECK(Proto_ErrorCodeFor(PROTO_ERR_VERSION) == PROTO_ERROR_UNSUPPORTED_VERSION);

    /* Every single-bit error in the frame is caught. */
    for (size_t byte = 2U; byte < PROTO_FRAME_SIZE; byte++)
    {
        for (unsigned bit = 0U; bit < 8U; bit++)
        {
            memcpy(frame, good, sizeof(frame));
            frame[byte] ^= (uint8_t)(1U << bit);
            CHECK(Proto_Decode(frame, &out) == PROTO_ERR_CRC);
        }
    }

    memcpy(frame, good, sizeof(frame));
    frame[4] = PROTO_MAX_PAYLOAD + 1U;
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_LENGTH);

    memcpy(frame, good, sizeof(frame));
    frame[2] = 0x42U;
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_COMMAND);
    CHECK(out.command == 0x42U);
    CHECK(Proto_ErrorCodeFor(PROTO_ERR_COMMAND) == PROTO_ERROR_UNKNOWN_COMMAND);

    CHECK(Proto_Decode(NULL, &out) == PROTO_ERR_ARG);
    CHECK(Proto_Decode(good, NULL) == PROTO_ERR_ARG);
}

static void test_decode_rejects_payload(void)
{
    ProtoMessage m = simple(PROTO_CMD_SET_VOLUME, 1U);
    ProtoMessage dev = device(2U, 1U, 4U, "Monitor");
    ProtoMessage out;
    uint8_t frame[PROTO_FRAME_SIZE];

    m.u.volume = 10U;
    CHECK(Proto_Encode(&m, frame) == PROTO_OK);
    frame[5] = 101U; /* volume out of range */
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_PAYLOAD);

    CHECK(Proto_Encode(&m, frame) == PROTO_OK);
    frame[4] = 2U; /* wrong payload length */
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_PAYLOAD);

    m = simple(PROTO_CMD_GET_DEVICE_LIST, 1U);
    CHECK(Proto_Encode(&m, frame) == PROTO_OK);
    frame[4] = 1U;
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_PAYLOAD);

    CHECK(Proto_Encode(&dev, frame) == PROTO_OK);
    frame[5 + 6] = 8U; /* name_len no longer matches length */
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_PAYLOAD);
    CHECK(out.u.device.total == 0U); /* fields cleared on error */
    CHECK(out.command == PROTO_CMD_DEVICE_LIST);

    CHECK(Proto_Encode(&dev, frame) == PROTO_OK);
    frame[5 + 7] = 0U; /* NUL inside the name */
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_PAYLOAD);

    CHECK(Proto_Encode(&dev, frame) == PROTO_OK);
    frame[4] = 6U; /* shorter than the fixed device fields */
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_PAYLOAD);

    m = simple(PROTO_CMD_ERROR, 1U);
    m.u.error.code = PROTO_ERROR_BUSY;
    CHECK(Proto_Encode(&m, frame) == PROTO_OK);
    frame[5] = 0x08U; /* unknown error code */
    reseal(frame);
    CHECK(Proto_Decode(frame, &out) == PROTO_ERR_PAYLOAD);
    CHECK(Proto_ErrorCodeFor(PROTO_ERR_PAYLOAD) == PROTO_ERROR_BAD_PAYLOAD);
    CHECK(Proto_ErrorCodeFor(PROTO_ERR_CRC) == PROTO_ERROR_BAD_FRAME);
}

int main(void)
{
    test_crc_check_value();
    test_header_and_golden_frame();
    test_round_trip_simple();
    test_round_trip_get_list_and_error();
    test_round_trip_device();
    test_encode_rejects();
    test_decode_rejects_frame();
    test_decode_rejects_payload();

    printf("%d checks, %d failures\n", g_checks, g_failures);
    return (g_failures == 0) ? 0 : 1;
}
