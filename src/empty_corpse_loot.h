#pragma once
#include <cstdint>

namespace unofficial_patch {

// The corpse search reads +0x325, while the part draw consumes +0x326 separately.
constexpr uint32_t corpseLootedOffset = 0x325;
constexpr uint32_t corpsePartsDroppedOffset = 0x326;
constexpr uint32_t characterDefinitionOffset = 0x9c;
constexpr uint32_t characterStateOffset = 0x114;
constexpr uint32_t deadOrDyingState = 0x600000;

struct CorpseLootCandidate {
    int16_t itemRow;
    int16_t weight;
    int16_t quantity;
};

// Output of the game's candidate builder (0x4f57f0), before either draw consumes an item.
struct CorpseLootCandidates {
    uint16_t unused;
    CorpseLootCandidate entries[10];
};
static_assert(sizeof(CorpseLootCandidates)==62,"The game writes ten six-byte candidates after a two-byte header");

inline bool hasCorpseLootCandidate(const CorpseLootCandidates& candidates) {
    for(const auto& candidate:candidates.entries)
        if(candidate.weight>0)
            return true;
    return false;
}

}  // namespace unofficial_patch
