// Unit tests for the host device model and protocol controller.
// No test framework; returns non-zero if a check fails.

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "AudioMapping.h"
#include "DeviceRegistry.h"
#include "EventQueue.h"
#include "FrameText.h"
#include "HidMatch.h"
#include "HostController.h"

using namespace stm32_audio::host;

namespace {

int g_checks = 0;
int g_failures = 0;

#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failures;                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);   \
        }                                                                 \
    } while (0)

// ---------------------------------------------------------------- fakes

class FakeAudio : public IAudioSystem {
public:
    std::vector<AudioEndpoint> endpoints;
    std::string defaultId;
    std::optional<unsigned> volume = 50;
    std::optional<bool> mute = false;
    bool failSetDefault = false;
    bool failSetVolume = false;
    std::vector<std::string> setDefaultCalls;

    std::vector<AudioEndpoint> renderEndpoints() override { return endpoints; }
    std::string defaultEndpointId() override { return defaultId; }
    bool setDefaultEndpoint(const std::string& id) override
    {
        setDefaultCalls.push_back(id);
        if (failSetDefault) {
            return false;
        }
        defaultId = id;
        return true;
    }
    std::optional<unsigned> volumePercent() override { return volume; }
    bool setVolumePercent(unsigned percent) override
    {
        if (failSetVolume || !volume) {
            return false;
        }
        volume = percent;
        return true;
    }
    std::optional<bool> muted() override { return mute; }
    bool setMuted(bool m) override
    {
        if (!mute) {
            return false;
        }
        mute = m;
        return true;
    }
};

class FakeSink : public IFrameSink {
public:
    std::deque<ProtoMessage> sent;
    bool fail = false;

    bool sendFrame(const Frame& frame) override
    {
        if (fail) {
            return false;
        }
        ProtoMessage msg{};
        // Every frame the controller sends must be a valid protocol frame.
        if (Proto_Decode(frame.data(), &msg) != PROTO_OK) {
            std::printf("FAIL controller sent an invalid frame\n");
            ++g_failures;
        }
        sent.push_back(msg);
        return true;
    }

    ProtoMessage pop()
    {
        ProtoMessage m{};
        if (!sent.empty()) {
            m = sent.front();
            sent.pop_front();
        }
        return m;
    }
};

AudioEndpoint ep(const std::string& id, const std::string& name, FormFactor ff = FormFactor::Speakers)
{
    return AudioEndpoint{id, name, ff};
}

Frame frameFor(std::uint8_t command, std::uint8_t sequence, std::uint8_t value = 0)
{
    ProtoMessage msg{};
    Frame frame{};
    msg.command = command;
    msg.sequence = sequence;
    msg.u.volume = value;  // one-byte payloads share the union's first byte
    (void)Proto_Encode(&msg, frame.data());
    return frame;
}

// ----------------------------------------------------- registry tests

void testTruncateUtf8()
{
    CHECK(truncateUtf8("abc", 48) == "abc");
    CHECK(truncateUtf8("abcdef", 4) == "abcd");
    // "ö" is two bytes: cutting after 3 bytes must not split it.
    CHECK(truncateUtf8("ab\xC3\xB6x", 3) == "ab");
    CHECK(truncateUtf8("ab\xC3\xB6x", 4) == "ab\xC3\xB6");
    // Four-byte sequence.
    CHECK(truncateUtf8("a\xF0\x9F\x8E\xA7", 3) == "a");
    std::string withNul("a\0b", 3);
    CHECK(truncateUtf8(withNul, 48) == "ab");
}

void testIcons()
{
    CHECK(iconFor(FormFactor::Speakers) == PROTO_ICON_SPEAKERS);
    CHECK(iconFor(FormFactor::Headphones) == PROTO_ICON_HEADPHONES);
    CHECK(iconFor(FormFactor::Headset) == PROTO_ICON_HEADSET);
    CHECK(iconFor(FormFactor::Display) == PROTO_ICON_DISPLAY);
    CHECK(iconFor(FormFactor::DigitalOutput) == PROTO_ICON_DIGITAL);
    CHECK(iconFor(FormFactor::Unknown) == PROTO_ICON_GENERIC);
}

