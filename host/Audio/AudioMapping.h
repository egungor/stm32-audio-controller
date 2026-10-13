// Conversions between Windows Core Audio values and the host model.
// Portable (no Windows headers) so they can be unit tested on any OS.
#pragma once

#include "IAudioSystem.h"

namespace stm32_audio::host {

// Map a Windows EndpointFormFactor value (mmdeviceapi.h) to FormFactor.
// Takes the raw integer so this file needs no Windows header.
FormFactor formFactorFromWindows(int endpointFormFactor);

// Master volume scalar (0.0 .. 1.0) to percent (0 .. 100), rounded to
// nearest. Out-of-range and NaN inputs are clamped.
unsigned percentFromScalar(float scalar);

// Percent (clamped to 0 .. 100) to master volume scalar.
float scalarFromPercent(unsigned percent);

}  // namespace stm32_audio::host
