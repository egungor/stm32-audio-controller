// Firmware <-> host loopback test (M5).
//
// The STM32 protocol client (firmware/link/link_client.c) and the host
// HostController run against each other over a simulated USB link, with a
// fake Windows audio system. Time is simulated in 1 ms steps:
//
// - USB: one 64-byte report per direction every kUsbIntervalMs (interrupt
//   endpoints, ADR-003).
// - STM32 receive queue: kFirmwareRxDepth frames (USB_LINK_RX_DEPTH); when
//   it is full the USB interrupt drops the frame (App/Src/usb_link.c).
// - STM32 UI loop: every kFirmwareLoopMs, at most kFirmwareFramesPerRun
//   frames, then Link_Poll (App/Src/link_port.c). It can be "busy" (LVGL
//   drawing) for a while.
// - Host: the HID interface is opened some time after the board is
//   plugged in (WinHidLink scans once a second); reports sent while no
//   handle is open are lost.
//
// What runs here is the real protocol code on both sides; USB, the LCD and
// Windows are models. No test framework; returns non-zero on failure.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <optional>
#include <string>
#include <vector>

#include "DeviceRegistry.h"
#include "HostController.h"
#include "link_client.h"

using namespace stm32_audio::host;

namespace {

int g_checks = 0;
int g_failures = 0;
const char* g_scenario = "";

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++g_checks;                                                                       \
        if (!(cond)) {                                                                    \
            ++g_failures;                                                                 \
            std::printf("FAIL [%s] %s:%d: %s\n", g_scenario, __FILE__, __LINE__, #cond);  \
        }                                                                                 \
    } while (0)

// Mirrors firmware/STM32AudioController (usb_link.h, link_port.c, app.c).
constexpr unsigned kUsbIntervalMs = 4;  // bInterval 5 ms, rounded down by Windows
constexpr std::size_t kFirmwareRxDepth = 40;
constexpr unsigned kFirmwareLoopMs = 10;
constexpr unsigned kFirmwareFramesPerRun = 8;
constexpr unsigned kHostOpenDelayMs = 700;  // within one WinHidLink scan period

// --------------------------------------------------------------- Windows

class FakeAudio : public IAudioSystem {
public:
    std::vector<AudioEndpoint> endpoints;
    std::string defaultId;
    bool failSetDefault = false;
    std::vector<std::string> setDefaultCalls;
    unsigned volume = 40;
    bool mute = false;

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
        volume = percent;
        return true;
    }
    std::optional<bool> muted() override { return mute; }
    bool setMuted(bool m) override
    {
        mute = m;
        return true;
    }
};

AudioEndpoint endpoint(const std::string& id, const std::string& name, FormFactor ff)
{
    AudioEndpoint e;
    e.id = id;
    e.name = name;
    e.formFactor = ff;
    return e;
}

// ------------------------------------------------------------------- rig

// What the LCD would show, as driven by the link client's callbacks.
struct FirmwareScreen {
    bool waiting = true;
    std::vector<UiDevice> devices;
    std::uint8_t active = UI_NO_DEVICE;
    std::uint8_t pending = UI_NO_DEVICE;
    int listsShown = 0;
    int notices[2] = {0, 0};
};

struct WireStats {
    std::vector<std::uint8_t> toHost;    // commands, in order
    std::vector<std::uint8_t> toDevice;
    std::vector<std::uint8_t> errorCodesToHost;
    std::vector<std::uint8_t> errorCodesToDevice;
    unsigned firmwareRxDropped = 0;

    unsigned count(const std::vector<std::uint8_t>& v, std::uint8_t cmd) const
    {
        unsigned n = 0;
        for (std::uint8_t c : v) {
            n += (c == cmd) ? 1U : 0U;
        }
        return n;
    }
};

class Rig : public IFrameSink {
public:
    FakeAudio audio;
    FirmwareScreen screen;
    WireStats wire;
    LinkClient link{};
    std::optional<HostController> host;
    std::uint32_t now;