void testRegistryIndices()
{
    DeviceRegistry reg;
    CHECK(reg.generation() == 0);

    CHECK(reg.update({ep("A", "Speakers"), ep("B", "Headphones")}, "B"));
    CHECK(reg.generation() == 1);
    const std::uint8_t a = *reg.indexOf("A");
    const std::uint8_t b = *reg.indexOf("B");
    CHECK(a != b);

    // Same content: no new generation.
    CHECK(!reg.update({ep("A", "Speakers"), ep("B", "Headphones")}, "B"));
    CHECK(reg.generation() == 1);

    // B removed, C added: A keeps its index, C does not get B's index.
    CHECK(reg.update({ep("A", "Speakers"), ep("C", "Monitor")}, "A"));
    CHECK(reg.generation() == 2);
    CHECK(*reg.indexOf("A") == a);
    CHECK(!reg.indexOf("B"));
    CHECK(*reg.indexOf("C") != b);
    CHECK(reg.findByIndex(b) == nullptr);

    // Rename changes the content.
    CHECK(reg.update({ep("A", "Speakers 2"), ep("C", "Monitor")}, "A"));
    CHECK(reg.generation() == 3);
    // Order change changes the content.
    CHECK(reg.update({ep("C", "Monitor"), ep("A", "Speakers 2")}, "A"));
    CHECK(reg.devices().front().endpoint.id == "C");
}

void testRegistryIndexWraps()
{
    DeviceRegistry reg;
    // Churn through more than 255 endpoints: indices stay in 0..254 and
    // never collide with an endpoint still present.
    for (int i = 0; i < 600; ++i) {
        CHECK(reg.update({ep("keep", "Keep"), ep("tmp" + std::to_string(i), "Tmp")}, "keep"));
        const std::uint8_t keep = *reg.indexOf("keep");
        const std::uint8_t tmp = *reg.indexOf("tmp" + std::to_string(i));
        CHECK(keep != tmp);
        CHECK(tmp != PROTO_NO_DEVICE);
    }
}

void testRegistryLimit()
{
    DeviceRegistry reg;
    std::vector<AudioEndpoint> many;
    for (unsigned i = 0; i < PROTO_MAX_DEVICES + 5U; ++i) {
        many.push_back(ep("id" + std::to_string(i), "Device " + std::to_string(i)));
    }
    const std::string lastId = many.back().id;

    reg.update(many, lastId);
    CHECK(reg.devices().size() == PROTO_MAX_DEVICES);
    CHECK(reg.indexOf(lastId).has_value());  // the default is kept
    CHECK(reg.devices().back().endpoint.id == lastId);

    reg.update(many, "id0");
    CHECK(reg.indexOf("id0").has_value());
    CHECK(!reg.indexOf(lastId).has_value());
}

void testRegistryEntries()
{
    DeviceRegistry reg;
    std::vector<ProtoDeviceEntry> e = reg.entries("");
    CHECK(e.size() == 1);
    CHECK(e[0].total == 0 && e[0].device_index == PROTO_NO_DEVICE && e[0].name_len == 0);

    const std::string longName(60, 'x');
    reg.update({ep("A", "Hoparl\xC3\xB6rler", FormFactor::Speakers),
                ep("B", longName, FormFactor::Headphones)},
               "B");
    e = reg.entries("B");
    CHECK(e.size() == 2);
    CHECK(e[0].total == 2 && e[0].position == 0 && e[1].position == 1);
    CHECK(e[0].generation == reg.generation());
    CHECK(e[0].flags == 0 && e[1].flags == PROTO_DEVICE_FLAG_DEFAULT);
    CHECK(e[0].icon == PROTO_ICON_SPEAKERS && e[1].icon == PROTO_ICON_HEADPHONES);
    CHECK(std::strcmp(e[0].name, "Hoparl\xC3\xB6rler") == 0);
    CHECK(e[1].name_len == PROTO_MAX_NAME_BYTES);
}

// --------------------------------------------------- controller tests

struct Rig {
    FakeAudio audio;
    FakeSink sink;
    HostController controller{audio, sink};

    Rig()
    {
        audio.endpoints = {ep("spk", "Speakers"), ep("hp", "Headphones", FormFactor::Headphones),
                           ep("tv", "TV", FormFactor::Display)};
        audio.defaultId = "hp";
        controller.onDevicesChanged();  // before GET_DEVICE_LIST: nothing sent
    }

