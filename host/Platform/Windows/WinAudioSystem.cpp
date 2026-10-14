// INITGUID makes this translation unit define the property keys and GUIDs
// declared by the Windows headers below (PKEY_Device_FriendlyName,
// PKEY_AudioEndpoint_FormFactor).
#include <initguid.h>

#include "WinAudioSystem.h"

#include <functiondiscoverykeys_devpkey.h>

#include <atomic>

#include "AudioMapping.h"
#include "PolicyConfig.h"
#include "WinStrings.h"

using Microsoft::WRL::ComPtr;

namespace stm32_audio::host::win {

namespace {

// Minimal IUnknown implementation for the notification objects.
template <typename Interface>
class ComObject : public Interface {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG refs = --refs_;
        if (refs == 0) {
            delete this;
        }
        return refs;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** object) override
    {
        if (object == nullptr) {
            return E_POINTER;
        }
        if (riid == __uuidof(IUnknown) || riid == __uuidof(Interface)) {
            *object = static_cast<Interface*>(this);
            AddRef();
            return S_OK;
        }
        *object = nullptr;
        return E_NOINTERFACE;
    }

protected:
    virtual ~ComObject() = default;

private:
    std::atomic<ULONG> refs_{1};
};

std::string endpointId(IMMDevice* device)
{
    LPWSTR id = nullptr;
    std::string out;
    if (SUCCEEDED(device->GetId(&id))) {
        out = toUtf8(id);
        CoTaskMemFree(id);
    }
    return out;
}

}  // namespace

// Device added / removed / changed and default changed. Runs on a Core
// Audio thread: only queue events here.
class WinAudioSystem::NotificationClient : public ComObject<IMMNotificationClient> {
public:
    explicit NotificationClient(EventQueue& events) : events_(events) {}

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override
    {
        events_.push(HostEvent::DevicesChanged);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override
    {
        events_.push(HostEvent::DevicesChanged);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override
    {
        events_.push(HostEvent::DevicesChanged);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override
    {
        if (flow == eRender && role == eConsole) {
            events_.push(HostEvent::DefaultChanged);
        }
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY key) override
    {
        // Renames matter; the many other property updates do not.
        if (key.fmtid == PKEY_Device_FriendlyName.fmtid && key.pid == PKEY_Device_FriendlyName.pid) {
            events_.push(HostEvent::DevicesChanged);
        }
        return S_OK;
    }

private:
    EventQueue& events_;
};

class WinAudioSystem::VolumeCallback : public ComObject<IAudioEndpointVolumeCallback> {
public:
    explicit VolumeCallback(EventQueue& events) : events_(events) {}

    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA) override
    {
        events_.push(HostEvent::VolumeChanged);
        return S_OK;
    }

private:
    EventQueue& events_;
};

WinAudioSystem::WinAudioSystem(EventQueue& events)
{
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(enumerator_.GetAddressOf())))) {
        enumerator_.Reset();
        return;
    }
    // The ComObject starts with one reference, which Attach() takes over.
    notificationClient_.Attach(new NotificationClient(events));
    volumeCallback_.Attach(new VolumeCallback(events));
    if (FAILED(enumerator_->RegisterEndpointNotificationCallback(notificationClient_.Get()))) {
        notificationClient_.Reset();
    }
    rebindVolumeNotifications();
}

WinAudioSystem::~WinAudioSystem()
{
    if (watchedVolume_) {
        watchedVolume_->UnregisterControlChangeNotify(volumeCallback_.Get());
    }
    if (enumerator_ && notificationClient_) {
        enumerator_->UnregisterEndpointNotificationCallback(notificationClient_.Get());
    }
}

std::vector<AudioEndpoint> WinAudioSystem::renderEndpoints()
{
    std::vector<AudioEndpoint> out;
    ComPtr<IMMDeviceCollection> devices;
    UINT count = 0;

    if (!enumerator_ ||
        FAILED(enumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, devices.GetAddressOf())) ||
        FAILED(devices->GetCount(&count))) {
        return out;
    }

    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        ComPtr<IPropertyStore> props;
        if (FAILED(devices->Item(i, device.GetAddressOf())) ||
            FAILED(device->OpenPropertyStore(STGM_READ, props.GetAddressOf()))) {
            continue;
        }

        AudioEndpoint endpoint;
        endpoint.id = endpointId(device.Get());

        PROPVARIANT value;
        PropVariantInit(&value);
        if (SUCCEEDED(props->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) {
            endpoint.name = toUtf8(value.pwszVal);
        }
        PropVariantClear(&value);

        PropVariantInit(&value);
        if (SUCCEEDED(props->GetValue(PKEY_AudioEndpoint_FormFactor, &value)) && value.vt == VT_UI4) {
            endpoint.formFactor = formFactorFromWindows(static_cast<int>(value.ulVal));
        }
        PropVariantClear(&value);

        if (!endpoint.id.empty()) {
            out.push_back(std::move(endpoint));
        }
    }
    return out;
}

