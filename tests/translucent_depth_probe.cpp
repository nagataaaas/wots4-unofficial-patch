// TranslucentDepthMask on synthetic render states. No game files, no GPU.
#include <cstdio>
#include "translucent_depth.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

int main() {
    bug_fix::TranslucentDepthMask mask;
    check("Direct3D defaults (opaque): all channels",mask.effective()==bug_fix::allColorChannels);
    mask.blending=true;
    check("blending: no channels",mask.effective()==0);
    mask.requested=0x7;
    check("blending with a mask asked for: still no channels",mask.effective()==0);
    mask.blending=false;
    check("opaque again: the mask asked for",mask.effective()==0x7);

    printf("RESULT tests=%u failures=%u; synthetic data only\n",tests,failures);
    return failures?1:0;
}
