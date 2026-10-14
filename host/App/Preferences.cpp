#include "Preferences.h"

#include <algorithm>
#include <cstring>
#include <set>

#include "DeviceRegistry.h"

namespace stm32_audio::host {

Preferences::Preferences()
{
    Proto_DefaultSettings(&settings_);
}

const DevicePreferences* Preferences::device(const std::string& endpointId) const
{
    const auto it = devices_.find(endpointId);
    return it == devices_.end() ? nullptr : &it->second;
}

bool Preferences::setAlias(const std::string& endpointId, const std::string& alias)
{
    const std::string cut = truncateUtf8(alias, PROTO_MAX_ALIAS_BYTES);
    const DevicePreferences* current = device(endpointId);

    if ((current == nullptr && cut.empty()) || (current != nullptr && current->alias == cut)) {
        return false;
    }
    devices_[endpointId].alias = cut;
    return true;
}

bool Preferences::setFavorite(const std::string& endpointId, bool favorite)
{
    const DevicePreferences* current = device(endpointId);

    if ((current == nullptr && !favorite) || (current != nullptr && current->favorite == favorite)) {
        return false;
    }
    devices_[endpointId].favorite = favorite;
    return true;
}

std::vector<AudioEndpoint> Preferences::arrange(const std::vector<AudioEndpoint>& endpoints)
{
    for (const AudioEndpoint& e : endpoints) {
        if (std::find(order_.begin(), order_.end(), e.id) == order_.end()) {
            order_.push_back(e.id);
        }
    }

    std::vector<AudioEndpoint> out;
    out.reserve(endpoints.size());
    for (const std::string& id : order_) {
        const auto it =
            std::find_if(endpoints.begin(), endpoints.end(), [&](const AudioEndpoint& e) { return e.id == id; });
        if (it != endpoints.end()) {
            out.push_back(*it);
        }
    }
    return out;
}

bool Preferences::move(const std::vector<std::string>& shownIds, const std::string& endpointId,
                       std::size_t newPosition)
{
    const auto from = std::find(shownIds.begin(), shownIds.end(), endpointId);
    if (from == shownIds.end() || newPosition >= shownIds.size()) {
        return false;
    }

    std::vector<std::string> moved(shownIds);
    moved.erase(moved.begin() + (from - shownIds.begin()));
    moved.insert(moved.begin() + static_cast<std::ptrdiff_t>(newPosition), endpointId);

    // The shown devices occupy some slots of the stored order; give those
    // slots the new order and leave every other device where it is.
    for (const std::string& id : shownIds) {
        if (std::find(order_.begin(), order_.end(), id) == order_.end()) {
            order_.push_back(id);
        }
    }
    std::vector<std::size_t> slots;
    for (std::size_t i = 0; i < order_.size(); ++i) {
        if (std::find(shownIds.begin(), shownIds.end(), order_[i]) != shownIds.end()) {
            slots.push_back(i);
        }
    }
    const std::vector<std::string> before(order_);
    for (std::size_t k = 0; k < slots.size(); ++k) {
        order_[slots[k]] = moved[k];
    }
    return order_ != before;
}

bool Preferences::setSettings(const ProtoSettings& settings)
{
    if (Proto_SettingsValid(&settings) == 0) {
        return false;
    }
    if (std::memcmp(&settings_, &settings, sizeof(settings)) == 0) {
        return false;
    }
    settings_ = settings;
    return true;
}

void Preferences::setOrder(std::vector<std::string> order)
{
    // Keep the first occurrence of each ID.
    std::set<std::string> seen;
    order_.clear();
    for (std::string& id : order) {
        if (seen.insert(id).second) {
            order_.push_back(std::move(id));
        }
    }
}

void Preferences::setDevices(std::map<std::string, DevicePreferences> devices)
{
    devices_ = std::move(devices);
    for (auto& kv : devices_) {
        kv.second.alias = truncateUtf8(kv.second.alias, PROTO_MAX_ALIAS_BYTES);
    }
}

}  // namespace stm32_audio::host
