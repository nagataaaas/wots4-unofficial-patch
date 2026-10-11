#pragma once
// Effects that follow a character joint hold a raw pointer to it. The game's own
// detach (0x4a8ec0) only covers effect types 0x62 and 0x84, so effects attached
// by 0x5658c0 (scripts' SetEffect, time table type 41) keep pointing at the
// joints after the model is freed. This clears them the same way 0x4a8ec0 does.
#include <cstdint>

namespace bug_fix {

// Effect manager ([0x9e966c]): 350 effect pointers from +0x1c (0x4a8ec0).
constexpr uint32_t effectSlotsOffset = 0x1c;
constexpr uint32_t effectSlotCount = 350;
// Effect: +8 is the followed node (read at 0x4a4cf1).
constexpr uint32_t effectFollowOffset = 8;
// Character model: joints from +0x48, JOINT_ID_END = 0x17 (0x5658c0).
constexpr uint32_t modelJointsOffset = 0x48;
constexpr uint32_t modelJointCount = 0x17;

// Clears +8 of every effect that follows one of model's joints. Returns how many.
inline uint32_t detachJointFollowers(uintptr_t manager, uintptr_t model) {
    if(!manager||!model)
        return 0;
    auto joints=reinterpret_cast<const uintptr_t*>(model+modelJointsOffset);
    auto slots=reinterpret_cast<const uintptr_t*>(manager+effectSlotsOffset);
    uint32_t detached=0;
    for(uint32_t i=0;i<effectSlotCount;++i) {
        if(!slots[i])
            continue;
        auto follow=reinterpret_cast<uintptr_t*>(slots[i]+effectFollowOffset);
        if(!*follow)
            continue;
        for(uint32_t j=0;j<modelJointCount;++j) {
            if(joints[j]==*follow) {
                *follow=0;
                ++detached;
                break;
            }
        }
    }
    return detached;
}

}  // namespace bug_fix
