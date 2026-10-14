#include "VolumeLimit.h"

#include <optional>

namespace stm32_audio::host {

std::vector<VolumeLimited> applyVolumeLimit(IAudioSystem& audio, const ProtoSettings& settings)
{
    std::vector<VolumeLimited> lowered;

    if (settings.volume_limit_on == 0U || settings.volume_limit > PROTO_MAX_VOLUME) {
        return lowered;
    }
    const unsigned limit = settings.volume_limit;
    for (const AudioEndpoint& e : audio.renderEndpoints()) {
        const std::optional<unsigned> volume = audio.endpointVolumePercent(e.id);
        if (volume && *volume > limit && audio.setEndpointVolumePercent(e.id, limit)) {
            lowered.push_back(VolumeLimited{e.id, e.name, *volume, limit});
        }
    }
    return lowered;
}

}  // namespace stm32_audio::host
