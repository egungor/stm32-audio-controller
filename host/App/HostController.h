// Protocol v1 controller of the host application (protocol/protocol.md,
// "Exchanges").
//
// Glues the STM32 (frames in through onFrame, out through IFrameSink) to
// the audio system (IAudioSystem). Platform independent; the Windows layer
// owns the HID device and the Core Audio notifications and forwards them.
//
// Not thread-safe: call every method from one thread. Core Audio delivers
// notifications on its own threads, so the Windows layer must queue them
// and call onDevicesChanged() / onDefaultChanged() / onVolumeChanged()
// from the thread that also calls onFrame().
#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "DeviceRegistry.h"
#include "IAudioSystem.h"
#include "IFrameSink.h"
#include "protocol.h"

namespace stm32_audio::host {

class HostController {
public:
    struct Stats {
        unsigned framesSent = 0;
        unsigned sendFailures = 0;
        unsigned framesRejected = 0;  // failed Proto_Decode
        unsigned framesIgnored = 0;   // valid, but not for the host (wrong direction)
        unsigned errorsReceived = 0;  // ERROR frames from the STM32
    };

    HostController(IAudioSystem& audio, IFrameSink& sink);

    // A frame arrived from the STM32.
    void onFrame(const Frame& frame);

    // Windows reported a change. Each sends only what differs from what
    // the STM32 was last told, and nothing before the STM32 has asked for
    // the device list since the last connect (it always starts with
    // GET_DEVICE_LIST, which is answered in full).
    void onDevicesChanged();  // endpoints added, removed or renamed
    void onDefaultChanged();
    void onVolumeChanged();   // volume or mute of the default endpoint

    // The HID device was (re)connected: forget what was sent, so the next
    // GET_DEVICE_LIST or change is sent in full.
    void onConnected();

    const DeviceRegistry& registry() const { return registry_; }
    const Stats& stats() const { return stats_; }

private:
    bool synced() const { return sentGeneration_.has_value(); }
    void handleGetDeviceList();
    void handleSelectDevice(const ProtoMessage& msg);
    void handleSetVolume(const ProtoMessage& msg);
    void handleSetMute(const ProtoMessage& msg);

    void refreshRegistry();
    void sendDeviceList();
    void sendDefault(bool force);
    void sendVolumeAndMute(bool force);
    void sendError(ProtoErrorCode code, std::uint8_t refCommand, std::uint8_t refSequence);
    void send(ProtoMessage msg);

    IAudioSystem& audio_;
    IFrameSink& sink_;
    DeviceRegistry registry_;
    Stats stats_;
    std::uint8_t sequence_ = 0;

    // Last values sent to the STM32 (nullopt: not sent since connect).
    std::optional<std::uint8_t> sentGeneration_;
    std::optional<std::uint8_t> sentDefault_;
    std::optional<unsigned> sentVolume_;
    std::optional<bool> sentMuted_;
};

}  // namespace stm32_audio::host
