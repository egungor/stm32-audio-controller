; Windows installer of the host programs (Inno Setup 6), issue #74.
;
; Per-user install, no administrator rights:
;   %LOCALAPPDATA%\Programs\STM32 Audio Controller\
;     stm32-audio-tray.exe   tray program (serves the controller)
;     stm32-audio-host.exe   console tools (list, alias, run, startup, ...)
; Start menu: the tray program and the uninstaller.
; Task "Start with Windows" (checked): the same Run value the tray menu and
; "stm32-audio-host startup on" write, so the tray menu shows it checked.
; After setup the tray program starts. Uninstall closes it and removes the
; Run value; preferences in %APPDATA%\stm32-audio-controller are kept.
;
; Build (installer/README.md):
;   iscc /DAppVersion=0.7.0 /DFileVersion=0.7.0.0 /DBinDir=<folder with the exes> stm32-audio-controller.iss

#ifndef AppVersion
  #define AppVersion "0.7.0-dev"
#endif
#ifndef FileVersion
  #define FileVersion "0.7.0.0"
#endif
#ifndef BinDir
  #define BinDir "..\build\host\Release"
#endif

#define AppName "STM32 Audio Controller"
#define TrayExe "stm32-audio-tray.exe"
#define HostExe "stm32-audio-host.exe"

[Setup]
AppId={{99969F48-403E-4CA5-AD99-4E0DE31E915B}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=egungor
AppPublisherURL=https://github.com/egungor/stm32-audio-controller
VersionInfoVersion={#FileVersion}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=stm32-audio-controller-setup-{#AppVersion}
SetupIconFile=..\host\Platform\Windows\stm32-audio-tray.ico
UninstallDisplayIcon={app}\{#TrayExe}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
; A running tray program is closed through the Restart Manager (it stops
; its service and saves the preferences on WM_ENDSESSION) and not
; restarted by Setup: the [Run] entry starts it again.
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
turkish.StartWithWindows=Windows açılınca başlat
english.StartWithWindows=Start with Windows
turkish.StartNow=STM32 Ses Kumandası'nı şimdi başlat
english.StartNow=Start STM32 Audio Controller now
turkish.TrayName=STM32 Ses Kumandası
english.TrayName=STM32 Audio Controller

[Tasks]
Name: "autostart"; Description: "{cm:StartWithWindows}"

[Files]
Source: "{#BinDir}\{#TrayExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#BinDir}\{#HostExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\host\README.md"; DestDir: "{app}"; DestName: "README.md"; Flags: ignoreversion

[Icons]
Name: "{group}\{cm:TrayName}"; Filename: "{app}\{#TrayExe}"
Name: "{group}\{cm:UninstallProgram,{cm:TrayName}}"; Filename: "{uninstallexe}"

[Registry]
; Same value name and quoting as WinStartup.cpp / Startup.cpp.
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "STM32 Audio Controller"; ValueData: """{app}\{#TrayExe}"""; Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{app}\{#TrayExe}"; Description: "{cm:StartNow}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Code]
// Uninstall: close the tray program before its files go. taskkill without
// /F sends WM_CLOSE to its window, so it stops the service and saves the
// preferences first; the wait lets it exit. Nothing happens if it is not
// running.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ResultCode: Integer;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/IM {#TrayExe}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    Sleep(2000);
    // Still running (no answer to WM_CLOSE): end it; preferences are saved
    // within a second of every change anyway.
    Exec(ExpandConstant('{sys}\taskkill.exe'), '/F /IM {#TrayExe}', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
  end;
end;

// Without the task, remove a Run value an earlier install or the tray menu
// may have written, so "Start with Windows" off really means off.
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssPostInstall) and not WizardIsTaskSelected('autostart') then
    RegDeleteValue(HKEY_CURRENT_USER, 'Software\Microsoft\Windows\CurrentVersion\Run', 'STM32 Audio Controller');
end;