    // Start close to the 32-bit wrap: the firmware's tick wraps too.
    explicit Rig(std::uint32_t start = 0xFFFFF000U)
        : now(start), hostOpenAt_(start), firmwareNextRun_(start), firmwareBusyUntil_(start)
    {
        static const LinkCallbacks callbacks = {
            &Rig::fwSend, &Rig::fwWaiting, &Rig::fwDevices, &Rig::fwActive, &Rig::fwPending, &Rig::fwNotice, nullptr,
        };
        LinkCallbacks cb = callbacks;
        cb.ctx = this;
        Link_Init(&link, &cb);
    }

    Rig(const Rig&) = delete;
    Rig& operator=(const Rig&) = delete;

    // ---- the physical world

    void plugIn()
    {
        cable_ = true;
        hostOpenAt_ = now + kHostOpenDelayMs;
    }

    void unplug()
    {
        cable_ = false;
        hostOut_.clear();
        firmwareRx_.clear();
        firmwareTx_.clear();
        if (hostOpen_) {
            hostOpen_ = false;
            if (host) {
                host->onConnected();  // HidDisconnected
            }
        }
    }

    // stm32-audio-host started / stopped while the board stays plugged in.
    void startHost()
    {
        host.emplace(audio, *this);
        hostRunning_ = true;
        hostOpenAt_ = now + kHostOpenDelayMs;
    }

    void stopHost()
    {
        host.reset();
        hostRunning_ = false;
        hostOpen_ = false;
        hostOut_.clear();
    }

    // The STM32 UI loop does not run for @p ms (e.g. LVGL redraw).
    void firmwareBusy(unsigned ms) { firmwareBusyUntil_ = now + ms; }

    // ---- Windows (notifications reach the host's consumer thread at once)

    void windowsSetDefault(const std::string& id)
    {
        audio.defaultId = id;
        if (host) {
            host->onDefaultChanged();
        }
    }

    void windowsAdd(const AudioEndpoint& e)
    {
        audio.endpoints.push_back(e);
        if (host) {
            host->onDevicesChanged();
        }
    }

