# ADR-002: Switching the Windows default playback device

## Status

Proposed — implemented in `host/Platform/Windows/` (issue #28); to be
confirmed by the developer after testing on Windows.

## Context

The product's main function is to change the Windows default playback
device. The Core Audio APIs Microsoft documents (MMDevice,
`IAudioEndpointVolume`, `IMMNotificationClient`) can enumerate endpoints,
read the default endpoint, control volume and report changes, but there
is **no documented API to set the default endpoint**.

The Windows Sound control panel and Settings app use the COM interface
`IPolicyConfig` (class `CPolicyConfigClient`), which is not part of the
public SDK. It has been stable since Windows 7 and is used by widely
deployed open-source tools that switch audio devices (for example
SoundSwitch and EarTrumpet).

## Decision

Use `IPolicyConfig::SetDefaultEndpoint` from the Windows layer only
(`host/Platform/Windows/PolicyConfig.h`), declared locally:

- CLSID `CPolicyConfigClient` `{870AF99C-171D-4F9E-AF0D-E63DF40C2BC9}`
- IID `IPolicyConfig` `{F8679F50-850A-41CF-9C72-430F290290C8}`

Selecting a device sets it as default for the `eConsole` and
`eMultimedia` roles, which is what "Set as Default Device" in the Sound
control panel does. The `eCommunications` role is left unchanged.

The rest of the host only sees `IAudioSystem::setDefaultEndpoint`, so the
mechanism can be replaced without touching the device model, the
protocol controller or the firmware.

## Alternatives

- **No switching, only show the default.** Removes the product's main
  function.
- **Automate the Sound control panel (UI automation, `mmsys.cpl`).**
  Fragile, slow, visible to the user, and also undocumented behaviour.
- **Per-application routing (`IAudioPolicyConfigFactory`, Windows 10
  1803+).** Also undocumented, and changes the device per app, not the
  system default.

## Consequences

- A future Windows update could change or remove the interface. The
  failure is contained: `setDefaultEndpoint` returns false, the host
  answers `SELECT_DEVICE` with `ERROR OPERATION_FAILED`, and the firmware
  keeps showing the real default. Enumeration, volume and notifications
  use documented APIs and are not affected.
- The interface layout (method order) is taken from public open-source
  users, not from Microsoft documentation. It must be verified on the
  developer's Windows version (REQUIRES WINDOWS).
- The host must not be distributed through channels that forbid
  undocumented APIs (e.g. Microsoft Store) without revisiting this ADR.
