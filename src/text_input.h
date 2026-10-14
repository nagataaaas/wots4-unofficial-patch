#pragma once
// Typing the sect name and weapon names (the shared text input polled by 0x4f4b10).
//
// Keyboard buttons: the input system maps keys to pad buttons while its byte +0x20 is
// nonzero (0x4f285a); by default E is Back and Space is OK. The player name input turns
// the mapping off while it runs (0x5f6e1d) and on again when it ends (0x5f78a7); the shared
// text input does not, so the E of romaji typed through the IME, which sends no character
// until the text is committed, closes it, and the IME's Space closes it as OK.
//
// Characters: the shared text input passes UTF-16 code units to the C runtime's isalnum
// (0x4f4be7). Under the Japanese locale the game sets, that classifies a code unit above
// 0xff by its bytes as if they were Shift_JIS, so Japanese characters are taken or dropped
// at random.
#include <cstdint>

namespace unofficial_patch {

// Turns the keyboard button mapping off while the shared text input is polled, and on
// again when the input ends or is no longer polled. Leaves the mapping alone when someone
// else (the player name input) turned it off.
struct KeyboardMappingPause {
    bool pausedByTextInput = false;
    bool polledSinceLastFrame = false;

    // Before each poll. keyboardMapping is the input system's byte +0x20.
    void beforePoll(uint8_t& keyboardMapping) {
        polledSinceLastFrame = true;
        if (!keyboardMapping)
            return;
        keyboardMapping = 0;
        pausedByTextInput = true;
    }

    // After each poll. finished is the poll's result: the text was accepted or cancelled.
    // Returns true when the mapping was turned on again.
    bool afterPoll(bool finished, uint8_t& keyboardMapping) {
        if (!finished)
            return false;
        return resume(keyboardMapping);
    }

    // Once per frame. A screen that stops polling without finishing (closed by something
    // else) must not leave the keyboard without buttons. Returns true when turned on again.
    bool onFrame(uint8_t& keyboardMapping) {
        bool polled = polledSinceLastFrame;
        polledSinceLastFrame = false;
        if (polled)
            return false;
        return resume(keyboardMapping);
    }

private:
    bool resume(uint8_t& keyboardMapping) {
        if (!pausedByTextInput)
            return false;
        pausedByTextInput = false;
        keyboardMapping = 1;
        return true;
    }
};

constexpr uint32_t privateUseFirst = 0xe000;  // The fullwidth digit fix keeps key images at 0xe010-0xe019
constexpr uint32_t privateUseLast = 0xf8ff;
constexpr uint32_t surrogateFirst = 0xd800;
constexpr uint32_t surrogateLast = 0xdfff;
constexpr uint32_t specialsFirst = 0xfff0;    // Specials and noncharacters up to 0xffff
constexpr uint32_t firstPrintableAfterControls = 0xa0;  // After the C1 controls 0x80-0x9f

inline bool isAsciiAlphanumeric(uint32_t codeUnit) {
    return (codeUnit >= '0' && codeUnit <= '9') || (codeUnit >= 'A' && codeUnit <= 'Z') ||
        (codeUnit >= 'a' && codeUnit <= 'z');
}

// Replaces isalnum at 0x4f4be7; the game also takes a space by itself. ASCII stays as
// before (letters and digits only). Other characters are taken unless they are controls,
// surrogates (cp932 never produces them), private use or specials.
inline bool acceptsTextInputCharacter(uint32_t codeUnit) {
    if (codeUnit < 0x80)
        return isAsciiAlphanumeric(codeUnit);
    if (codeUnit < firstPrintableAfterControls)
        return false;
    if (codeUnit >= surrogateFirst && codeUnit <= surrogateLast)
        return false;
    if (codeUnit >= privateUseFirst && codeUnit <= privateUseLast)
        return false;
    return codeUnit < specialsFirst;
}

}  // namespace unofficial_patch