    std::uint8_t index(const std::string& id) { return *controller.registry().indexOf(id); }
};

void testNothingBeforeFirstRequest()
{
    Rig r;
    CHECK(r.sink.sent.empty());
    r.audio.volume = 70;
    r.controller.onVolumeChanged();
    r.controller.onDefaultChanged();
    CHECK(r.sink.sent.empty());
}

void testGetDeviceList()
{
    Rig r;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));

    CHECK(r.sink.sent.size() == 3 + 3);
    for (unsigned i = 0; i < 3; ++i) {
        const ProtoMessage m = r.sink.pop();
        CHECK(m.command == PROTO_CMD_DEVICE_LIST);
        CHECK(m.u.device.total == 3 && m.u.device.position == i);
    }
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_CHANGED && m.u.device_index == r.index("hp"));
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_VOLUME_CHANGED && m.u.volume == 50);
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_MUTE_CHANGED && m.u.muted == 0);

    // A second request is answered in full again.
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 2));
    CHECK(r.sink.sent.size() == 6);
}

void testSequenceNumbers()
{
    Rig r;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    std::uint8_t expected = 0;
    while (!r.sink.sent.empty()) {
        CHECK(r.sink.pop().sequence == expected);
        ++expected;
    }
}

void testEmptyList()
{
    Rig r;
    r.audio.endpoints.clear();
    r.audio.defaultId.clear();
    r.audio.volume.reset();
    r.audio.mute.reset();
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    CHECK(r.sink.sent.size() == 2);
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_LIST && m.u.device.total == 0);
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_CHANGED && m.u.device_index == PROTO_NO_DEVICE);
}

void testSelectDevice()
{
    Rig r;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    r.sink.sent.clear();

    r.controller.onFrame(frameFor(PROTO_CMD_SELECT_DEVICE, 5, r.index("tv")));
    CHECK(r.audio.setDefaultCalls.size() == 1 && r.audio.setDefaultCalls[0] == "tv");
    CHECK(r.sink.sent.size() == 1);
    const ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_CHANGED && m.u.device_index == r.index("tv"));

    // The Windows notification that follows does not repeat it.
    r.controller.onDefaultChanged();
    CHECK(r.sink.sent.empty());
}

// Indices belong to a session: a SELECT_DEVICE before the STM32 has asked
// for the list since the last connect may carry an index from an earlier
// session (host restarted, board kept plugged in) and is refused.
// Indices belong to a session: a SELECT_DEVICE before the list has been
// sent since the last connect may carry an index from an earlier session
// and is refused.
void testSelectBeforeListSent()
{
    Rig r;
    r.controller.onDevicesChanged();  // the registry is filled, nothing sent
    CHECK(r.sink.sent.empty());

    r.controller.onFrame(frameFor(PROTO_CMD_SELECT_DEVICE, 4, r.index("tv")));
    CHECK(r.audio.setDefaultCalls.empty());
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_ERROR && m.u.error.code == PROTO_ERROR_UNKNOWN_DEVICE);
    CHECK(m.u.error.ref_command == PROTO_CMD_SELECT_DEVICE && m.u.error.ref_sequence == 4);

    // Same after a disconnect: the old session's list no longer counts.
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 5));
    r.sink.sent.clear();
    r.controller.onDisconnected();
    r.controller.onFrame(frameFor(PROTO_CMD_SELECT_DEVICE, 6, r.index("tv")));
    CHECK(r.audio.setDefaultCalls.empty());
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_ERROR && m.u.error.code == PROTO_ERROR_UNKNOWN_DEVICE);

    // Once the list has been sent (requested or on connect), it works.
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 7));
    r.sink.sent.clear();
    r.controller.onFrame(frameFor(PROTO_CMD_SELECT_DEVICE, 8, r.index("tv")));
    CHECK(r.audio.setDefaultCalls.size() == 1 && r.audio.setDefaultCalls[0] == "tv");
}

