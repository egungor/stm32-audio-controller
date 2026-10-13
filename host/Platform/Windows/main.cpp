// stm32-audio-host: Windows console program.
//
// Until USB HID exists (M3) this is a test console for the Windows audio
// layer and the protocol controller:
//
//   stm32-audio-host list              playback devices, default, volume
//   stm32-audio-host set-default <n>   make device <n> (from "list") the default
//   stm32-audio-host volume <0-100>    volume of the default device
//   stm32-audio-host mute on|off       mute of the default device
//   stm32-audio-host watch             run HostController; print the frames it
//                                      would send to the STM32 (Ctrl+C to stop)

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "AudioMapping.h"
#include "DeviceRegistry.h"
#include "EventQueue.h"
#include "FrameText.h"
#include "HostController.h"
#include "WinAudioSystem.h"

using namespace stm32_audio::host;

namespace {

EventQueue* g_events = nullptr;

BOOL WINAPI onConsoleCtrl(DWORD type)
{
    if ((type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) && g_events != nullptr) {
        g_events->push(HostEvent::Quit);
        return TRUE;
    }
    return FALSE;
}

const char* formFactorName(FormFactor f)
{
    switch (f) {
    case FormFactor::Speakers:
        return "speakers";
    case FormFactor::Headphones:
        return "headphones";
    case FormFactor::Headset:
        return "headset";
    case FormFactor::Display:
        return "display";
    case FormFactor::DigitalOutput:
        return "digital";
    case FormFactor::Unknown:
        break;
    }
    return "generic";
}

// Prints every frame instead of sending it over HID.
class PrintSink : public IFrameSink {
public:
    bool sendFrame(const Frame& frame) override
    {
        ProtoMessage msg{};
        if (Proto_Decode(frame.data(), &msg) != PROTO_OK) {
            std::printf("-> (invalid frame)\n");
            return false;
        }
        std::printf("-> %s\n", describeMessage(msg).c_str());
        std::fflush(stdout);
        return true;
    }
};

int cmdList(win::WinAudioSystem& audio)
{
    const std::vector<AudioEndpoint> endpoints = audio.renderEndpoints();
    const std::string def = audio.defaultEndpointId();

    if (endpoints.empty()) {
        std::printf("No active playback devices.\n");
    }
    for (std::size_t i = 0; i < endpoints.size(); ++i) {
        const AudioEndpoint& e = endpoints[i];
        std::printf("%2lu %s %-10s %s\n", static_cast<unsigned long>(i + 1), e.id == def ? "*" : " ",
                    formFactorName(e.formFactor), e.name.c_str());
        std::printf("       %s\n", e.id.c_str());
    }
    const std::optional<unsigned> volume = audio.volumePercent();
    const std::optional<bool> muted = audio.muted();
    std::printf("Default volume: %s%%, muted: %s\n", volume ? std::to_string(*volume).c_str() : "?",
                muted ? (*muted ? "yes" : "no") : "?");
    return 0;
}

int cmdSetDefault(win::WinAudioSystem& audio, const char* arg)
{
    const std::vector<AudioEndpoint> endpoints = audio.renderEndpoints();
    const long n = std::strtol(arg, nullptr, 10);

    if (n < 1 || static_cast<std::size_t>(n) > endpoints.size()) {
        std::printf("Device number must be 1..%lu (see \"list\").\n",
                    static_cast<unsigned long>(endpoints.size()));
        return 2;
    }
    const AudioEndpoint& e = endpoints[static_cast<std::size_t>(n - 1)];
    if (!audio.setDefaultEndpoint(e.id)) {
        std::printf("FAIL: could not make \"%s\" the default device.\n", e.name.c_str());
        return 1;
    }
    std::printf("OK: \"%s\" is now the default device.\n", e.name.c_str());
    return 0;
}

int cmdVolume(win::WinAudioSystem& audio, const char* arg)
{
    const long percent = std::strtol(arg, nullptr, 10);
    if (percent < 0 || percent > 100 || !audio.setVolumePercent(static_cast<unsigned>(percent))) {
        std::printf("FAIL: volume must be 0..100 and a default device must exist.\n");
        return 1;
    }
    std::printf("OK: volume %ld%%\n", percent);
    return 0;
}

int cmdMute(win::WinAudioSystem& audio, const std::string& arg)
{
    if ((arg != "on" && arg != "off") || !audio.setMuted(arg == "on")) {
        std::printf("FAIL: use \"mute on\" or \"mute off\"; a default device must exist.\n");
        return 1;
    }
    std::printf("OK: mute %s\n", arg.c_str());
    return 0;
}

int cmdWatch(win::WinAudioSystem& audio, EventQueue& events)
{
    PrintSink sink;
    HostController controller(audio, sink);

    g_events = &events;
    SetConsoleCtrlHandler(onConsoleCtrl, TRUE);

    // Act as if the STM32 had just connected and asked for the list.
    controller.onConnected();
    ProtoMessage request{};
    Frame frame{};
    request.command = PROTO_CMD_GET_DEVICE_LIST;
    (void)Proto_Encode(&request, frame.data());
    std::printf("<- %s (simulated)\n", describeMessage(request).c_str());
    controller.onFrame(frame);
    std::printf("Watching Windows audio changes. Change the default device or the volume; Ctrl+C stops.\n");
    std::fflush(stdout);

    for (;;) {
        const std::optional<HostEvent> event = events.pop(std::chrono::milliseconds(500));
        if (!event) {
            continue;
        }
        switch (*event) {
        case HostEvent::DevicesChanged:
            controller.onDevicesChanged();
            break;
        case HostEvent::DefaultChanged:
            audio.rebindVolumeNotifications();
            controller.onDefaultChanged();
            break;
        case HostEvent::VolumeChanged:
            controller.onVolumeChanged();
            break;
        case HostEvent::Quit:
            SetConsoleCtrlHandler(onConsoleCtrl, FALSE);
            g_events = nullptr;
            std::printf("Stopped.\n");
            return 0;
        }
    }
}

int usage()
{
    std::printf(
        "usage: stm32-audio-host list | set-default <n> | volume <0-100> | mute on|off | watch\n");
    return 2;
}

int run(int argc, char** argv)
{
    if (argc < 2) {
        return usage();
    }
    const std::string cmd = argv[1];

    EventQueue events;
    win::WinAudioSystem audio(events);
    if (!audio.ok()) {
        std::printf("FAIL: cannot open the Windows audio device enumerator.\n");
        return 1;
    }

    if (cmd == "list") {
        return cmdList(audio);
    }
    if (cmd == "set-default" && argc == 3) {
        return cmdSetDefault(audio, argv[2]);
    }
    if (cmd == "volume" && argc == 3) {
        return cmdVolume(audio, argv[2]);
    }
    if (cmd == "mute" && argc == 3) {
        return cmdMute(audio, argv[2]);
    }
    if (cmd == "watch") {
        return cmdWatch(audio, events);
    }
    return usage();
}

}  // namespace

int main(int argc, char** argv)
{
    SetConsoleOutputCP(CP_UTF8);  // device names are printed as UTF-8

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        std::printf("FAIL: COM initialisation failed.\n");
        return 1;
    }
    const int result = run(argc, argv);  // COM objects are released inside run()
    CoUninitialize();
    return result;
}
