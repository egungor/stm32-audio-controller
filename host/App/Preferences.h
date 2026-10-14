// User preferences kept by the host (ADR-004): per-device alias, favorite
// flag and display order, keyed by Windows endpoint ID, plus the board's UI
// settings record.
//
// In memory only; loading and saving belong to the caller (#59). Platform
// independent.
#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "IAudioSystem.h"
#include "protocol.h"

namespace stm32_audio::host {

struct DevicePreferences {
    std::string alias;  // UTF-8, at most PROTO_MAX_ALIAS_BYTES; empty: none
    bool favorite = false;
};

class Preferences {
public:
    Preferences();

    // nullptr if nothing is stored for @p endpointId.
    const DevicePreferences* device(const std::string& endpointId) const;

    // Truncated to PROTO_MAX_ALIAS_BYTES at a UTF-8 boundary; empty removes
    // the alias. Return true if something changed.
    bool setAlias(const std::string& endpointId, const std::string& alias);
    bool setFavorite(const std::string& endpointId, bool favorite);

    // @p endpoints (Windows order) in the user's order. Endpoints never seen
    // before are appended to the stored order, in Windows order.
    std::vector<AudioEndpoint> arrange(const std::vector<AudioEndpoint>& endpoints);

    // Move @p endpointId to @p newPosition within @p shownIds (the list the
    // board shows, in display order). Devices that are not shown keep their
    // place in the stored order. Return false if @p endpointId is not in
    // @p shownIds or @p newPosition is out of range.
    bool move(const std::vector<std::string>& shownIds, const std::string& endpointId, std::size_t newPosition);

    const ProtoSettings& settings() const { return settings_; }
    // Ignored (false) if a field is out of range; true if something changed.
    bool setSettings(const ProtoSettings& settings);

    // Whole state, for saving and loading.
    const std::vector<std::string>& order() const { return order_; }
    const std::map<std::string, DevicePreferences>& devices() const { return devices_; }
    void setOrder(std::vector<std::string> order);
    void setDevices(std::map<std::string, DevicePreferences> devices);

private:
    std::vector<std::string> order_;  // known endpoint IDs, user's order
    std::map<std::string, DevicePreferences> devices_;
    ProtoSettings settings_{};
};

}  // namespace stm32_audio::host