void testSelectErrors()
{
    Rig r;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    r.sink.sent.clear();

    r.controller.onFrame(frameFor(PROTO_CMD_SELECT_DEVICE, 9, 200));  // not in the list
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_ERROR && m.u.error.code == PROTO_ERROR_UNKNOWN_DEVICE);
    CHECK(m.u.error.ref_command == PROTO_CMD_SELECT_DEVICE && m.u.error.ref_sequence == 9);
    CHECK(r.audio.setDefaultCalls.empty());

    r.audio.failSetDefault = true;
    r.controller.onFrame(frameFor(PROTO_CMD_SELECT_DEVICE, 10, r.index("spk")));
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_ERROR && m.u.error.code == PROTO_ERROR_OPERATION_FAILED);
    CHECK(m.u.error.ref_sequence == 10);
    CHECK(r.sink.sent.empty());
}

void testVolumeAndMute()
{
    Rig r;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    r.sink.sent.clear();

    r.controller.onFrame(frameFor(PROTO_CMD_SET_VOLUME, 3, 80));
    CHECK(r.audio.volume == 80U);
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_VOLUME_CHANGED && m.u.volume == 80);
    CHECK(r.sink.sent.empty());

    r.controller.onFrame(frameFor(PROTO_CMD_SET_MUTE, 4, 1));
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_MUTE_CHANGED && m.u.muted == 1);

    // Windows reports what was already sent: nothing new.
    r.controller.onVolumeChanged();
    CHECK(r.sink.sent.empty());

    // Changed from the Windows side.
    r.audio.volume = 20;
    r.controller.onVolumeChanged();
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_VOLUME_CHANGED && m.u.volume == 20);
    CHECK(r.sink.sent.empty());

    r.audio.failSetVolume = true;
    r.controller.onFrame(frameFor(PROTO_CMD_SET_VOLUME, 6, 30));
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_ERROR && m.u.error.code == PROTO_ERROR_OPERATION_FAILED);
}

void testWindowsChanges()
{
    Rig r;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    r.sink.sent.clear();
    const std::uint8_t generation = r.controller.registry().generation();

    // No change: nothing sent.
    r.controller.onDevicesChanged();
    CHECK(r.sink.sent.empty());

    // Device removed: new list with a new generation, then DEVICE_CHANGED.
    r.audio.endpoints.pop_back();
    r.controller.onDevicesChanged();
    CHECK(r.sink.sent.size() == 3);
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_LIST && m.u.device.total == 2);
    CHECK(m.u.device.generation != generation);
    r.sink.pop();
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_CHANGED && m.u.device_index == r.index("hp"));

    // Default changed in Windows.
    r.audio.defaultId = "spk";
    r.audio.volume = 30;
    r.controller.onDefaultChanged();
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_CHANGED && m.u.device_index == r.index("spk"));
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_VOLUME_CHANGED && m.u.volume == 30);
    CHECK(r.sink.sent.empty());

    // New default that is also a new device: list first.
    r.audio.endpoints.push_back(ep("usb", "USB DAC", FormFactor::DigitalOutput));
    r.audio.defaultId = "usb";
    r.controller.onDefaultChanged();
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_LIST && m.u.device.total == 3);
    r.sink.pop();
    r.sink.pop();
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_CHANGED && m.u.device_index == r.index("usb"));
}

void testReconnect()
{
    Rig r;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    r.sink.sent.clear();
    r.controller.onDisconnected();

    r.audio.volume = 90;
    r.controller.onVolumeChanged();  // nothing while disconnected
    r.controller.onDevicesChanged();
    CHECK(r.sink.sent.empty());
}

// The host opens the device: the full answer to GET_DEVICE_LIST goes out
// without a request, so a board that kept the previous session's list
// (host restarted, board stayed plugged in) is brought up to date.
void testConnectSendsList()
{
    Rig r;
    r.controller.onConnected();
    CHECK(r.sink.sent.size() == 3 + 3);
    for (unsigned i = 0; i < 3; ++i) {
        const ProtoMessage m = r.sink.pop();
        CHECK(m.command == PROTO_CMD_DEVICE_LIST && m.u.device.total == 3 && m.u.device.position == i);
    }
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_DEVICE_CHANGED && m.u.device_index == r.index("hp"));
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_VOLUME_CHANGED && m.u.volume == 50);
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_MUTE_CHANGED);

    // Synced: changes are sent, and a selection is accepted.
    r.audio.volume = 70;
    r.controller.onVolumeChanged();
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_VOLUME_CHANGED && m.u.volume == 70);
    r.controller.onFrame(frameFor(PROTO_CMD_SELECT_DEVICE, 3, r.index("tv")));
    CHECK(r.audio.setDefaultCalls.size() == 1 && r.audio.setDefaultCalls[0] == "tv");

    // A GET_DEVICE_LIST that crossed the pushed list is answered in full
    // again (the board may also ask right after USB enumeration).
    r.sink.sent.clear();
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 4));
    CHECK(r.sink.sent.size() == 6);

    // Every connect sends the list again.
    r.sink.sent.clear();
    r.controller.onDisconnected();
    r.controller.onConnected();
    CHECK(r.sink.sent.size() == 6);
}

