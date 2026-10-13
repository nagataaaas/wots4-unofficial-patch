#include <array>
#include <cstdio>
#include <cstring>
#include "fullwidth_digits.h"

using namespace unofficial_patch;
static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

struct Font {
    FontDatabase header{21,32,2,0};
    FontGlyph glyphs[21]{};
};
static Font makeFont() {
    Font font;
    for(unsigned digit=0;digit<digitCount;++digit) {
        font.glyphs[digit]={static_cast<uint16_t>(fullwidthDigitFirst+digit),
            static_cast<uint16_t>(300+digit*20),110,19,0};
        font.glyphs[digit+10]={static_cast<uint16_t>(fullwidthDigitFirst+digit),
            static_cast<uint16_t>(digit==0?480:192+(digit-1)*32),32,32,1};
    }
    font.glyphs[20]={L'2',193,2,16,0};
    return font;
}

int main() {
    auto font=makeFont();
    const auto before=font;
    DigitGlyphs found;
    check("normal digits and numeric keys are found",findDigitGlyphs(&font.header,found));
    check("validation leaves every font byte unchanged",!memcmp(&font,&before,sizeof(font)));
    separateDigitKeys(found);
    for(unsigned digit=0;digit<digitCount;++digit) {
        check("normal fullwidth digit retains its original record",
            !memcmp(&font.glyphs[digit],&before.glyphs[digit],sizeof(FontGlyph)));
        auto expected=before.glyphs[digit+10];
        expected.codepoint=privateDigitFirst+digit;
        check("numeric key moves to its private code with the same image and width",
            !memcmp(&font.glyphs[digit+10],&expected,sizeof(expected)));
    }
    check("ASCII digit retains its original record",!memcmp(&font.glyphs[20],&before.glyphs[20],sizeof(FontGlyph)));

    auto rejected=[&](const char* description, Font changed) {
        const auto snapshot=changed;
        DigitGlyphs plan;
        check(description,!findDigitGlyphs(&changed.header,plan)&&!memcmp(&changed,&snapshot,sizeof(changed)));
    };
    check("missing database is rejected",!findDigitGlyphs(nullptr,found));
    auto changed=makeFont(); changed.header.height=16;
    rejected("unexpected glyph height is rejected without changes",changed);
    changed=makeFont(); changed.header.pages=1;
    rejected("missing keyboard page is rejected without changes",changed);
    changed=makeFont(); changed.header.count=10;
    rejected("missing key records are rejected without changes",changed);
    changed=makeFont(); changed.glyphs[0].codepoint=L'A';
    rejected("missing normal digit is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[20]=changed.glyphs[1];
    rejected("duplicate normal digit is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[20]=changed.glyphs[11];
    rejected("duplicate numeric key is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[20].codepoint=privateDigitFirst;
    rejected("private code collision is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[19].page=2;
    rejected("unexpected digit page is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[9].width=0;
    rejected("empty digit is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[19].x=0;
    rejected("wrong numeric key cell is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[19].y=0;
    rejected("wrong numeric key row is rejected without changes",changed);
    changed=makeFont(); changed.glyphs[19].width=31;
    rejected("wrong numeric key width is rejected without changes",changed);

    std::array<FontGlyph*,65536> lookup{};
    for(auto& glyph:before.glyphs)
        lookup[glyph.codepoint]=const_cast<FontGlyph*>(&glyph);
    lookup[privateDigitFirst]=&font.glyphs[20];
    keepDigitKeyAliases(lookup.data());
    check("reload fallback preserves an existing private glyph",lookup[privateDigitFirst]==&font.glyphs[20]);
    for(unsigned digit=1;digit<digitCount;++digit)
        check("reload fallback resolves an already-converted numeric key",
            lookup[privateDigitFirst+digit]==lookup[fullwidthDigitFirst+digit]);
    lookup[privateDigitFirst+1]=nullptr;
    lookup[fullwidthDigitFirst+1]=nullptr;
    keepDigitKeyAliases(lookup.data());
    check("missing digit is not replaced with an unrelated glyph",lookup[privateDigitFirst+1]==nullptr);
    keepDigitKeyAliases(nullptr);
    check("missing lookup is accepted without dereferencing it",true);
    printf("RESULT tests=%u failures=%u; synthetic font data only\n",tests,failures);
    return failures?1:0;
}
