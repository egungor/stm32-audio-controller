cmake_minimum_required(VERSION 3.20)

# Windows host application. This build contains the platform-independent
# core (device model, protocol controller) and its tests; it builds on
# Windows (MSVC) and Linux. The Windows Core Audio and HID layers are added
# in later tasks (M3/M4).
project(stm32_audio_host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Shared protocol codec (C), same sources as the firmware.
set(PROTOCOL_DIR ${CMAKE_CURRENT_LIST_DIR}/../protocol)
add_library(protocol STATIC ${PROTOCOL_DIR}/src/protocol.c)
target_include_directories(protocol PUBLIC ${PROTOCOL_DIR}/include)

add_library(host_core STATIC
    Audio/DeviceRegistry.cpp
    App/HostController.cpp)
target_include_directories(host_core PUBLIC Audio Hid App)
target_link_libraries(host_core PUBLIC protocol)

if(MSVC)
    target_compile_options(host_core PRIVATE /W4 /WX /permissive-)
else()
    target_compile_options(host_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_host Tests/test_host.cpp)
    target_link_libraries(test_host PRIVATE host_core)
    if(MSVC)
        target_compile_options(test_host PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(test_host PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME host COMMAND test_host)
endif()