void testBadFrames()
{
    Rig r;
    Frame frame = frameFor(PROTO_CMD_SET_VOLUME, 7, 10);
    frame[10] ^= 0x01U;  // corrupt the payload area: CRC fails
    r.controller.onFrame(frame);
    ProtoMessage m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_ERROR && m.u.error.code == PROTO_ERROR_BAD_FRAME);
    CHECK(m.u.error.ref_command == PROTO_CMD_SET_VOLUME && m.u.error.ref_sequence == 7);

    // Unsupported version.
    frame = frameFor(PROTO_CMD_GET_DEVICE_LIST, 8);
    frame[1] = 2;
    const std::uint16_t crc = Proto_Crc16(frame.data(), PROTO_CRC_OFFSET);
    frame[PROTO_CRC_OFFSET] = static_cast<std::uint8_t>(crc & 0xFFU);
    frame[PROTO_CRC_OFFSET + 1] = static_cast<std::uint8_t>(crc >> 8);
    r.controller.onFrame(frame);
    m = r.sink.pop();
    CHECK(m.command == PROTO_CMD_ERROR && m.u.error.code == PROTO_ERROR_UNSUPPORTED_VERSION);

    // A damaged ERROR frame is not answered.
    ProtoMessage err{};
    err.command = PROTO_CMD_ERROR;
    err.u.error.code = PROTO_ERROR_BUSY;
    (void)Proto_Encode(&err, frame.data());
    frame[20] ^= 0x80U;
    r.controller.onFrame(frame);
    CHECK(r.sink.sent.empty());
    CHECK(r.controller.stats().framesRejected == 3);

    // A valid ERROR frame is counted, not answered.
    (void)Proto_Encode(&err, frame.data());
    r.controller.onFrame(frame);
    CHECK(r.sink.sent.empty());
    CHECK(r.controller.stats().errorsReceived == 1);

    // Wrong direction (host -> STM32 command) is dropped.
    r.controller.onFrame(frameFor(PROTO_CMD_VOLUME_CHANGED, 1, 10));
    CHECK(r.sink.sent.empty());
    CHECK(r.controller.stats().framesIgnored == 1);
}

void testSendFailure()
{
    Rig r;
    r.sink.fail = true;
    r.controller.onFrame(frameFor(PROTO_CMD_GET_DEVICE_LIST, 1));
    CHECK(r.controller.stats().sendFailures == 6);
    CHECK(r.controller.stats().framesSent == 0);
}

// ------------------------------------------------- helper tests

void testAudioMapping()
{
    CHECK(formFactorFromWindows(1) == FormFactor::Speakers);
    CHECK(formFactorFromWindows(3) == FormFactor::Headphones);
    CHECK(formFactorFromWindows(5) == FormFactor::Headset);
    CHECK(formFactorFromWindows(6) == FormFactor::Headset);
    CHECK(formFactorFromWindows(7) == FormFactor::DigitalOutput);
    CHECK(formFactorFromWindows(8) == FormFactor::DigitalOutput);
    CHECK(formFactorFromWindows(9) == FormFactor::Display);
    CHECK(formFactorFromWindows(0) == FormFactor::Unknown);
    CHECK(formFactorFromWindows(2) == FormFactor::Unknown);
    CHECK(formFactorFromWindows(10) == FormFactor::Unknown);
    CHECK(formFactorFromWindows(-1) == FormFactor::Unknown);
    CHECK(formFactorFromWindows(99) == FormFactor::Unknown);

    CHECK(percentFromScalar(0.0F) == 0U);
    CHECK(percentFromScalar(1.0F) == 100U);
    CHECK(percentFromScalar(0.5F) == 50U);
    CHECK(percentFromScalar(0.004F) == 0U);
    CHECK(percentFromScalar(0.006F) == 1U);
    CHECK(percentFromScalar(-0.5F) == 0U);
    CHECK(percentFromScalar(1.5F) == 100U);
    CHECK(percentFromScalar(std::numeric_limits<float>::quiet_NaN()) == 0U);
    CHECK(scalarFromPercent(0U) == 0.0F);
    CHECK(scalarFromPercent(100U) == 1.0F);
    CHECK(scalarFromPercent(250U) == 1.0F);
    // Every percent survives a round trip through the scalar.
    for (unsigned p = 0; p <= 100U; ++p) {
        CHECK(percentFromScalar(scalarFromPercent(p)) == p);
    }
}

