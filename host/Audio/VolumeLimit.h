// Startup volume limit (ADR-004): when the settings record has it on,
// every active playback device whose volume is above the limit is lowered
// to it. Volumes are never raised and mute is not touched.
#pragma once

#include <string>
#include <vector>

#include "IAudioSystem.h"
#include "protocol.h"

namespace stm32_audio::host {

struct VolumeLimited {
    std::string endpointId;
    std::string name;
    unsigned from = 0;
    unsigned to = 0;
};

// Apply the limit of @p settings to every endpoint of @p audio. Returns the
// devices that were lowered (empty when the limit is off). A device whose
// volume cannot be read or set is skipped.
std::vector<VolumeLimited> applyVolumeLimit(IAudioSystem& audio, const ProtoSettings& settings);

}  // namespace stm32_audio::host
