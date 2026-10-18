// RenderTargetRecovery on synthetic render target groups. No game files, no device.
#include <cstdio>
#include "render_target_recovery.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

int main() {
    int sceneGroup, defaultGroup;
    unofficial_patch::RenderTargetRecovery recovery;

    check("a call outside any other runs",recovery.mayBegin(&defaultGroup)&&!recovery.resetNeeded);

    // The default group fails and the game falls back to it again.
    auto outer=recovery.start(&defaultGroup);
    check("the fallback into the failing default group is stopped",!recovery.mayBegin(&defaultGroup));
    check("a device reset is requested",recovery.resetNeeded);
    recovery.finish(outer);
    check("after the call nothing is in progress",recovery.groupInProgress==nullptr);

    // A scene group fails: the fallback to the default group runs, its own fallback is stopped.
    recovery.resetNeeded=false;
    auto aroundScene=recovery.start(&sceneGroup);
    check("the fallback from another group to the default group runs",recovery.mayBegin(&defaultGroup));
    auto aroundDefault=recovery.start(&defaultGroup);
    check("its second fallback into the default group is stopped",!recovery.mayBegin(&defaultGroup));
    recovery.finish(aroundDefault);
    check("the scene group is in progress again after the fallback",recovery.groupInProgress==&sceneGroup);
    recovery.finish(aroundScene);
    check("after both calls nothing is in progress and a reset is requested",
        recovery.groupInProgress==nullptr&&recovery.resetNeeded);

    recovery.resetNeeded=false;
    recovery.start(nullptr);
    check("a null group is passed to the game, which refuses it itself",recovery.mayBegin(nullptr)&&!recovery.resetNeeded);

    printf("RESULT tests=%u failures=%u; synthetic data only\n",tests,failures);
    return failures?1:0;
}
