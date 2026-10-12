// shouldReturnProperty on synthetic script names and flags. No game files.
#include <cstdio>
#include <cstring>
#include "property_return.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

int main() {
    using unofficial_patch::shouldReturnProperty;
    const char rescue[]="scenario/p420/BR_14_00_S210";
    check("S210 with the property taken: return it",shouldReturnProperty(rescue,1));
    check("S210 after the property was given back: nothing",!shouldReturnProperty(rescue,0));
    // The other branch of BR_14_00_S050 and the scenes around it.
    check("S060 (fight branch): nothing",!shouldReturnProperty("scenario/p420/BR_14_00_S060",1));
    check("S140 (gives back by itself): nothing",!shouldReturnProperty("scenario/p420/BR_14_00_S140",1));
    check("BR_15_00: nothing",!shouldReturnProperty("scenario/p420/BR_15_00",1));
    check("torture/TortureFree: nothing",!shouldReturnProperty("torture/TortureFree",1));
    check("a longer name starting with S210: nothing",!shouldReturnProperty("scenario/p420/BR_14_00_S2100",1));
    check("a shorter name: nothing",!shouldReturnProperty("scenario/p420/BR_14_00_S21",1));
    check("empty name: nothing",!shouldReturnProperty("",1));
    check("no name: nothing",!shouldReturnProperty(nullptr,1));

    // SetNextScript copies at most 0x80 bytes, so the buffer may have no terminator.
    char full[unofficial_patch::nextScriptSize+1];
    memset(full,'a',sizeof(full));
    full[unofficial_patch::nextScriptSize]='\0';
    check("an unterminated 0x80-byte name: nothing",!shouldReturnProperty(full,1));

    printf("RESULT tests=%u failures=%u; synthetic data only\n",tests,failures);
    return failures?1:0;
}
