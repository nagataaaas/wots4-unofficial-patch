cmake_minimum_required(VERSION 3.25)
project(wots4_mod_bug_fix LANGUAGES C CXX)

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

wots4_use_minhook()

# 実ゲームでの確認が済むまで配布しない
wots4_add_mod(bug_fix SOURCES src/mod.cpp DEF src/mod.def
    LIBS wots4_minhook
    LICENSES docs/licenses/minhook.txt
    NO_RELEASE)
target_include_directories(mod_bug_fix PRIVATE src)

if(WOTS4_BUILD_TESTS)
    # 人物のモデルの骨に付いたエフェクトを外す処理を、合成したデータで確かめる
    wots4_add_probe(follow_detach_probe SOURCES tests/follow_detach_probe.cpp
        INCLUDES ${CMAKE_CURRENT_SOURCE_DIR}/src)
    # exe の写しで演出モデル・実績処理の故障条件を調べる（ゲームは起動しない）。
    # 実ゲームで報告された結末クラッシュとの照合は未実施。
    set(WOTS4_UNPACKED_EXE "${WOTS4_ROOT}/research/bin/WayOfTheSamurai4.unpacked.exe" CACHE FILEPATH "復号済みの exe")
    file(TO_NATIVE_PATH "${WOTS4_UNPACKED_EXE}" unpacked_native)
    wots4_add_probe(jail_ending_native_probe SOURCES tests/jail_ending_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE ARGS ${unpacked_native} NEEDS_EXE)
    # exe の写しに mod.dll を入れ、ゲームのエフェクト更新と人物のエフェクト解放を実行する（ゲームは起動しない）
    wots4_add_probe(follow_detach_native_probe SOURCES tests/follow_detach_native_probe.cpp
        LINK_OPTIONS /BASE:0x10000000 /DYNAMICBASE
        ARGS ${unpacked_native} $<TARGET_FILE:mod_bug_fix> NEEDS_EXE)
    add_dependencies(follow_detach_native_probe mod_bug_fix)
endif()
