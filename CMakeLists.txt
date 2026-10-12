cmake_minimum_required(VERSION 3.25)
project(wots4_mod_unofficial_patch LANGUAGES C CXX)

# To build this Mod alone, pass -DWOTS4_SDK_DIR=<path to wots4-mods>
if(NOT COMMAND wots4_add_mod)
    set(WOTS4_SDK_DIR "" CACHE PATH "Path to wots4-mods (the ModLoader)")
    if(NOT EXISTS "${WOTS4_SDK_DIR}/cmake/Wots4.cmake")
        message(FATAL_ERROR "Set WOTS4_SDK_DIR to the path of wots4-mods")
    endif()
    include(${WOTS4_SDK_DIR}/cmake/Wots4.cmake)
    enable_testing()
    set(WOTS4_BUILD_TESTS ON)
endif()

wots4_use_minhook()

wots4_add_mod(unofficial_patch SOURCES src/mod.cpp DEF src/mod.def
    LIBS wots4_minhook
    FILES config.ini
    LICENSES docs/licenses/minhook.txt)
target_include_directories(mod_unofficial_patch PRIVATE src)

if(WOTS4_BUILD_TESTS)
    # Checks, with synthetic data, the detach of effects that follow a character model's joints
    wots4_add_probe(follow_detach_probe SOURCES tests/follow_detach_probe.cpp
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Checks, with synthetic names and flags, when the confiscated property is given back
    wots4_add_probe(property_return_probe SOURCES tests/property_return_probe.cpp
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Checks, with synthetic render states, the write mask of the depth render target
    wots4_add_probe(translucent_depth_probe SOURCES tests/translucent_depth_probe.cpp
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Examines the failure conditions of the scene models and the achievement code on a copy of the exe
    # (the game is not started). Not yet matched against the ending crash reported in the actual game.
    set(WOTS4_UNPACKED_EXE "${WOTS4_ROOT}/research/bin/WayOfTheSamurai4.unpacked.exe" CACHE FILEPATH "Decrypted exe")
    file(TO_NATIVE_PATH "${WOTS4_UNPACKED_EXE}" unpacked_native)
    wots4_add_probe(jail_ending_native_probe SOURCES tests/jail_ending_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE ARGS ${unpacked_native} NEEDS_EXE)
    # Loads mod.dll into a copy of the exe and runs the game's effect update and character effect release
    # (the game is not started)
    wots4_add_probe(follow_detach_native_probe SOURCES tests/follow_detach_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(follow_detach_native_probe mod_unofficial_patch)
    # Loads mod.dll into a copy of the exe and runs the hooked SetNextScript (the game is not started)
    wots4_add_probe(property_return_native_probe SOURCES tests/property_return_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(property_return_native_probe mod_unofficial_patch)
    # Loads copies of mod.dll into a copy of the exe with a fake Direct3D device where the renderer
    # keeps it, and sets render states as the game does (no GPU, the game is not started)
    wots4_add_probe(translucent_depth_native_probe SOURCES tests/translucent_depth_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(translucent_depth_native_probe mod_unofficial_patch)
    # Loads mod.dll into a copy of the exe with config.ini files that turn fixes off (the game is not started)
    wots4_add_probe(config_native_probe SOURCES tests/config_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(config_native_probe mod_unofficial_patch)
    # Loads mod.dll into a copy of the exe and runs the game's display mode check with fake
    # GetSystemMetrics and ChangeDisplaySettingsA (the display mode is not changed)
    wots4_add_probe(display_mode_check_native_probe SOURCES tests/display_mode_check_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(display_mode_check_native_probe mod_unofficial_patch)
    # Checks, with synthetic window states, when full screen must be applied again on activation
    wots4_add_probe(full_screen_reapply_probe SOURCES tests/full_screen_reapply_probe.cpp
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Loads mod.dll into a copy of the exe and runs the game's full screen setup with fake window,
    # display and renderer calls (no real window, display mode or device is changed)
    wots4_add_probe(full_screen_reapply_native_probe SOURCES tests/full_screen_reapply_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(full_screen_reapply_native_probe mod_unofficial_patch)
    # Loads mod.dll into a copy of the exe and sends messages to the game's window procedure
    # with a recording DefWindowProcA (no window is painted)
    wots4_add_probe(window_background_native_probe SOURCES tests/window_background_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(window_background_native_probe mod_unofficial_patch)
    # Checks, with synthetic night visit progress objects, how the left-over result is discarded
    wots4_add_probe(yobai_result_probe SOURCES tests/yobai_result_probe.cpp
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Loads mod.dll into a copy of the exe and loads save data (from a file and from the retry snapshot)
    # through the game's own last lines (the game is not started)
    wots4_add_probe(yobai_result_native_probe SOURCES tests/yobai_result_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(yobai_result_native_probe mod_unofficial_patch)
    # Checks, with synthetic camera axes, where sounds land around the 3D sound listener
    wots4_add_probe(listener_orientation_probe SOURCES tests/listener_orientation_probe.cpp
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # Loads mod.dll into a copy of the exe and runs the game's 3D listener update with a fake
    # DirectSound (no audio device is opened, the game is not started)
    wots4_add_probe(listener_orientation_native_probe SOURCES tests/listener_orientation_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_unofficial_patch> NEEDS_EXE)
    add_dependencies(listener_orientation_native_probe mod_unofficial_patch)
endif()
