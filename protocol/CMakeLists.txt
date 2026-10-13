cmake_minimum_required(VERSION 3.20)

project(stm32_audio_protocol LANGUAGES C)

# Shared protocol codec: built here for the host-side tests; the firmware
# and the Windows host add protocol/src/protocol.c to their own builds.
# The code is C99. MSVC has no C99 mode, so it builds as C11 there.
if(MSVC)
    set(PROTOCOL_C_STANDARD 11)
else()
    set(PROTOCOL_C_STANDARD 99)
endif()

add_library(protocol STATIC src/protocol.c)
target_include_directories(protocol PUBLIC include)
set_target_properties(protocol PROPERTIES
    C_STANDARD ${PROTOCOL_C_STANDARD} C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)

if(MSVC)
    target_compile_options(protocol PRIVATE /W4 /WX)
else()
    target_compile_options(protocol PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Werror)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(test_protocol tests/test_protocol.c)
    target_link_libraries(test_protocol PRIVATE protocol)
    set_target_properties(test_protocol PROPERTIES
        C_STANDARD ${PROTOCOL_C_STANDARD} C_STANDARD_REQUIRED ON C_EXTENSIONS OFF)
    add_test(NAME protocol COMMAND test_protocol)
endif()
