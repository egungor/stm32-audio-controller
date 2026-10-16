# Windows installer

`stm32-audio-controller.iss` (Inno Setup 6) installs the host programs for
the current user, without administrator rights (issue #74):

| | |
|---|---|
| Folder | `%LOCALAPPDATA%\Programs\STM32 Audio Controller` |
| Programs | `stm32-audio-tray.exe` (tray program), `stm32-audio-host.exe` (console tools) |
| Start menu | "STM32 Ses Kumandası" and its uninstaller |
| "Windows açılınca başlat" | checked by default: `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, value `STM32 Audio Controller`, the same value the tray menu and `stm32-audio-host startup on` use |
| After setup | the tray program starts |
| Update | run a newer setup: a running tray program is closed (Restart Manager) and started again |
| Uninstall | closes the tray program, removes the programs and the Run value; the preferences in `%APPDATA%\stm32-audio-controller` stay |

Setup speaks Turkish or English (Windows language, or the choice on the
first page).

## Where to get it

The **Host installer** workflow builds it on every change to `host/`,
`protocol/` or `installer/`: Actions → Host installer → the run →
artifact `stm32-audio-controller-setup`. On a `v*` tag it is attached to
the GitHub release (created if missing; a tag with a "-", such as
`v0.7.0-beta.1`, makes a pre-release):

```text
git tag v0.7.0-beta.1 <commit>
git push origin v0.7.0-beta.1
```

The setup is not code-signed, so Windows SmartScreen may show "Windows
protected your PC": "More info" → "Run anyway".

## Build by hand (Windows)

```text
cmake -S host -B build/host -DBUILD_TESTING=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
cmake --build build/host --config Release
"%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" /DAppVersion=0.7.0 /DFileVersion=0.7.0.0 ^
    /DBinDir=%CD%\build\host\Release installer\stm32-audio-controller.iss
```

The setup lands in `installer\Output\`.
