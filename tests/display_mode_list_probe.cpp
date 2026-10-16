// gameListsDisplayMode on synthetic modes. No game files.
#include <cstdio>
#include "display_mode_list.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

int main() {
    using unofficial_patch::gameListsDisplayMode;
    check("3840x2160 32-bit 60 Hz: listed",gameListsDisplayMode(32,3840,60));
    check("640x480 32-bit 144 Hz (the narrowest listed width): listed",gameListsDisplayMode(32,640,144));
    check("1024x768 16-bit 144 Hz: not listed",!gameListsDisplayMode(16,1024,144));
    check("1024x768 8-bit 60 Hz: not listed",!gameListsDisplayMode(8,1024,60));
    check("1024x768 24-bit 60 Hz: not listed",!gameListsDisplayMode(24,1024,60));
    check("639 pixels wide: not listed",!gameListsDisplayMode(32,639,60));
    check("320x200 32-bit 70 Hz: not listed",!gameListsDisplayMode(32,320,70));
    check("59 Hz: not listed",!gameListsDisplayMode(32,1920,59));
    // EnumDisplaySettings reports 0 or 1 for the hardware's default rate; the game drops it too.
    check("1 Hz (the default rate): not listed",!gameListsDisplayMode(32,1920,1));

    printf("RESULT tests=%u failures=%u; synthetic data only\n",tests,failures);
    return failures?1:0;
}
