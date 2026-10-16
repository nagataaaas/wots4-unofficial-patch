#pragma once
// The resolution list of the display settings (0x536a90, filled once on first use).
//
// The game takes every mode EnumDisplaySettingsA lists, asks ChangeDisplaySettingsA with
// CDS_TEST (0x536af9) whether it can be set, and only then drops the modes it never lists:
// not 32 bits per pixel, narrower than 640 pixels or below 60 Hz (0x536b07-0x536b24).
// Windows lists every resolution again in 8 and 16 bits for old programs, and testing one of
// those takes about 0.24 s, so a monitor with many modes stops the game for minutes.
#include <cstdint>

namespace unofficial_patch {

constexpr uint32_t listedBitsPerPixel = 32;    // cmp [ebp-0x40],0x20 (0x536b07)
constexpr uint32_t minimumListedWidth = 640;   // cmp esi,0x280 (0x536b14)
constexpr uint32_t minimumListedFrequency = 60;  // cmp [ebp-0x30],0x3c (0x536b20), in Hz

// True for a mode the game would keep after a successful test, so only those need the test.
inline bool gameListsDisplayMode(uint32_t bitsPerPixel, uint32_t width, uint32_t frequency) {
    return bitsPerPixel == listedBitsPerPixel && width >= minimumListedWidth &&
           frequency >= minimumListedFrequency;
}

}  // namespace unofficial_patch
