// Host-side device model: maps Windows endpoints to the protocol's
// session-local device_index values and builds the DEVICE_LIST entries.
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "IAudioSystem.h"
#include "protocol.h"

namespace stm32_audio::host {

// Truncate @p text to at most @p maxBytes bytes without splitting a UTF-8
// sequence, and drop NUL bytes (the protocol forbids them in names).
std::string truncateUtf8(const std::string& text, std::size_t maxBytes);

// Protocol icon for a form factor.
std::uint8_t iconFor(FormFactor formFactor);

class DeviceRegistry {
public:
    struct Device {
        std::uint8_t index;  // device_index sent to the STM32
        AudioEndpoint endpoint;
        std::string displayName;  // truncated UTF-8 name
    };

    // Replace the device set with @p endpoints (Windows order).
    //
    // - At most PROTO_MAX_DEVICES devices are kept; if the default
    //   device would be cut off, it replaces the last kept one.
    // - An endpoint that stays present keeps its index. New endpoints get
    //   the next free index in round-robin order (0..254), so an index that
    //   was just released is not handed to another device right away.
    // - generation() changes only if the list content (order, indices,
    //   names, icons) changed.
    //
    // Returns true if the list content changed.
    bool update(const std::vector<AudioEndpoint>& endpoints, const std::string& defaultId);

    const std::vector<Device>& devices() const { return devices_; }
    std::uint8_t generation() const { return generation_; }

    std::optional<std::uint8_t> indexOf(const std::string& endpointId) const;
    const Device* findByIndex(std::uint8_t index) const;

    // DEVICE_LIST payloads for the current list, in display order. One
    // entry with total = 0 when the list is empty. @p defaultId sets the
    // default flag.
    std::vector<ProtoDeviceEntry> entries(const std::string& defaultId) const;

private:
    std::uint8_t allocateIndex(const std::map<std::string, std::uint8_t>& kept);

    std::vector<Device> devices_;
    std::map<std::string, std::uint8_t> indexById_;
    std::uint8_t nextIndex_ = 0;
    std::uint8_t generation_ = 0;
};

}  // namespace stm32_audio::host