    // Removing the default makes Windows pick another one (first left).
    void windowsRemove(const std::string& id)
    {
        for (std::size_t i = 0; i < audio.endpoints.size(); ++i) {
            if (audio.endpoints[i].id == id) {
                audio.endpoints.erase(audio.endpoints.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
        }
        const bool wasDefault = (audio.defaultId == id);
        if (wasDefault) {
            audio.defaultId = audio.endpoints.empty() ? std::string() : audio.endpoints.front().id;
        }
        if (host) {
            host->onDevicesChanged();
            if (wasDefault) {
                host->onDefaultChanged();
            }
        }
    }

    void windowsVolume(unsigned percent)
    {
        audio.volume = percent;
        if (host) {
            host->onVolumeChanged();
        }
    }

    // ---- the user

    void tap(std::uint8_t deviceId) { Link_Select(&link, deviceId, now); }

    std::optional<std::uint8_t> shownIdOf(const std::string& name) const
    {
        for (const UiDevice& d : screen.devices) {
            if (name == d.name) {
                return d.id;
            }
        }
        return std::nullopt;
    }

    // ---- time

    void run(unsigned ms)
    {
        for (unsigned i = 0; i < ms; ++i) {
            step();
        }
    }

    // ---- checks

    bool hostOpen() const { return hostOpen_; }

    // The screen shows exactly the host's current list and the Windows
    // default, with nothing pending.
    bool inSync() const
    {
        if (!host || screen.waiting || screen.pending != UI_NO_DEVICE) {
            return false;
        }
        const std::vector<DeviceRegistry::Device>& devices = host->registry().devices();
        if (screen.devices.size() != devices.size()) {
            return false;
        }
        for (std::size_t i = 0; i < devices.size(); ++i) {
            const UiDevice& shown = screen.devices[i];
            if (shown.id != devices[i].index || shown.icon != iconFor(devices[i].endpoint.formFactor) ||
                devices[i].displayName != shown.name) {
                return false;
            }
        }
        const std::optional<std::uint8_t> def = host->registry().indexOf(audio.defaultId);
        return screen.active == (def ? *def : UI_NO_DEVICE);
    }

    // IFrameSink: HostController -> WinHidLink::sendFrame.
    bool sendFrame(const Frame& frame) override
    {
        if (!hostOpen_) {
            return false;
        }
        hostOut_.push_back(frame);
        return true;
    }

private:
    static Rig& self(void* ctx) { return *static_cast<Rig*>(ctx); }

    // UsbLink_Send: the firmware's IN endpoint queue (blocking send).
    static bool fwSend(const std::uint8_t frame[PROTO_FRAME_SIZE], void* ctx)
    {
        Rig& r = self(ctx);
        if (!r.cable_) {
            return false;
        }
        Frame f{};
        std::memcpy(f.data(), frame, f.size());
        r.firmwareTx_.push_back(f);
        return true;
    }
    static void fwWaiting(void* ctx)
    {
        FirmwareScreen& s = self(ctx).screen;
        s.waiting = true;
        s.devices.clear();
        s.active = UI_NO_DEVICE;
        s.pending = UI_NO_DEVICE;
    }
    static void fwDevices(const UiDevice* devices, std::size_t count, void* ctx)
    {
        FirmwareScreen& s = self(ctx).screen;
        s.waiting = false;
        s.devices.assign(devices, devices + count);
        s.pending = UI_NO_DEVICE;
        ++s.listsShown;
    }
    static void fwActive(std::uint8_t id, void* ctx)
    {
        FirmwareScreen& s = self(ctx).screen;
        s.active = id;
        s.pending = UI_NO_DEVICE;  // Ui_SetActive clears the pending mark
    }
    static void fwPending(std::uint8_t id, void* ctx) { self(ctx).screen.pending = id; }
    static void fwNotice(LinkNotice n, void* ctx) { ++self(ctx).screen.notices[n]; }

    static bool reached(std::uint32_t now, std::uint32_t at)
    {
        return static_cast<std::int32_t>(now - at) >= 0;
    }

    static void note(const Frame& f, std::vector<std::uint8_t>& cmds, std::vector<std::uint8_t>& errors)
    {
        ProtoMessage msg{};
        if (Proto_Decode(f.data(), &msg) != PROTO_OK) {
            cmds.push_back(0xFF);
            return;
        }
        cmds.push_back(msg.command);
        if (msg.command == PROTO_CMD_ERROR) {
            errors.push_back(msg.u.error.code);
        }
    }

    void step()
    {
        ++now;

        // Host opens the HID interface (WinHidLink scan) -> HidConnected.
        if (cable_ && hostRunning_ && !hostOpen_ && reached(now, hostOpenAt_)) {
            hostOpen_ = true;
            host->onConnected();
        }

        // USB, one report per direction per interval.
        if (cable_ && now % kUsbIntervalMs == 0) {
            if (!hostOut_.empty()) {
                const Frame f = hostOut_.front();
                hostOut_.pop_front();
                note(f, wire.toDevice, wire.errorCodesToDevice);
                if (firmwareRx_.size() < kFirmwareRxDepth) {
                    firmwareRx_.push_back(f);
                } else {
                    ++wire.firmwareRxDropped;  // usb_link.c: rx_dropped
                }
            }
            if (!firmwareTx_.empty()) {
                const Frame f = firmwareTx_.front();
                firmwareTx_.pop_front();
                if (hostOpen_) {  // no handle open: Windows discards the report
                    note(f, wire.toHost, wire.errorCodesToHost);
                    host->onFrame(f);
                }
            }
        }

        // STM32 UI loop (LinkPort_Run).
        if (reached(now, firmwareNextRun_) && reached(now, firmwareBusyUntil_)) {
            firmwareNextRun_ = now + kFirmwareLoopMs;
            if (cable_ != firmwareConfigured_) {
                firmwareConfigured_ = cable_;
                if (cable_) {
                    Link_OnConnected(&link, now);
                } else {
                    Link_OnDisconnected(&link);
                }
            }
            for (unsigned i = 0; i < kFirmwareFramesPerRun && !firmwareRx_.empty(); ++i) {
                const Frame f = firmwareRx_.front();
                firmwareRx_.pop_front();
                Link_OnFrame(&link, f.data(), now);
            }
            Link_Poll(&link, now);
        }
    }

    bool cable_ = false;
    bool hostRunning_ = false;
    bool hostOpen_ = false;
    std::uint32_t hostOpenAt_ = 0;
    bool firmwareConfigured_ = false;
    std::uint32_t firmwareNextRun_ = 0;
    std::uint32_t firmwareBusyUntil_ = 0;
    std::deque<Frame> hostOut_;
    std::deque<Frame> firmwareRx_;
    std::deque<Frame> firmwareTx_;
};

// Three devices, default = headphones.
void addHomeSetup(FakeAudio& audio)
{
    audio.endpoints = {
        endpoint("{spk}", "Hoparl\xC3\xB6r", FormFactor::Speakers),
        endpoint("{hp}", "Kulakl\xC4\xB1k", FormFactor::Headphones),
        endpoint("{hdmi}", "LG HDR 4K", FormFactor::Display),
    };
    audio.defaultId = "{hp}";
}

// Host running, board plugged in, list shown.
void connect(Rig& r)
{
    r.startHost();
    r.plugIn();
    r.run(3000);
}

// ------------------------------------------------------------- scenarios

void testConnect()
{
    g_scenario = "connect";
    Rig r;
    addHomeSetup(r.audio);
    r.startHost();
    r.plugIn();
    CHECK(r.screen.waiting);

    r.run(3000);
    CHECK(r.inSync());
    CHECK(r.link.state == LINK_STATE_READY);
    CHECK(r.screen.devices.size() == 3);
    CHECK(r.shownIdOf("Kulakl\xC4\xB1k") == r.screen.active);
    CHECK(r.screen.devices.size() == 3 && r.screen.devices[0].icon == UI_ICON_SPEAKERS &&
          r.screen.devices[1].icon == UI_ICON_HEADPHONES && r.screen.devices[2].icon == UI_ICON_DISPLAY);
    // The first GET_DEVICE_LIST went out before the host had the device
    // open and was lost; the 1 s retry got through.
    CHECK(r.wire.count(r.wire.toHost, PROTO_CMD_GET_DEVICE_LIST) == 1);
    CHECK(r.wire.count(r.wire.toDevice, PROTO_CMD_DEVICE_LIST) == 3);
    CHECK(r.wire.errorCodesToHost.empty());
    CHECK(r.wire.errorCodesToDevice.empty());
    CHECK(r.wire.firmwareRxDropped == 0);
    CHECK(r.link.frames_rejected == 0);
    CHECK(r.host->stats().framesRejected == 0 && r.host->stats().sendFailures == 0);
}

void testHostStartedLate()
{
    g_scenario = "host started late";
    Rig r;
    addHomeSetup(r.audio);
    r.plugIn();
    r.run(5000);
    CHECK(r.screen.waiting);
    CHECK(r.screen.listsShown == 0);

    r.startHost();
    r.run(2500);
    CHECK(r.inSync());
    CHECK(r.wire.errorCodesToHost.empty() && r.wire.errorCodesToDevice.empty());
}

void testSelectFromBoard()
{
    g_scenario = "select from board";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);
    const std::uint8_t spk = *r.shownIdOf("Hoparl\xC3\xB6r");
    const unsigned changedBefore = r.wire.count(r.wire.toDevice, PROTO_CMD_DEVICE_CHANGED);

    r.tap(spk);
    CHECK(r.screen.pending == spk);
    CHECK(r.screen.active != spk);  // not active before the host confirms
    r.run(100);
    CHECK(r.audio.setDefaultCalls == std::vector<std::string>{"{spk}"});
    CHECK(r.audio.defaultId == "{spk}");
    CHECK(r.screen.active == spk);
    CHECK(r.screen.pending == UI_NO_DEVICE);

    // Windows' own notification of the switch comes later: no second
    // DEVICE_CHANGED, no new list.
    r.windowsSetDefault("{spk}");
    r.run(100);
    CHECK(r.wire.count(r.wire.toDevice, PROTO_CMD_DEVICE_CHANGED) == changedBefore + 1);
    CHECK(r.wire.count(r.wire.toDevice, PROTO_CMD_DEVICE_LIST) == 3);
    CHECK(r.inSync());
    CHECK(r.screen.notices[LINK_NOTICE_SELECT_FAILED] == 0 && r.screen.notices[LINK_NOTICE_HOST_TIMEOUT] == 0);
}

void testWindowsDefaultChange()
{
    g_scenario = "Windows default change";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);
    const unsigned listsBefore = r.wire.count(r.wire.toDevice, PROTO_CMD_DEVICE_LIST);

