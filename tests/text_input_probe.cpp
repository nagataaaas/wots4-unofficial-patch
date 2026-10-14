// KeyboardMappingPause and acceptsTextInputCharacter with synthetic values. No game files.
#include <cstdio>
#include <cstdint>
#include "text_input.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

static void checkPause() {
    using unofficial_patch::KeyboardMappingPause;
    {
        KeyboardMappingPause pause;
        uint8_t mapping=1;
        pause.beforePoll(mapping);
        check("a poll turns the keyboard buttons off",mapping==0&&pause.pausedByTextInput);
        check("an unfinished poll keeps them off",!pause.afterPoll(false,mapping)&&mapping==0);
        check("a frame with a poll keeps them off",!pause.onFrame(mapping)&&mapping==0);
        pause.beforePoll(mapping);
        check("a finished poll turns them on again",pause.afterPoll(true,mapping)&&mapping==1&&!pause.pausedByTextInput);
    }
    {
        KeyboardMappingPause pause;
        uint8_t mapping=1;
        pause.beforePoll(mapping);
        pause.afterPoll(false,mapping);
        pause.onFrame(mapping);
        check("a frame without a poll turns them on again",pause.onFrame(mapping)&&mapping==1);
        check("later frames change nothing",!pause.onFrame(mapping)&&mapping==1);
    }
    {
        KeyboardMappingPause pause;
        uint8_t mapping=0;  // Turned off by the player name input
        pause.beforePoll(mapping);
        check("buttons turned off by someone else are not taken over",mapping==0&&!pause.pausedByTextInput);
        check("finishing does not turn on what someone else turned off",!pause.afterPoll(true,mapping)&&mapping==0);
        pause.onFrame(mapping);
        check("frames do not turn on what someone else turned off",!pause.onFrame(mapping)&&mapping==0);
    }
    {
        KeyboardMappingPause pause;
        uint8_t mapping=1;
        check("frames without any text input change nothing",!pause.onFrame(mapping)&&!pause.onFrame(mapping)&&mapping==1);
    }
}

static void checkCharacters() {
    using unofficial_patch::acceptsTextInputCharacter;
    const uint32_t accepted[]={'0','9','A','Z','a','z',0x3042,0x30a8,0x30fc,0x4f8d,0x5200,0x6d41,0x9b3c,0x90ce,
        0x3000,0x3001,0xff21,0xff11,0xff71,0xff5e,0x00e9};
    const uint32_t rejected[]={0,8,0x0d,0x1b,' ','-','_','!','~',0x7f,0x80,0x9f,0xd800,0xdfff,0xe000,0xe010,0xe019,
        0xf8ff,0xfff0,0xfffd,0xffff};
    for(auto codeUnit:accepted) {
        char label[64];
        sprintf_s(label,"U+%04X is taken",codeUnit);
        check(label,acceptsTextInputCharacter(codeUnit));
    }
    for(auto codeUnit:rejected) {
        char label[64];
        sprintf_s(label,"U+%04X is not taken",codeUnit);
        check(label,!acceptsTextInputCharacter(codeUnit));
    }
}

int main() {
    checkPause();
    checkCharacters();
    printf("RESULT tests=%u failures=%u; synthetic values only\n",tests,failures);
    return failures?1:0;
}
