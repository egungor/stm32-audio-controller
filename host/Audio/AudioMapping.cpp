#include "AudioMapping.h"

#include <cmath>

namespace stm32_audio::host {

namespace {

// EndpointFormFactor values from mmdeviceapi.h.
enum : int {
    kRemoteNetworkDevice = 0,
    kSpeakers = 1,
    kLineLevel = 2,
    kHeadphones = 3,
    kMicrophone = 4,
    kHeadset = 5,
    kHandset = 6,
    kUnknownDigitalPassthrough = 7,
    kSpdif = 8,
    kDigitalAudioDisplayDevice = 9,
    kUnknownFormFactor = 10,
};

}  // namespace

FormFactor formFactorFromWindows(int endpointFormFactor)
{
    switch (endpointFormFactor) {
    case kSpeakers:
        return FormFactor::Speakers;
    case kHeadphones:
        return FormFactor::Headphones;
    case kHeadset:
    case kHandset:
        return FormFactor::Headset;
    case kDigitalAudioDisplayDevice:
        return FormFactor::Display;
    case kSpdif:
    case kUnknownDigitalPassthrough:
        return FormFactor::DigitalOutput;
    case kRemoteNetworkDevice:
    case kLineLevel:
    case kMicrophone:
    case kUnknownFormFactor:
    default:
        return FormFactor::Unknown;
    }
}

unsigned percentFromScalar(float scalar)
{
    if (!(scalar > 0.0F)) {  // also catches NaN
        return 0U;
    }
    if (scalar >= 1.0F) {
        return 100U;
    }
    return static_cast<unsigned>(std::lround(static_cast<double>(scalar) * 100.0));
}

float scalarFromPercent(unsigned percent)
{
    return static_cast<float>(percent > 100U ? 100U : percent) / 100.0F;
}

}  // namespace stm32_audio::host