    r.windowsSetDefault("{hdmi}");
    r.run(100);
    CHECK(r.inSync());
    CHECK(r.screen.active == *r.shownIdOf("LG HDR 4K"));
    CHECK(r.wire.count(r.wire.toDevice, PROTO_CMD_DEVICE_LIST) == listsBefore);
}

void testDevicesAddedAndRemoved()
{
    g_scenario = "devices added and removed";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);
    const std::uint8_t hp = *r.shownIdOf("Kulakl\xC4\xB1k");
    const std::uint8_t hdmi = *r.shownIdOf("LG HDR 4K");

    r.windowsAdd(endpoint("{usb}", "USB Headset", FormFactor::Headset));
    r.run(200);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 4);
    CHECK(r.shownIdOf("Kulakl\xC4\xB1k") == hp);  // indices are kept
    CHECK(r.shownIdOf("LG HDR 4K") == hdmi);
    CHECK(r.screen.active == hp);

    r.windowsRemove("{spk}");
    r.run(200);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 3);
    CHECK(!r.shownIdOf("Hoparl\xC3\xB6r"));
    CHECK(r.shownIdOf("Kulakl\xC4\xB1k") == hp);
    CHECK(r.wire.errorCodesToHost.empty() && r.wire.errorCodesToDevice.empty());
}

