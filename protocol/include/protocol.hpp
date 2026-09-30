#pragma once

#include <cstdint>

namespace stm32_audio::protocol {

constexpr std::uint8_t kProtocolVersion = 0;

enum class Command : std::uint8_t {
    GetDeviceList   = 0x01,
    DeviceList      = 0x02,
    SelectDevice    = 0x03,
    DeviceChanged   = 0x04,
    SetVolume       = 0x05,
    VolumeChanged   = 0x06,
    SetMute         = 0x07,
    MuteChanged     = 0x08,
    Error            = 0x7F,
};

} // namespace stm32_audio::protocol
