// Installs mod.dll on a copy of the exe mapped into this process and runs the
// game's display mode check (0x5383d0). Its imports are replaced on the copy:
// GetSystemMetrics answers like Windows does on a 125% monitor of 3840x2160
// (3072x1728 to a DPI unaware thread), ChangeDisplaySettingsA only records
// the call, so the real display mode is never changed, and memset is the
// test's own. Never attaches to the
// game process or reads saves.
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr int physicalWidth = 3840, physicalHeight = 2160;
constexpr int scaledWidth = 3072, scaledHeight = 1728;  // 125%
constexpr DWORD testFlag = 2, fullscreenFlag = 4;       // CDS_TEST, CDS_FULLSCREEN

using GetThreadDpiAwarenessContextFunction = HANDLE (WINAPI*)();
using GetAwarenessFromDpiAwarenessContextFunction = int (WINAPI*)(HANDLE);
static GetThreadDpiAwarenessContextFunction getThreadDpiAwarenessContext;
static GetAwarenessFromDpiAwarenessContextFunction getAwarenessFromDpiAwarenessContext;

static int threadDpiAwareness() { return getAwarenessFromDpiAwarenessContext(getThreadDpiAwarenessContext()); }

static int WINAPI fakeGetSystemMetrics(int index) {
    bool unaware=threadDpiAwareness()==0;  // DPI_AWARENESS_UNAWARE
    if(index==SM_CXSCREEN) return unaware?scaledWidth:physicalWidth;
    if(index==SM_CYSCREEN) return unaware?scaledHeight:physicalHeight;
    return 0;
}

struct DisplayModeRequest { DWORD flags, width, height; };
static std::vector<DisplayModeRequest> displayModeRequests;
static LONG WINAPI recordChangeDisplaySettings(DEVMODEA* mode, DWORD flags) {
    displayModeRequests.push_back({flags,mode?mode->dmPelsWidth:0,mode?mode->dmPelsHeight:0});
    return DISP_CHANGE_SUCCESSFUL;
}

static unsigned char window[0x100];  // The window object: 0x5383d0 writes the new size at +0xbc/+0xc0

// 0x5383d0 as the game calls it: the width in ebx, (window, height) on the stack.
static bool checkDisplayMode(int width, int height) {
    auto function=(void*)va(0x5383d0);
    auto windowObject=(void*)window;
    unsigned char changed;
    __asm {
        push ebx
        mov ebx, width
        push height
        push windowObject
        call function
        pop ebx
        mov changed, al
    }
    return changed!=0;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    auto user32=LoadLibraryA("user32.dll");
    getThreadDpiAwarenessContext=(GetThreadDpiAwarenessContextFunction)GetProcAddress(user32,"GetThreadDpiAwarenessContext");
    getAwarenessFromDpiAwarenessContext=(GetAwarenessFromDpiAwarenessContextFunction)GetProcAddress(user32,
        "GetAwarenessFromDpiAwarenessContext");
    if(!getThreadDpiAwarenessContext||!getAwarenessFromDpiAwarenessContext) {
        printf("SKIP this Windows has no per-thread DPI awareness\n");
        return 0;
    }
    if(!mapImage(argv[1])) return 3;
    *(void**)va(0x905404)=(void*)&fakeGetSystemMetrics;
    *(void**)va(0x90547c)=(void*)&recordChangeDisplaySettings;
    *(void**)va(0x905228)=(void*)&memset;  // msvcr memset, through the jump at 0x8d4b06
    check("the test thread starts DPI unaware",threadDpiAwareness()==0);

    check("without the Mod, 3840x2160 on a 125% screen is not seen as the current size",
        checkDisplayMode(physicalWidth,physicalHeight));
    check("without the Mod, the display mode is changed (CDS_TEST, then CDS_FULLSCREEN to 3840x2160)",
        displayModeRequests.size()==2&&displayModeRequests[0].flags==testFlag&&
        displayModeRequests[1].flags==fullscreenFlag&&displayModeRequests[1].width==physicalWidth&&
        displayModeRequests[1].height==physicalHeight);
    displayModeRequests.clear();

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x5383d0 is claimed (9 bytes)",claimed(0x5383d0,9));
    check("the hook is written at 0x5383d0",*(unsigned char*)va(0x5383d0)==0xe9);
    check("the installation is logged",loggedText("physical screen size check installed at 0x5383d0"));

    memset(window,0,sizeof(window));
    check("with the Mod, 3840x2160 is the current size: the check reports no change",
        !checkDisplayMode(physicalWidth,physicalHeight));
    check("with the Mod, ChangeDisplaySettingsA is not called",displayModeRequests.empty());
    check("the window object is left unchanged",*(int*)(window+0xbc)==0&&*(int*)(window+0xc0)==0);
    check("the thread is DPI unaware again after the check",threadDpiAwareness()==0);
    check("the skipped change is logged",loggedText(
        "display mode check: the game wants 3840x2160, the primary screen is 3840x2160 (physical pixels); "
        "no display mode change"));

    check("with the Mod, another size (2560x1440) still changes the display mode",
        checkDisplayMode(2560,1440));
    check("the change to 2560x1440 is requested as before",
        displayModeRequests.size()==2&&displayModeRequests[1].flags==fullscreenFlag&&
        displayModeRequests[1].width==2560&&displayModeRequests[1].height==1440);
    check("the new size is stored in the window object",*(int*)(window+0xbc)==2560&&*(int*)(window+0xc0)==1440);
    check("the thread is DPI unaware again after the change",threadDpiAwareness()==0);
    check("the requested change is logged",loggedText("the game wants 2560x1440, the primary screen is 3840x2160 "
        "(physical pixels); the game changes the display mode"));

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only, display mode not changed\n",
        tests,failures);
    return failures?1:0;
}