void testDefaultDeviceRemoved()
{
    g_scenario = "default device removed";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);

    r.windowsRemove("{hp}");  // Windows falls back to the speakers
    r.run(200);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 2);
    CHECK(r.screen.active == *r.shownIdOf("Hoparl\xC3\xB6r"));
}

void testAllDevicesRemoved()
{
    g_scenario = "all devices removed";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);

    r.windowsRemove("{spk}");
    r.windowsRemove("{hdmi}");
    r.windowsRemove("{hp}");
    r.run(300);
    CHECK(r.inSync());
    CHECK(!r.screen.waiting);
    CHECK(r.screen.devices.empty());
    CHECK(r.screen.active == UI_NO_DEVICE);

    r.windowsAdd(endpoint("{spk}", "Hoparl\xC3\xB6r", FormFactor::Speakers));
    r.windowsSetDefault("{spk}");
    r.run(200);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 1);
}

void testWindowsRefusesSwitch()
{
    g_scenario = "Windows refuses the switch";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);
    const std::uint8_t hp = *r.shownIdOf("Kulakl\xC4\xB1k");
    r.audio.failSetDefault = true;

    r.tap(*r.shownIdOf("LG HDR 4K"));
    r.run(100);
    CHECK(r.screen.notices[LINK_NOTICE_SELECT_FAILED] == 1);
    CHECK(r.screen.active == hp);
    CHECK(r.inSync());
    CHECK(r.wire.errorCodesToDevice == std::vector<std::uint8_t>{PROTO_ERROR_OPERATION_FAILED});
    CHECK(r.wire.errorCodesToHost.empty());

    // Nothing else happens afterwards (no timeout notice, no re-request).
    const std::size_t framesBefore = r.wire.toHost.size();
    r.run(5000);
    CHECK(r.screen.notices[LINK_NOTICE_HOST_TIMEOUT] == 0);
    CHECK(r.wire.toHost.size() == framesBefore);
}

void testSelectRacesDeviceRemoval()
{
    g_scenario = "select races device removal";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);

    // The user taps the speakers just as Windows removes them; the new
    // list is still on its way when SELECT_DEVICE leaves the board.
    const std::uint8_t spk = *r.shownIdOf("Hoparl\xC3\xB6r");
    r.windowsRemove("{spk}");
    r.tap(spk);
    r.run(3000);
    CHECK(r.audio.setDefaultCalls.empty());
    CHECK(r.wire.errorCodesToDevice == std::vector<std::uint8_t>{PROTO_ERROR_UNKNOWN_DEVICE});
    // The new list arrives before the ERROR and drops the pending mark
    // (the tapped device is no longer on screen), so no notice.
    CHECK(r.screen.notices[LINK_NOTICE_SELECT_FAILED] == 0);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 2);
}

