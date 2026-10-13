// Outgoing side of the HID transport, as seen by the protocol controller.
//
// The Windows HID implementation writes each frame as one output report.
// Incoming frames are passed to HostController::onFrame by the transport.
#pragma once

#include <array>
#include <cstdint>

#include "protocol.h"

namespace stm32_audio::host {

using Frame = std::array<std::uint8_t, PROTO_FRAME_SIZE>;

class IFrameSink {
public:
    virtual ~IFrameSink() = default;

    // Send one encoded protocol frame. Returns false if it could not be
    // sent (device gone); the controller does not retry.
    virtual bool sendFrame(const Frame& frame) = 0;
};

}  // namespace stm32_audio::host
