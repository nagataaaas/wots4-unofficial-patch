cmake_minimum_required(VERSION 3.25)
project(wots4_mod_crash_fix LANGUAGES C CXX)

# 単体でビルドするときは -DWOTS4_SDK_DIR=<wots4-mods のパス> を渡す
if(NOT COMMAND wots4_add_mod)
    set(WOTS4_SDK_DIR "" CACHE PATH "wots4-mods（ModLoader）のパス")
    if(NOT EXISTS "${WOTS4_SDK_DIR}/cmake/Wots4.cmake")
        message(FATAL_ERROR "WOTS4_SDK_DIR に wots4-mods のパスを指定してください")
    endif()
    include(${WOTS4_SDK_DIR}/cmake/Wots4.cmake)
    enable_testing()
    set(WOTS4_BUILD_TESTS ON)
endif()

# まだ修正は入っていない。配布はしない。
wots4_add_mod(crash_fix SOURCES src/mod.cpp DEF src/mod.def NO_RELEASE)

if(WOTS4_BUILD_TESTS)
    # 牢屋の写真家から再開したあとの結末のクラッシュを、exe の写しの上で再現し、
    # ガードで防げることを確かめる（ゲームは起動しない）
    set(WOTS4_UNPACKED_EXE "${WOTS4_ROOT}/research/bin/WayOfTheSamurai4.unpacked.exe" CACHE FILEPATH "復号済みの exe")
    file(TO_NATIVE_PATH "${WOTS4_UNPACKED_EXE}" unpacked_native)
    wots4_add_probe(jail_ending_native_probe SOURCES tests/jail_ending_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE ARGS ${unpacked_native} NEEDS_EXE)
endif()