void testUsbUnplugAndReplug()
{
    g_scenario = "USB unplug and replug";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);

    r.unplug();
    r.run(50);
    CHECK(r.screen.waiting);

    // Windows changes while the board is away: nothing is sent.
    r.windowsAdd(endpoint("{usb}", "USB Headset", FormFactor::Headset));
    r.windowsSetDefault("{usb}");
    r.run(1000);
    CHECK(r.host->stats().sendFailures == 0);

    r.plugIn();
    r.run(3000);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 4);
    CHECK(r.screen.active == *r.shownIdOf("USB Headset"));
}

void testUnplugDuringList()
{
    g_scenario = "unplug during the list";
    Rig r;
    addHomeSetup(r.audio);
    for (int i = 0; i < 20; ++i) {
        r.audio.endpoints.push_back(
            endpoint("{x" + std::to_string(i) + "}", "Device " + std::to_string(i), FormFactor::Unknown));
    }
    r.startHost();
    r.plugIn();
    // The first GET_DEVICE_LIST is lost (host not open yet); the retry
    // after 1 s starts the 26-frame answer. Unplug half way through it.
    r.run(1000 + 50);
    CHECK(r.screen.waiting);
    r.unplug();
    r.run(100);
    r.plugIn();
    r.run(3000);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 23);
}

void testHostTimeoutAndRecovery()
{
    g_scenario = "host stops answering";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);

    // stm32-audio-host is closed; the board stays plugged in and does not
    // know. The user taps a device.
    r.stopHost();
    r.tap(*r.shownIdOf("LG HDR 4K"));
    r.run(LINK_RESPONSE_TIMEOUT_MS + 100);
    CHECK(r.screen.notices[LINK_NOTICE_HOST_TIMEOUT] == 1);
    CHECK(r.screen.waiting);

    r.run(5000);  // keeps asking for the list
    CHECK(r.screen.waiting);

    r.startHost();
    r.run(2500);
    CHECK(r.inSync());
    CHECK(r.audio.setDefaultCalls.empty());  // the lost tap is not replayed
}

// The board keeps the list of the previous host session after the host
// program restarts. Device indices are session-local (protocol.md), so the
// new session must not act on an index from the old one.
void testHostRestartKeepsOldIndices()
{
    g_scenario = "host restart, stale index";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);
    const std::uint8_t oldHdmi = *r.shownIdOf("LG HDR 4K");  // index 2

    r.stopHost();
    r.run(500);
    // While the host is closed: the speakers go away and a DAC arrives.
    r.audio.endpoints.erase(r.audio.endpoints.begin());
    r.audio.endpoints.push_back(endpoint("{dac}", "USB DAC", FormFactor::DigitalOutput));

    r.startHost();
    r.run(1000);
    // A Windows notification reaches the new session before the board
    // asks for anything; the new registry numbers {hp}=0 {hdmi}=1 {dac}=2.
    r.windowsVolume(55);
    r.windowsAdd(endpoint("{bt}", "Bluetooth", FormFactor::Headphones));
    r.run(100);

    // The board still shows the old list; the user taps the LG display.
    CHECK(r.link.state == LINK_STATE_READY);
    r.tap(oldHdmi);
    r.run(3000);
    // Old index 2 must not switch Windows to whatever has index 2 now.
    CHECK(r.audio.setDefaultCalls.empty());
    CHECK(r.wire.errorCodesToDevice == std::vector<std::uint8_t>{PROTO_ERROR_UNKNOWN_DEVICE});
    CHECK(r.screen.notices[LINK_NOTICE_SELECT_FAILED] == 1);
    CHECK(r.inSync());  // the board fetched the new list
}

