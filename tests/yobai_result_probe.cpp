// discardYobaiResult on synthetic night visit progress objects. No game files.
#include <cstdio>
#include <cstring>
#include "yobai_result.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

constexpr unsigned progressSize = 0x44;
constexpr unsigned char untouched = 0x5a;

// A progress object with every byte set to untouched except the result and the handled mark.
static void makeProgress(unsigned char* progress, signed char result, unsigned char handled) {
    memset(progress,untouched,progressSize);
    progress[bug_fix::yobaiResultOffset]=static_cast<unsigned char>(result);
    progress[bug_fix::yobaiHandledOffset]=handled;
}

static bool isCleared(const unsigned char* progress) {
    return static_cast<signed char>(progress[bug_fix::yobaiResultOffset])==bug_fix::noYobaiResult &&
        progress[bug_fix::yobaiHandledOffset]==0;
}

static bool othersUntouched(const unsigned char* progress) {
    for(unsigned offset=0;offset<progressSize;++offset) {
        if(offset==bug_fix::yobaiResultOffset||offset==bug_fix::yobaiHandledOffset)
            continue;
        if(progress[offset]!=untouched)
            return false;
    }
    return true;
}

int main() {
    using bug_fix::discardYobaiResult;
    unsigned char progress[progressSize];

    // What the reported bug leaves when a save file is loaded: the result was handed out, then the
    // player returned to the title.
    const signed char handledResults[]={0,1,2,3};
    for(auto result:handledResults) {
        makeProgress(progress,result,1);
        char label[96];
        sprintf_s(label,"handled result %d: discarded and reported",result);
        check(label,discardYobaiResult(progress)==result&&isCleared(progress));
    }
    makeProgress(progress,0,1);
    discardYobaiResult(progress);
    check("only the result and the handled mark change",othersUntouched(progress));

    // A visit whose result was not handed out yet does not belong to a loaded save file either.
    makeProgress(progress,1,0);
    check("result not handed out yet: discarded too",discardYobaiResult(progress)==1&&isCleared(progress));

    makeProgress(progress,bug_fix::noYobaiResult,0);
    check("no result: nothing reported, stays cleared",
        discardYobaiResult(progress)==bug_fix::noYobaiResult&&isCleared(progress)&&othersUntouched(progress));
    makeProgress(progress,bug_fix::noYobaiResult,1);
    check("no result with the mark set: the mark is cleared, nothing reported",
        discardYobaiResult(progress)==bug_fix::noYobaiResult&&isCleared(progress));

    check("no progress object: nothing",discardYobaiResult(nullptr)==bug_fix::noYobaiResult);

    printf("RESULT tests=%u failures=%u; synthetic data only\n",tests,failures);
    return failures?1:0;
}
