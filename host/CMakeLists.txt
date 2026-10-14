cmake_minimum_required(VERSION 3.20)

# Windows host application.
#
# host_core: platform-independent device model and protocol controller,
#            with tests; builds on Windows and Linux.
# stm32-audio-host: Windows console program on Core Audio and USB HID
#                   (Windows only).
project(stm32_audio_host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared protocol codec (C), same sources as the firmware.
set(PROTOCOL_DIR ${CMAKE_CURRENT_LIST_DIR}/../protocol)
add_library(protocol STATIC ${PROTOCOL_DIR}/src/protocol.c)
target_include_directories(protocol PUBLIC ${PROTOCOL_DIR}/include)

add_library(host_core STATIC
    Audio/AudioMapping.cpp
    Audio/DeviceRegistry.cpp
    App/FrameText.cpp
    App/HostController.cpp
    App/Preferences.cpp)
target_include_directories(host_core PUBLIC Audio Hid App)
target_link_libraries(host_core PUBLIC protocol)

if(MSVC)
    target_compile_options(host_core PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(host_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

if(WIN32)
    add_executable(stm32-audio-host
        Platform/Windows/main.cpp
        Platform/Windows/WinAudioSystem.cpp
        Platform/Windows/WinHidLink.cpp
        Platform/Windows/WinStrings.cpp)
    target_include_directories(stm32-audio-host PRIVATE Platform/Windows)
    target_compile_definitions(stm32-audio-host PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(stm32-audio-host PRIVATE host_core ole32 setupapi hid)
    if(MSVC)
        target_compile_options(stm32-audio-host PRIVATE /W4 /WX /permissive- /utf-8)
    else()
        target_compile_options(stm32-audio-host PRIVATE -Wall -Wextra -Werror)
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Threads REQUIRED)
    add_executable(test_host Tests/test_host.cpp)
    target_link_libraries(test_host PRIVATE host_core Threads::Threads)
    if(MSVC)
        target_compile_options(test_host PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(test_host PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME host COMMAND test_host)
endif()
