// Installs mod.dll on a copy of the exe mapped into this process and runs the
// game's full screen setup (0x538180) as WM_ACTIVATEAPP does. On the copy,
// GetSystemMetrics answers like Windows on a 125% monitor of 3840x2160,
// SetWindowLongA, SetWindowPos and ChangeDisplaySettingsA only record the call,
// and the renderer setup (0x4ddc70) only counts it, so no real window, display
// mode or device is changed. The window the Mod inspects is a hidden window of
// this test. Never attaches to the game process or reads saves.
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr int physicalWidth = 3840, physicalHeight = 2160;
constexpr int scaledWidth = 3072, scaledHeight = 1728;  // 125%

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

static unsigned windowCalls, displayModeCalls, rendererSetups;
static LONG WINAPI recordSetWindowLong(HWND, int, LONG) { ++windowCalls; return 0; }
static BOOL WINAPI recordSetWindowPos(HWND, HWND, int, int, int, int, UINT) { ++windowCalls; return TRUE; }
static LONG WINAPI recordChangeDisplaySettings(DEVMODEA*, DWORD) { ++displayModeCalls; return DISP_CHANGE_SUCCESSFUL; }
static bool __stdcall countRendererSetup(void*, int, int) { ++rendererSetups; return true; }

static void jumpTo(unsigned from, void* to) {
    auto code=(unsigned char*)va(from);
    code[0]=0xe9;
    *(int*)(code+1)=(int)((unsigned)to-(va(from)+5));
}

static unsigned char window[0x100];  // The window object: +4 HWND, +0xec full screen

static void resetCounts() { windowCalls=displayModeCalls=rendererSetups=0; }

// 0x538180 as the game calls it: the window object in eax, the bool on the stack.
static void applyFullScreen(bool fullScreen) {
    auto function=(void*)va(0x538180);
    auto windowObject=(void*)window;
    int argument=fullScreen?1:0;
    __asm {
        mov eax, windowObject
        push argument
        call function
    }
}

static bool applied() { return rendererSetups==1&&windowCalls>0; }
static bool untouched() { return rendererSetups==0&&windowCalls==0&&displayModeCalls==0; }

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
    *(void**)va(0x905400)=(void*)&recordSetWindowLong;
    *(void**)va(0x905480)=(void*)&recordSetWindowPos;
    *(void**)va(0x90547c)=(void*)&recordChangeDisplaySettings;
    *(void**)va(0x905228)=(void*)&memset;  // msvcr memset, through the jump at 0x8d4b06
    jumpTo(0x4ddc70,(void*)&countRendererSetup);
    *(int*)va(0x9d30d4)=physicalWidth;
    *(int*)va(0x9d30d8)=physicalHeight;
    *(void**)va(0x9ebb8c)=nullptr;  // No UI manager to update

    // Hidden, so nothing appears on the screen; placed as the game places its full screen window.
    HWND handle=CreateWindowExA(0,"STATIC","",WS_POPUP|WS_SYSMENU,0,0,scaledWidth,scaledHeight,
        nullptr,nullptr,nullptr,nullptr);
    if(!handle) return 4;
    *(HWND*)(window+4)=handle;
    window[0xec]=1;

    applyFullScreen(true);
    check("without the Mod, activation applies full screen again (renderer setup and window calls)",applied());
    resetCounts();

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x538180 is claimed (7 bytes)",claimed(0x538180,7));
    check("the installation is logged",loggedText("full screen re-apply skip installed at 0x538180"));

    applyFullScreen(true);
    check("nothing changed: no renderer setup, window call or display mode change",untouched());
    check("the skip is logged",loggedText(
        "full screen re-apply skipped: the display is already 3840x2160 and the window covers it"));
    check("the thread is DPI unaware again after the check",threadDpiAwareness()==0);
    check("the full screen flag stays set",window[0xec]==1);

    SetWindowPos(handle,nullptr,0,0,physicalWidth,physicalHeight,SWP_NOZORDER|SWP_NOACTIVATE);
    applyFullScreen(true);
    check("window larger than the screen: applied again",applied());
    check("the reason is logged",loggedText("full screen applied again: the window does not cover the screen"));
    SetWindowPos(handle,nullptr,0,0,scaledWidth,scaledHeight,SWP_NOZORDER|SWP_NOACTIVATE);
    resetCounts();

    *(int*)va(0x9d30d4)=2560; *(int*)va(0x9d30d8)=1440;
    applyFullScreen(true);
    check("settings differ from the display mode: applied again",applied()&&displayModeCalls>0);
    check("the reason is logged",loggedText("full screen applied again: the display mode differs from the settings"));
    *(int*)va(0x9d30d4)=physicalWidth; *(int*)va(0x9d30d8)=physicalHeight;
    resetCounts();

    window[0xec]=0;
    applyFullScreen(true);
    check("entering full screen from windowed mode: applied",applied()&&window[0xec]==1);
    resetCounts();

    logged.clear();
    applyFullScreen(false);
    check("switching to windowed mode: the game's code runs",rendererSetups==1&&window[0xec]==0);
    check("switching to windowed mode is not logged as a re-apply",!loggedText("full screen"));

    DestroyWindow(handle);
    printf("RESULT tests=%u failures=%u; mapped exe copy and a hidden window in this process only, "
        "display mode not changed\n",tests,failures);
    return failures?1:0;
}
