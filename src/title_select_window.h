#pragma once
// The title (GamePartTitle) makes a select window (UI type 0x1b) when it opens and keeps it at
// +0x4c for the New Game confirmation and the difficulty selection. Opening Options on the title
// makes a new select window in the UI manager (0x6763df), which deletes the old one first, and
// the title keeps pointing at the deleted window.
#include <cstdint>

namespace unofficial_patch {

constexpr uint32_t titleSelectWindowOffset = 0x4c;
constexpr uint32_t titleUiManagerOffset = 0x58;
constexpr uint32_t uiManagerSelectWindowOffset = 4+0x1b*4;  // [manager+4+type*4] (0x67fcdc)

// Points the title at the select window the UI manager holds now. Returns true if it changed.
inline bool useCurrentSelectWindow(uint8_t* title) {
    if(!title)
        return false;
    auto uiManager=*reinterpret_cast<uint8_t**>(title+titleUiManagerOffset);
    if(!uiManager)
        return false;
    auto current=*reinterpret_cast<void**>(uiManager+uiManagerSelectWindowOffset);
    auto& kept=*reinterpret_cast<void**>(title+titleSelectWindowOffset);
    if(!current||current==kept)
        return false;
    kept=current;
    return true;
}

}  // namespace unofficial_patch
