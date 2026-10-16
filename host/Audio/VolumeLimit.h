// Volume limit on switch (ADR-004): when the settings record has it on and
// the STM32 selects a playback device whose volume is above the limit, the
// device is lowered to it before it becomes the default. Volumes are never
// raised and mute is not touched.
#pragma once

#include <optional>
#include <string>

#include "IAudioSystem.h"
#include "protocol.h"

namespace stm32_audio::host {

struct VolumeLimited {
    std::string endpointId;
    std::string name;
    unsigned from = 0;
    unsigned to = 0;
};

// Apply the limit of @p settings to @p endpoint. Returns the change when
// the device was lowered; nothing when the limit is off, the volume is at
// or below it, or the volume cannot be read or set.
std::optional<VolumeLimited> applyVolumeLimit(IAudioSystem& audio, const ProtoSettings& settings,
                                              const AudioEndpoint& endpoint);

}  // namespace stm32_audio::host