std::string WinAudioSystem::defaultEndpointId()
{
    ComPtr<IMMDevice> device;
    if (!enumerator_ ||
        FAILED(enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf()))) {
        return {};  // E_NOTFOUND when no playback device exists
    }
    return endpointId(device.Get());
}

bool WinAudioSystem::setDefaultEndpoint(const std::string& id)
{
    ComPtr<IPolicyConfig> policy;
    const std::wstring wideId = toWide(id);

    if (wideId.empty() ||
        FAILED(CoCreateInstance(kClsidPolicyConfigClient, nullptr, CLSCTX_ALL, kIidPolicyConfig,
                                reinterpret_cast<void**>(policy.GetAddressOf())))) {
        return false;
    }
    // Same roles as "Set as Default Device" in the Sound control panel.
    return SUCCEEDED(policy->SetDefaultEndpoint(wideId.c_str(), eConsole)) &&
           SUCCEEDED(policy->SetDefaultEndpoint(wideId.c_str(), eMultimedia));
}

ComPtr<IAudioEndpointVolume> WinAudioSystem::defaultVolume()
{
    ComPtr<IMMDevice> device;
    ComPtr<IAudioEndpointVolume> volume;

    if (enumerator_ &&
        SUCCEEDED(enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, device.GetAddressOf()))) {
        device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(volume.GetAddressOf()));
    }
    return volume;
}

ComPtr<IAudioEndpointVolume> WinAudioSystem::endpointVolume(const std::string& id)
{
    ComPtr<IMMDevice> device;
    ComPtr<IAudioEndpointVolume> volume;
    const std::wstring wideId = toWide(id);

    if (enumerator_ && !id.empty() && SUCCEEDED(enumerator_->GetDevice(wideId.c_str(), device.GetAddressOf()))) {
        device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                         reinterpret_cast<void**>(volume.GetAddressOf()));
    }
    return volume;
}

std::optional<unsigned> WinAudioSystem::endpointVolumePercent(const std::string& id)
{
    float scalar = 0.0F;
    const ComPtr<IAudioEndpointVolume> volume = endpointVolume(id);
    if (!volume || FAILED(volume->GetMasterVolumeLevelScalar(&scalar))) {
        return std::nullopt;
    }
    return percentFromScalar(scalar);
}

bool WinAudioSystem::setEndpointVolumePercent(const std::string& id, unsigned percent)
{
    const ComPtr<IAudioEndpointVolume> volume = endpointVolume(id);
    return volume && SUCCEEDED(volume->SetMasterVolumeLevelScalar(scalarFromPercent(percent), nullptr));
}

std::optional<unsigned> WinAudioSystem::volumePercent()
{
    float scalar = 0.0F;
    const ComPtr<IAudioEndpointVolume> volume = defaultVolume();
    if (!volume || FAILED(volume->GetMasterVolumeLevelScalar(&scalar))) {
        return std::nullopt;
    }
    return percentFromScalar(scalar);
}

bool WinAudioSystem::setVolumePercent(unsigned percent)
{
    const ComPtr<IAudioEndpointVolume> volume = defaultVolume();
    return volume && SUCCEEDED(volume->SetMasterVolumeLevelScalar(scalarFromPercent(percent), nullptr));
}

std::optional<bool> WinAudioSystem::muted()
{
    BOOL mute = FALSE;
    const ComPtr<IAudioEndpointVolume> volume = defaultVolume();
    if (!volume || FAILED(volume->GetMute(&mute))) {
        return std::nullopt;
    }
    return mute != FALSE;
}

bool WinAudioSystem::setMuted(bool mute)
{
    const ComPtr<IAudioEndpointVolume> volume = defaultVolume();
    return volume && SUCCEEDED(volume->SetMute(mute ? TRUE : FALSE, nullptr));
}

void WinAudioSystem::rebindVolumeNotifications()
{
    if (watchedVolume_) {
        watchedVolume_->UnregisterControlChangeNotify(volumeCallback_.Get());
        watchedVolume_.Reset();
    }
    ComPtr<IAudioEndpointVolume> volume = defaultVolume();
    if (volume && volumeCallback_ && SUCCEEDED(volume->RegisterControlChangeNotify(volumeCallback_.Get()))) {
        watchedVolume_ = volume;
    }
}

}  // namespace stm32_audio::host::win
