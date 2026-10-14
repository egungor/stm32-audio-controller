#include "DeviceRegistry.h"

#include <algorithm>
#include <cstring>

#include "Preferences.h"

namespace stm32_audio::host {

namespace {

constexpr unsigned kIndexCount = PROTO_NO_DEVICE;  // valid indices: 0 .. 254

bool isContinuationByte(char c)
{
    return (static_cast<unsigned char>(c) & 0xC0U) == 0x80U;
}

bool sameContent(const DeviceRegistry::Device& a, const DeviceRegistry::Device& b)
{
    return a.index == b.index && a.endpoint.id == b.endpoint.id && a.displayName == b.displayName &&
           a.endpoint.formFactor == b.endpoint.formFactor && a.alias == b.alias && a.favorite == b.favorite;
}

}  // namespace

std::string truncateUtf8(const std::string& text, std::size_t maxBytes)
{
    std::string out;
    out.reserve(std::min(text.size(), maxBytes));
    for (char c : text) {
        if (c != '\0') {
            out.push_back(c);
        }
    }
    if (out.size() <= maxBytes) {
        return out;
    }
    std::size_t cut = maxBytes;
    // Back up to the start of the sequence that would be split.
    while (cut > 0 && isContinuationByte(out[cut])) {
        --cut;
    }
    out.resize(cut);
    return out;
}

std::uint8_t iconFor(FormFactor formFactor)
{
    switch (formFactor) {
    case FormFactor::Speakers:
        return PROTO_ICON_SPEAKERS;
    case FormFactor::Headphones:
        return PROTO_ICON_HEADPHONES;
    case FormFactor::Headset:
        return PROTO_ICON_HEADSET;
    case FormFactor::Display:
        return PROTO_ICON_DISPLAY;
    case FormFactor::DigitalOutput:
        return PROTO_ICON_DIGITAL;
    case FormFactor::Unknown:
        break;
    }
    return PROTO_ICON_GENERIC;
}

std::uint8_t DeviceRegistry::allocateIndex(const std::map<std::string, std::uint8_t>& kept)
{
    for (unsigned tries = 0; tries < kIndexCount; ++tries) {
        const std::uint8_t candidate = nextIndex_;
        nextIndex_ = static_cast<std::uint8_t>((nextIndex_ + 1U) % kIndexCount);
        const bool used = std::any_of(kept.begin(), kept.end(),
                                      [candidate](const auto& kv) { return kv.second == candidate; });
        if (!used) {
            return candidate;
        }
    }
    // Unreachable: at most PROTO_MAX_DEVICES (32) of 255 indices are in use.
    return 0;
}

bool DeviceRegistry::update(const std::vector<AudioEndpoint>& endpoints, const std::string& defaultId,
                            const Preferences* preferences)
{
    std::vector<AudioEndpoint> kept(endpoints.begin(),
                                    endpoints.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(
                                                            endpoints.size(), PROTO_MAX_DEVICES)));
    if (!defaultId.empty() && endpoints.size() > PROTO_MAX_DEVICES) {
        const auto inKept = std::find_if(kept.begin(), kept.end(),
                                         [&](const AudioEndpoint& e) { return e.id == defaultId; });
        const auto inAll = std::find_if(endpoints.begin(), endpoints.end(),
                                        [&](const AudioEndpoint& e) { return e.id == defaultId; });
        if (inKept == kept.end() && inAll != endpoints.end()) {
            kept.back() = *inAll;
        }
    }

    // Indices of endpoints that stay present.
    std::map<std::string, std::uint8_t> newIndexById;
    for (const AudioEndpoint& e : kept) {
        const auto it = indexById_.find(e.id);
        if (it != indexById_.end()) {
            newIndexById[e.id] = it->second;
        }
    }

    std::vector<Device> newDevices;
    newDevices.reserve(kept.size());
    for (const AudioEndpoint& e : kept) {
        if (newIndexById.count(e.id) == 0U) {
            newIndexById[e.id] = allocateIndex(newIndexById);
        }
        Device d{newIndexById[e.id], e, truncateUtf8(e.name, PROTO_MAX_NAME_BYTES), std::string(), false};
        const DevicePreferences* prefs = preferences != nullptr ? preferences->device(e.id) : nullptr;
        if (prefs != nullptr) {
            d.alias = truncateUtf8(prefs->alias, PROTO_MAX_ALIAS_BYTES);
            d.favorite = prefs->favorite;
        }
        newDevices.push_back(std::move(d));
    }

    const bool changed = newDevices.size() != devices_.size() ||
                         !std::equal(newDevices.begin(), newDevices.end(), devices_.begin(), sameContent);
    devices_ = std::move(newDevices);
    indexById_ = std::move(newIndexById);
    if (changed) {
        ++generation_;
    }
    return changed;
}

std::optional<std::uint8_t> DeviceRegistry::indexOf(const std::string& endpointId) const
{
    const auto it = indexById_.find(endpointId);
    if (it == indexById_.end()) {
        return std::nullopt;
    }
    return it->second;
}

const DeviceRegistry::Device* DeviceRegistry::findByIndex(std::uint8_t index) const
{
    for (const Device& d : devices_) {
        if (d.index == index) {
            return &d;
        }
    }
    return nullptr;
}

std::vector<ProtoDeviceEntry> DeviceRegistry::entries(const std::string& defaultId) const
{
    std::vector<ProtoDeviceEntry> out;

    if (devices_.empty()) {
        ProtoDeviceEntry empty{};
        empty.generation = generation_;
        empty.device_index = PROTO_NO_DEVICE;
        out.push_back(empty);
        return out;
    }

    for (std::size_t i = 0; i < devices_.size(); ++i) {
        const Device& d = devices_[i];
        ProtoDeviceEntry e{};
        e.generation = generation_;
        e.total = static_cast<std::uint8_t>(devices_.size());
        e.position = static_cast<std::uint8_t>(i);
        e.device_index = d.index;
        e.flags = static_cast<std::uint8_t>(((d.endpoint.id == defaultId) ? PROTO_DEVICE_FLAG_DEFAULT : 0U) |
                                            (d.favorite ? PROTO_DEVICE_FLAG_FAVORITE : 0U) |
                                            (d.alias.empty() ? 0U : PROTO_DEVICE_FLAG_ALIAS));
        e.icon = iconFor(d.endpoint.formFactor);
        e.name_len = static_cast<std::uint8_t>(d.displayName.size());
        std::memcpy(e.name, d.displayName.data(), d.displayName.size());
        e.name[e.name_len] = '\0';
        out.push_back(e);
    }
    return out;
}

std::vector<ProtoAliasEntry> DeviceRegistry::aliasEntries() const
{
    std::vector<ProtoAliasEntry> out;

    for (std::size_t i = 0; i < devices_.size(); ++i) {
        const Device& d = devices_[i];
        if (d.alias.empty()) {
            continue;
        }
        ProtoAliasEntry a{};
        a.generation = generation_;
        a.position = static_cast<std::uint8_t>(i);
        a.alias_len = static_cast<std::uint8_t>(d.alias.size());
        std::memcpy(a.alias, d.alias.data(), d.alias.size());
        a.alias[a.alias_len] = '\0';
        out.push_back(a);
    }
    return out;
}

}  // namespace stm32_audio::host