void testManyDevicesAndLongNames()
{
    g_scenario = "40 devices, long names";
    Rig r;
    // 40 endpoints; long names with 2-byte characters across the 48-byte
    // limit; the default is past the 32-device cut.
    for (int i = 0; i < 40; ++i) {
        std::string name = "\xC3\x87\xC4\xB1k\xC4\xB1\xC5\x9F " + std::to_string(i) + " ";
        while (name.size() < 60) {
            name += "\xC4\x9F\xC3\xBC\xC5\x9F";  // ğüş
        }
        r.audio.endpoints.push_back(endpoint("{d" + std::to_string(i) + "}", name, FormFactor::Speakers));
    }
    r.audio.defaultId = "{d39}";
    connect(r);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 32);
    CHECK(r.screen.active != UI_NO_DEVICE);
    bool lengthsOk = true;
    for (const UiDevice& d : r.screen.devices) {
        const std::size_t len = std::strlen(d.name);
        // Never more than 48 bytes, never a cut UTF-8 sequence.
        lengthsOk = lengthsOk && len <= UI_MAX_NAME_BYTES && len > 40 &&
                    (static_cast<unsigned char>(d.name[len - 1]) & 0xC0U) != 0xC0U;
    }
    CHECK(lengthsOk);
    CHECK(r.wire.firmwareRxDropped == 0);
}

void testVolumeFramesIgnoredByBoard()
{
    g_scenario = "volume frames";
    Rig r;
    addHomeSetup(r.audio);
    connect(r);
    const int lists = r.screen.listsShown;

    r.windowsVolume(10);
    r.windowsVolume(20);
    r.run(100);
    CHECK(r.wire.count(r.wire.toDevice, PROTO_CMD_VOLUME_CHANGED) == 3);  // 1 on connect + 2
    CHECK(r.wire.errorCodesToHost.empty());
    CHECK(r.host->stats().errorsReceived == 0);
    CHECK(r.screen.listsShown == lists);
    CHECK(r.inSync());
}

// The STM32 UI loop is held up (LVGL redraw) while a long list arrives.
void testBoardBusyDuringLongList()
{
    g_scenario = "board busy during a long list";
    Rig r;
    addHomeSetup(r.audio);
    for (int i = 0; i < 25; ++i) {
        r.audio.endpoints.push_back(
            endpoint("{x" + std::to_string(i) + "}", "Device " + std::to_string(i), FormFactor::Unknown));
    }
    connect(r);
    CHECK(r.inSync());

    // A device is added: the host sends a new 29-entry list. The board is
    // busy for 100 ms just then; the receive queue holds the frames.
    r.windowsAdd(endpoint("{usb}", "USB Headset", FormFactor::Headset));
    r.firmwareBusy(100);
    r.run(500);
    CHECK(r.wire.firmwareRxDropped == 0);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 29);

    // Two devices arrive one after the other (two lists, 61 frames) while
    // the board is busy for 400 ms: the queue overflows and the second
    // list loses its tail. The board asks again 1 s after the last entry.
    const unsigned requests = r.wire.count(r.wire.toHost, PROTO_CMD_GET_DEVICE_LIST);
    r.firmwareBusy(400);
    r.windowsAdd(endpoint("{usb2}", "USB DAC", FormFactor::DigitalOutput));
    r.windowsAdd(endpoint("{usb3}", "Dock", FormFactor::Speakers));
    r.windowsSetDefault("{usb2}");
    r.run(600);
    CHECK(r.wire.firmwareRxDropped > 0);
    CHECK(!r.screen.waiting);
    r.run(2000);
    CHECK(r.wire.count(r.wire.toHost, PROTO_CMD_GET_DEVICE_LIST) == requests + 1);
    CHECK(r.inSync());
    CHECK(r.screen.devices.size() == 31);
    CHECK(r.screen.active == *r.shownIdOf("USB DAC"));
}

}  // namespace

int main()
{
    testConnect();
    testHostStartedLate();
    testSelectFromBoard();
    testWindowsDefaultChange();
    testDevicesAddedAndRemoved();
    testDefaultDeviceRemoved();
    testAllDevicesRemoved();
    testWindowsRefusesSwitch();
    testSelectRacesDeviceRemoval();
    testUsbUnplugAndReplug();
    testUnplugDuringList();
    testHostTimeoutAndRecovery();
    testHostRestartKeepsOldIndices();
    testManyDevicesAndLongNames();
    testVolumeFramesIgnoredByBoard();
    testBoardBusyDuringLongList();

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
