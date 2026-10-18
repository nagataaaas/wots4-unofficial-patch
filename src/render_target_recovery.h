#pragma once
// When a render target group cannot be set, the game's renderer (0x88f300) falls back by calling
// itself with the default group (the back buffer). If the back buffer cannot be set either, the
// fallback repeats until the stack overflows. This stops the second fallback into the group that
// is failing and remembers that the device must be reset before the next frame is drawn.
namespace unofficial_patch {

struct RenderTargetRecovery {
    const void* groupInProgress=nullptr;  // The group of the call that is running, nullptr outside
    bool resetNeeded=false;

    // Whether the call for group may run. False for a fallback into the group that is failing.
    bool mayBegin(const void* group) {
        if(!group||group!=groupInProgress)
            return true;
        resetNeeded=true;
        return false;
    }

    // Marks group as running and returns the group of the call around it, for finish().
    const void* start(const void* group) {
        auto outer=groupInProgress;
        groupInProgress=group;
        return outer;
    }

    void finish(const void* outer) { groupInProgress=outer; }
};

}  // namespace unofficial_patch
