// Human-readable one-line description of a protocol message, for logs and
// the test console.
#pragma once

#include <string>

#include "protocol.h"

namespace stm32_audio::host {

// e.g. "DEVICE_LIST seq=3 gen=1 2/3 index=4 default icon=headphones \"Kulaklık\""
std::string describeMessage(const ProtoMessage& msg);

}  // namespace stm32_audio::host
