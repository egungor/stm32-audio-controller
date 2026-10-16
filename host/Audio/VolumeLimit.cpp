#include "VolumeLimit.h"

namespace stm32_audio::host {

std::optional<VolumeLimited> applyVolumeLimit(IAudioSystem& audio, const ProtoSettings& settings,
                                              const AudioEndpoint& endpoint)
{
    if (settings.volume_limit_on == 0U || settings.volume_limit > PROTO_MAX_VOLUME) {
        return std::nullopt;
    }
    const unsigned limit = settings.volume_limit;
    const std::optional<unsigned> volume = audio.endpointVolumePercent(endpoint.id);
    if (volume && *volume > limit && audio.setEndpointVolumePercent(endpoint.id, limit)) {
        return VolumeLimited{endpoint.id, endpoint.name, *volume, limit};
    }
    return std::nullopt;
}

}  // namespace stm32_audio::host
