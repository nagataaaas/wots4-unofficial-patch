// fullScreenReapplyReason on synthetic window states. No game files, no windows.
#include <cstdio>
#include "full_screen_reapply.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

// Full screen 3840x2160 on a 125% monitor: the window covers 3072x1728 in its own coordinates.
static bug_fix::FullScreenState unchanged() {
    return {true,true,3840,2160,3840,2160,false,true,0,0,3072,1728,3072,1728};
}

int main() {
    using bug_fix::fullScreenReapplyReason;
    auto state=unchanged();
    check("nothing changed: skipped",fullScreenReapplyReason(state)==nullptr);

    state=unchanged(); state.requestedFullScreen=false;
    check("switching to windowed mode: applied",fullScreenReapplyReason(state)!=nullptr);
    state=unchanged(); state.alreadyFullScreen=false;
    check("entering full screen (Alt+Enter): applied",fullScreenReapplyReason(state)!=nullptr);
    state=unchanged(); state.physicalWidth=state.physicalHeight=0;
    check("physical size unknown: applied",fullScreenReapplyReason(state)!=nullptr);
    state=unchanged(); state.physicalWidth=1920; state.physicalHeight=1080;
    check("display mode restored to another size on leaving: applied",fullScreenReapplyReason(state)!=nullptr);
    state=unchanged(); state.minimized=true;
    check("minimized: applied",fullScreenReapplyReason(state)!=nullptr);
    state=unchanged(); state.popupStyle=false;
    check("window with a frame: applied",fullScreenReapplyReason(state)!=nullptr);
    state=unchanged(); state.windowRight=3840; state.windowBottom=2160;
    check("window larger than the screen: applied",fullScreenReapplyReason(state)!=nullptr);
    state=unchanged(); state.windowLeft=-8;
    check("window moved: applied",fullScreenReapplyReason(state)!=nullptr);

    printf("RESULT tests=%u failures=%u; synthetic data only\n",tests,failures);
    return failures?1:0;
}
