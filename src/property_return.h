#pragma once
// "The Fighting Spirit Never Dies" takes the player's weapons, items and outfit
// (PlayerTakeProperty, 0x553d80) and gives them back only in BR_14_00_S140,
// after the fight with the three sisters. When Madara rescues the player
// (save 0x3760 = 1), BR_14_00_S050 goes to S210 instead and nothing gives them
// back. The weapons wait in an area that is not saved, so they are lost once
// the game is saved and loaded.
// Normal play never sets 0x3760 to 1 (only a save editor or similar does), so
// this makes an unfinished branch consistent rather than fixing a reachable bug.
#include <cstdint>
#include <cstring>

namespace unofficial_patch {

// Script state ([0x9eb5f8]): SetNextScript (0x55e140) copies the name to +0x2c, 0x80 bytes.
constexpr uint32_t nextScriptOffset = 0x2c;
constexpr uint32_t nextScriptSize = 0x80;
// Game state ([0x9e967c]): +0x48b9e is 1 after PlayerTakeProperty (0x553f50) and
// 0 after PlayerGiveProperty (0x55408e).
constexpr uint32_t propertyTakenOffset = 0x48b9e;
// BR_14_00_S050 goes here only when akemi_proof (0x3760) is 1.
constexpr char rescueScript[] = "scenario/p420/BR_14_00_S210";

// True when the next script is the scene after Madara's rescue and the property is still taken.
inline bool shouldReturnProperty(const char* nextScript, uint8_t propertyTaken) {
    if(!nextScript||!propertyTaken)
        return false;
    // The name may fill all 0x80 bytes without a terminator; the bound stops there.
    return !strncmp(nextScript,rescueScript,nextScriptSize);
}

}  // namespace unofficial_patch
