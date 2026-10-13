#pragma once
#include <cstdint>

namespace unofficial_patch {

constexpr uint16_t fullwidthDigitFirst = 0xff10;
constexpr uint16_t privateDigitFirst = 0xe010;
constexpr unsigned digitCount = 10;
constexpr uint8_t fontGlyphHeight = 32;
constexpr uint8_t fontTexturePages = 2;
constexpr unsigned numericKeyCellSize = 32;
constexpr unsigned firstNumericKeyX = 192;
constexpr unsigned zeroNumericKeyX = 480;
constexpr unsigned numericKeyY = 32;

// FontDatabase records: the last page-one entry otherwise hides the normal digit.
struct FontGlyph {
    uint16_t codepoint, x, y;
    uint8_t width, page;
};
struct FontDatabase {
    uint16_t count;
    uint8_t height, pages;
    uint32_t reserved;
};
static_assert(sizeof(FontGlyph)==8&&sizeof(FontDatabase)==8,"Unexpected font record layout");

struct DigitGlyphs {
    FontGlyph* normal[digitCount]{};
    FontGlyph* keys[digitCount]{};
};

// Validate every pair before writing any code, so an unknown font keeps its original records.
inline bool findDigitGlyphs(FontDatabase* database, DigitGlyphs& found) {
    if(!database||database->height!=fontGlyphHeight||database->pages!=fontTexturePages)
        return false;
    found={};
    auto glyphs=reinterpret_cast<FontGlyph*>(database+1);
    for(unsigned index=0;index<database->count;++index) {
        auto& glyph=glyphs[index];
        if(glyph.codepoint>=privateDigitFirst&&glyph.codepoint<privateDigitFirst+digitCount)
            return false;
        if(glyph.codepoint<fullwidthDigitFirst||glyph.codepoint>=fullwidthDigitFirst+digitCount)
            continue;
        unsigned digit=glyph.codepoint-fullwidthDigitFirst;
        if(glyph.page>1||!glyph.width)
            return false;
        auto& slot=glyph.page==0?found.normal[digit]:found.keys[digit];
        if(slot)
            return false;
        slot=&glyph;
    }
    for(unsigned digit=0;digit<digitCount;++digit) {
        if(!found.normal[digit]||!found.keys[digit])
            return false;
        // The numeric keys occupy one row of 32-pixel cells in the keyboard atlas.
        auto key=found.keys[digit];
        unsigned expectedX=digit==0?zeroNumericKeyX:firstNumericKeyX+(digit-1)*numericKeyCellSize;
        if(key->x!=expectedX||key->y!=numericKeyY||key->width!=numericKeyCellSize)
            return false;
    }
    return true;
}

inline void separateDigitKeys(const DigitGlyphs& found) {
    for(unsigned digit=0;digit<digitCount;++digit)
        found.keys[digit]->codepoint=privateDigitFirst+digit;
}

// Previously converted UI strings may survive a failed font reload. Keep them resolvable
// without taking over a private code already used by the replacement font.
inline void keepDigitKeyAliases(FontGlyph** lookup) {
    if(!lookup)
        return;
    for(unsigned digit=0;digit<digitCount;++digit) {
        auto& alias=lookup[privateDigitFirst+digit];
        if(!alias)
            alias=lookup[fullwidthDigitFirst+digit];
    }
}

}  // namespace unofficial_patch
