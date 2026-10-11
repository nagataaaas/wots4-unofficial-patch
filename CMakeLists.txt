cmake_minimum_required(VERSION 3.25)
project(wots4_mod_bug_fix LANGUAGES C CXX)

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

# Not released until it is confirmed in the actual game
wots4_add_mod(bug_fix SOURCES src/mod.cpp DEF src/mod.def
    LIBS wots4_minhook
    LICENSES docs/licenses/minhook.txt
    NO_RELEASE)
target_include_directories(mod_bug_fix PRIVATE src)

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
        ARGS ${unpacked_native} $<TARGET_FILE:mod_bug_fix> NEEDS_EXE)
    add_dependencies(follow_detach_native_probe mod_bug_fix)
    # Loads mod.dll into a copy of the exe and runs the hooked SetNextScript (the game is not started)
    wots4_add_probe(property_return_native_probe SOURCES tests/property_return_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_bug_fix> NEEDS_EXE)
    add_dependencies(property_return_native_probe mod_bug_fix)
    # Loads copies of mod.dll into a copy of the exe with a fake Direct3D device where the renderer
    # keeps it, and sets render states as the game does (no GPU, the game is not started)
    wots4_add_probe(translucent_depth_native_probe SOURCES tests/translucent_depth_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_bug_fix> NEEDS_EXE)
    add_dependencies(translucent_depth_native_probe mod_bug_fix)
endif()