std::optional<HostEvent> popEvent(EventQueue& q)
{
    const std::optional<QueuedEvent> e = q.pop(std::chrono::milliseconds(1));
    if (!e) {
        return std::nullopt;
    }
    return e->event;
}

void testEventQueue()
{
    EventQueue q;
    CHECK(!q.pop(std::chrono::milliseconds(1)).has_value());

    // Duplicates of a waiting audio event are dropped; order is kept.
    q.push(HostEvent::DevicesChanged);
    q.push(HostEvent::VolumeChanged);
    q.push(HostEvent::DevicesChanged);
    q.push(HostEvent::VolumeChanged);
    CHECK(q.size() == 2);
    CHECK(popEvent(q) == HostEvent::DevicesChanged);
    CHECK(popEvent(q) == HostEvent::VolumeChanged);
    CHECK(q.size() == 0);

    // A popped event can be queued again.
    q.push(HostEvent::DevicesChanged);
    CHECK(q.size() == 1);
    (void)q.pop(std::chrono::milliseconds(1));

    // Connect / disconnect are never merged: each one is a state change.
    q.push(HostEvent::HidConnected);
    q.push(HostEvent::HidDisconnected);
    q.push(HostEvent::HidConnected);
    q.push(HostEvent::HidDisconnected);
    CHECK(q.size() == 4);
    CHECK(popEvent(q) == HostEvent::HidConnected);
    CHECK(popEvent(q) == HostEvent::HidDisconnected);
    CHECK(popEvent(q) == HostEvent::HidConnected);
    CHECK(popEvent(q) == HostEvent::HidDisconnected);

    // Push from another thread wakes the waiting consumer.
    std::thread producer([&q] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        for (int i = 0; i < 1000; ++i) {
            q.push(HostEvent::VolumeChanged);
        }
        q.push(HostEvent::Quit);
    });
    bool sawQuit = false;
    int volumeEvents = 0;
    while (!sawQuit) {
        const std::optional<QueuedEvent> e = q.pop(std::chrono::seconds(5));
        CHECK(e.has_value());
        if (!e) {
            break;
        }
        sawQuit = (e->event == HostEvent::Quit);
        volumeEvents += (e->event == HostEvent::VolumeChanged) ? 1 : 0;
    }
    producer.join();
    CHECK(sawQuit);
    CHECK(volumeEvents >= 1 && volumeEvents <= 1000);
}

void testEventQueueFrames()
{
    EventQueue q(3);
    Frame a{};
    Frame b{};
    a[0] = 0x11;
    b[0] = 0x22;

    // Frames are carried with their content, in order, and never merged
    // (two identical frames are two requests).
    CHECK(q.pushFrame(a));
    q.push(HostEvent::DevicesChanged);
    CHECK(q.pushFrame(b));
    CHECK(q.pushFrame(b));
    std::optional<QueuedEvent> e = q.pop(std::chrono::milliseconds(1));
    CHECK(e && e->event == HostEvent::FrameReceived && e->frame == a);
    CHECK(popEvent(q) == HostEvent::DevicesChanged);
    e = q.pop(std::chrono::milliseconds(1));
    CHECK(e && e->event == HostEvent::FrameReceived && e->frame == b);

    // Bounded: with 3 frames waiting the next is dropped and counted;
    // other events still get in.
    CHECK(q.pushFrame(a));
    CHECK(q.pushFrame(a));
    CHECK(!q.pushFrame(b));
    CHECK(q.droppedFrames() == 1);
    q.push(HostEvent::HidDisconnected);
    CHECK(q.size() == 4);

    // Popping a frame frees a slot.
    for (int i = 0; i < 3; ++i) {
        CHECK(popEvent(q) == HostEvent::FrameReceived);
    }
    CHECK(q.pushFrame(b));
    CHECK(q.droppedFrames() == 1);
    CHECK(popEvent(q) == HostEvent::HidDisconnected);
    e = q.pop(std::chrono::milliseconds(1));
    CHECK(e && e->frame == b);
    CHECK(q.size() == 0);
}

