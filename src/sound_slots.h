#pragma once
// SoundMain.cpp plays every sound in one of 24 slots (0x74 bytes each from
// 0x9f02c0). A slot is freed only by the end notification of its channel
// (0x5a1950, called by 0x6a2480 at the top of the slot update 0x5a0640), or for
// a stream when +0x10 is 2. 0x6a2480 sends the notification only on a change:
// the channel played in the previous update, does not play now, and is not
// paused. An immediate stop (0x5a1210 with no fade) only stops the channel, so a
// slot stopped while paused (menus pause looping sound effects, 0x5a39a0) or
// before the first update saw it playing is never freed. Such slots keep
// counting toward the limit of 5 streams (0x5a05c0) and are played again when a
// menu closes.
#include <cstdint>

namespace unofficial_patch {

// Slot (0x5a03a0 resets one).
constexpr uint32_t slotChannelOffset = 0x14;  // Channel (0x6a2420), or 0 while a stream opens
constexpr uint32_t slotSoundOffset = 0x18;    // AudioLib Sound of this slot
constexpr uint32_t slotInUseOffset = 0x34;
// Channel (0x20 bytes, 0x6a2420).
constexpr uint32_t channelPausedOffset = 0x0c;   // Menu pause (0x5a39a0) or a paused start (0x6a2510)
constexpr uint32_t channelPlayingOffset = 0x19;  // Playing at the last update; 0x6a2480 moves it to +0x18

// Called after the game's stop (0x5a1210) with the slot it stopped and the fade it was given.
// Makes the next update see a channel that played and stopped, so the game's own end
// notification frees the slot (a stream through +0x10 = 2, a sample at once). Returns whether it
// did. A fade out of a playing channel is left to the update, which stops and frees it when the
// fade ends. A paused channel is silent and cannot fade, so it is freed like an immediate stop.
// Runs on every stop of a sound, so it only reads and writes a few bytes.
inline bool prepareStoppedSlotRelease(uintptr_t slot, float fadeSeconds) {
    if(!slot||!*reinterpret_cast<const uint8_t*>(slot+slotInUseOffset))
        return false;
    auto channel=*reinterpret_cast<const uintptr_t*>(slot+slotChannelOffset);
    // Without a channel or Sound, 0x5a1210 has already marked the slot for release.
    if(!channel||!*reinterpret_cast<const uintptr_t*>(slot+slotSoundOffset))
        return false;
    auto paused=reinterpret_cast<uint8_t*>(channel+channelPausedOffset);
    // Same test as 0x5a1243: anything but 0 or less fades, NaN included.
    bool fades=!(fadeSeconds<=0.0f);
    if(fades&&!*paused)
        return false;
    *paused=0;
    *reinterpret_cast<uint8_t*>(channel+channelPlayingOffset)=1;
    return true;
}

}  // namespace unofficial_patch
