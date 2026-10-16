// Abstraction of the Windows audio system used by the host logic.
//
// The Windows implementation (Core Audio / MMDevice) lives behind this
// interface so that the device model and the protocol controller can be
// tested without Windows. No Windows types appear here: strings are UTF-8
// and the form factor is our own enum, mapped from EndpointFormFactor by
// the Windows layer.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace stm32_audio::host {

// Kind of playback device, as far as the UI icon is concerned.
enum class FormFactor {
    Unknown,
    Speakers,
    Headphones,
    Headset,
    Display,        // HDMI / DisplayPort audio
    DigitalOutput,  // S/PDIF and other digital connections
};

// One active playback (render) endpoint.
struct AudioEndpoint {
    std::string id;    // opaque Windows endpoint ID; never sent to the STM32
    std::string name;  // friendly name, UTF-8
    FormFactor formFactor = FormFactor::Unknown;
};

class IAudioSystem {
public:
    virtual ~IAudioSystem() = default;

    // Active render endpoints, in the order the UI should show them.
    virtual std::vector<AudioEndpoint> renderEndpoints() = 0;

    // ID of the default render endpoint (console role); empty if none.
    virtual std::string defaultEndpointId() = 0;

    // Make @p id the default render endpoint. Returns false on failure.
    virtual bool setDefaultEndpoint(const std::string& id) = 0;

    // Master volume of the default endpoint, 0..100; nullopt if unavailable.
    virtual std::optional<unsigned> volumePercent() = 0;
    virtual bool setVolumePercent(unsigned percent) = 0;

    // Mute state of the default endpoint; nullopt if unavailable.
    virtual std::optional<bool> muted() = 0;
    virtual bool setMuted(bool muted) = 0;

    // Master volume of the active render endpoint @p id, 0..100; nullopt if
    // unavailable. Used for the volume limit on switch (ADR-004).
    virtual std::optional<unsigned> endpointVolumePercent(const std::string& id) = 0;
    virtual bool setEndpointVolumePercent(const std::string& id, unsigned percent) = 0;
};

}  // namespace stm32_audio::host