void testHidMatch()
{
    HidInfo info;
    info.vendorId = 0x1209;
    info.productId = 0x0001;
    info.usagePage = 0xFF00;
    info.usage = 0x0001;
    info.inputReportBytes = 65;
    info.outputReportBytes = 65;
    CHECK(isController(info));
    CHECK(kReportBytes == 65);

    HidInfo other = info;
    other.vendorId = 0x0483;  // ST
    CHECK(!isController(other));
    other = info;
    other.productId = 0x0002;
    CHECK(!isController(other));
    other = info;
    other.usagePage = 0x0001;  // e.g. a keyboard collection of the same device
    CHECK(!isController(other));
    other = info;
    other.usage = 0x0002;
    CHECK(!isController(other));
    other = info;
    other.inputReportBytes = 64;  // a firmware with a report ID or another size
    CHECK(!isController(other));
    other = info;
    other.outputReportBytes = 0;  // input-only interface
    CHECK(!isController(other));
}

void testFrameText()
{
    ProtoMessage m{};
    m.command = PROTO_CMD_DEVICE_LIST;
    m.sequence = 3;
    m.u.device.generation = 1;
    m.u.device.total = 3;
    m.u.device.position = 1;
    m.u.device.device_index = 4;
    m.u.device.flags = PROTO_DEVICE_FLAG_DEFAULT;
    m.u.device.icon = PROTO_ICON_HEADPHONES;
    std::memcpy(m.u.device.name, "Kulakl\xC4\xB1k", 9);
    m.u.device.name_len = 9;
    CHECK(describeMessage(m) == "DEVICE_LIST seq=3 gen=1 2/3 index=4 default icon=headphones \"Kulakl\xC4\xB1k\"");

    m = ProtoMessage{};
    m.command = PROTO_CMD_DEVICE_LIST;
    m.u.device.device_index = PROTO_NO_DEVICE;
    CHECK(describeMessage(m) == "DEVICE_LIST seq=0 gen=0 empty");

    m = ProtoMessage{};
    m.command = PROTO_CMD_DEVICE_CHANGED;
    m.u.device_index = PROTO_NO_DEVICE;
    CHECK(describeMessage(m) == "DEVICE_CHANGED seq=0 index=none");

    m = ProtoMessage{};
    m.command = PROTO_CMD_ERROR;
    m.sequence = 7;
    m.u.error.code = PROTO_ERROR_UNKNOWN_DEVICE;
    m.u.error.ref_command = PROTO_CMD_SELECT_DEVICE;
    m.u.error.ref_sequence = 2;
    CHECK(describeMessage(m) == "ERROR seq=7 code=UNKNOWN_DEVICE ref_cmd=3 ref_seq=2");

    m = ProtoMessage{};
    m.command = PROTO_CMD_MUTE_CHANGED;
    m.u.muted = 1;
    CHECK(describeMessage(m) == "MUTE_CHANGED seq=0 muted=1");
}

}  // namespace

int main()
{
    testTruncateUtf8();
    testIcons();
    testRegistryIndices();
    testRegistryIndexWraps();
    testRegistryLimit();
    testRegistryEntries();
    testNothingBeforeFirstRequest();
    testGetDeviceList();
    testSequenceNumbers();
    testEmptyList();
    testSelectDevice();
    testSelectBeforeListSent();
    testSelectErrors();
    testVolumeAndMute();
    testWindowsChanges();
    testReconnect();
    testConnectSendsList();
    testBadFrames();
    testSendFailure();
    testAudioMapping();
    testEventQueue();
    testEventQueueFrames();
    testHidMatch();
    testFrameText();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
