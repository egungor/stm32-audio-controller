cmake_minimum_required(VERSION 3.20)

# Headless Linux build of firmware/ui with LVGL (see README.md).
project(ui_simulator LANGUAGES C)

set(FW_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
set(LVGL_DIR ${FW_DIR}/STM32AudioController/Middlewares/Third_Party/lvgl)
set(UI_DIR ${FW_DIR}/ui)

if(NOT EXISTS ${LVGL_DIR}/lvgl.h)
    message(FATAL_ERROR "LVGL not found in ${LVGL_DIR}. Run: git submodule update --init")
endif()

# LVGL sources, configured by firmware/ui/lv_conf.h (the same file the
# firmware will use). Options turned off in lv_conf.h compile to nothing.
file(GLOB_RECURSE LVGL_SOURCES CONFIGURE_DEPENDS ${LVGL_DIR}/src/*.c)
add_library(lvgl STATIC ${LVGL_SOURCES})
target_include_directories(lvgl PUBLIC ${LVGL_DIR} ${UI_DIR})
target_compile_definitions(lvgl PUBLIC LV_CONF_INCLUDE_SIMPLE)
set_target_properties(lvgl PROPERTIES C_STANDARD 99)

add_library(ui STATIC
    ${UI_DIR}/ui.c
    ${UI_DIR}/fonts/ui_font_16.c
    ${UI_DIR}/fonts/ui_font_22.c
    ${UI_DIR}/fonts/ui_icons_40.c)
target_include_directories(ui PUBLIC ${UI_DIR})
target_link_libraries(ui PUBLIC lvgl)
set_target_properties(ui PROPERTIES C_STANDARD 99)
set_source_files_properties(${UI_DIR}/ui.c PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Werror")

add_executable(ui_simulator main.c)
target_link_libraries(ui_simulator PRIVATE ui)
target_compile_options(ui_simulator PRIVATE -Wall -Wextra -Werror)
set_target_properties(ui_simulator PROPERTIES C_STANDARD 99)
