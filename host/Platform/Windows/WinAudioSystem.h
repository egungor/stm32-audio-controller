// IAudioSystem on Windows Core Audio (MMDevice API).
//
// - Enumeration, default endpoint, volume and mute use documented APIs.
// - Setting the default endpoint uses IPolicyConfig (ADR-002).
// - Notifications (IMMNotificationClient, IAudioEndpointVolumeCallback)
//   arrive on Core Audio threads; they only push events into the
//   EventQueue. Everything else must be called from the consumer thread.
//
// COM must be initialised on the calling thread (CoInitializeEx) before
// construction and stay initialised until destruction.
#pragma once

#include <windows.h>

#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <string>

#include "EventQueue.h"
#include "IAudioSystem.h"

namespace stm32_audio::host::win {

class WinAudioSystem : public IAudioSystem {
public:
    explicit WinAudioSystem(EventQueue& events);
    ~WinAudioSystem() override;

    WinAudioSystem(const WinAudioSystem&) = delete;
    WinAudioSystem& operator=(const WinAudioSystem&) = delete;

    // False if the MMDevice enumerator could not be created; then every
    // other call fails or returns nothing.
    bool ok() const { return enumerator_ != nullptr; }

    std::vector<AudioEndpoint> renderEndpoints() override;
    std::string defaultEndpointId() override;
    bool setDefaultEndpoint(const std::string& id) override;
    std::optional<unsigned> volumePercent() override;
    bool setVolumePercent(unsigned percent) override;
    std::optional<bool> muted() override;
    bool setMuted(bool muted) override;
    std::optional<unsigned> endpointVolumePercent(const std::string& id) override;
    bool setEndpointVolumePercent(const std::string& id, unsigned percent) override;

    // Move the volume notification to the current default endpoint. Call
    // on the consumer thread after a HostEvent::DefaultChanged.
    void rebindVolumeNotifications();

private:
    class NotificationClient;
    class VolumeCallback;

    Microsoft::WRL::ComPtr<IAudioEndpointVolume> defaultVolume();
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> endpointVolume(const std::string& id);

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<NotificationClient> notificationClient_;
    Microsoft::WRL::ComPtr<VolumeCallback> volumeCallback_;
    Microsoft::WRL::ComPtr<IAudioEndpointVolume> watchedVolume_;
};

}  // namespace stm32_audio::host::win
