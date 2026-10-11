// Installs mod.dll on a copy of the exe mapped into this process and sends
// messages to the game's window procedure (0x5384c0). On the copy,
// DefWindowProcA only records the message, so no real window is painted.
// Never attaches to the game process or reads saves.
#include "mapped_exe.h"

using namespace mapped_exe;

static UINT lastDefaultMessage;
static unsigned defaultCalls;
constexpr LRESULT defaultResult = 0x1234;

static LRESULT WINAPI recordDefWindowProc(HWND, UINT message, WPARAM, LPARAM) {
    ++defaultCalls;
    lastDefaultMessage=message;
    return defaultResult;
}

static unsigned char window[0x100];  // The window object at [0x9ecf90]; only read for other messages

static LRESULT sendToGame(UINT message) {
    auto procedure=(WNDPROC)va(0x5384c0);
    return procedure(nullptr,message,0,0);
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    *(void**)va(0x9053e4)=(void*)&recordDefWindowProc;
    *(void**)va(0x9ecf90)=window;  // The window procedure asserts that the window object exists

    check("without the Mod, WM_ERASEBKGND goes to DefWindowProcA (white brush)",
        sendToGame(WM_ERASEBKGND)==defaultResult&&defaultCalls==1&&lastDefaultMessage==WM_ERASEBKGND);
    defaultCalls=0;

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x5384c0 is claimed (6 bytes)",claimed(0x5384c0,6));
    check("the installation is logged",loggedText("no white background installed at 0x5384c0"));

    check("with the Mod, WM_ERASEBKGND reports the background as erased",sendToGame(WM_ERASEBKGND)==1);
    check("with the Mod, WM_ERASEBKGND does not reach DefWindowProcA",defaultCalls==0);
    check("other messages still reach the game's procedure and DefWindowProcA",
        sendToGame(WM_NULL)==defaultResult&&defaultCalls==1&&lastDefaultMessage==WM_NULL);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only, no window painted\n",tests,failures);
    return failures?1:0;
}
