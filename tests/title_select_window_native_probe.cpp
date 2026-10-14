// Installs mod.dll on a copy of the exe mapped into this process and calls the hooked Options
// state of the title (0x4ee380) with a synthetic title and UI manager. The state's body needs the
// running game, so on the copy, after installation, the code right after the part MinHook moves
// records the title it was given and returns. Never attaches to the game process or reads saves.
#include <cstdlib>
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr unsigned titleOptionsState = 0x4ee380;
constexpr unsigned afterMovedInstructions = 0x4ee386;  // push ebp; mov ebp,esp; mov eax,[ebp+8]
constexpr unsigned titleSelectWindowOffset = 0x4c;
constexpr unsigned titleUiManagerOffset = 0x58;
constexpr unsigned uiManagerSelectWindowOffset = 0x70;
constexpr int updateEvent = 0;
static unsigned char* titleSeenByGame;

// mov [titleSeenByGame],ecx; mov esp,ebp; pop ebp; ret 4
static void writeStateStub() {
    unsigned char stub[]={0x89,0x0d,0,0,0,0,0x8b,0xe5,0x5d,0xc2,0x04,0x00};
    *(unsigned*)(stub+2)=(unsigned)&titleSeenByGame;
    memcpy((void*)va(afterMovedInstructions),stub,sizeof(stub));
}

static void callTitleOptionsState(unsigned char* title, int event) {
    auto function=va(titleOptionsState);
    __asm {
        push event
        mov ecx, title
        call function
    }
}

static void*& field(unsigned char* object, unsigned offset) { return *(void**)(object+offset); }

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }

    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x4ee380 is claimed (6 bytes) and hooked",
        claimed(titleOptionsState,6)&&*(unsigned char*)va(titleOptionsState)==0xe9);
    check("the install is logged",loggedText("title select window refresh installed at 0x4ee380"));
    writeStateStub();

    // The title kept the window it made; Options made the one the UI manager holds now.
    unsigned char title[0x60]={};
    unsigned char uiManager[0x90]={};
    unsigned char deletedWindow[0xf0]={};
    unsigned char currentWindow[0xf0]={};
    field(title,titleUiManagerOffset)=uiManager;
    field(title,titleSelectWindowOffset)=deletedWindow;
    field(uiManager,uiManagerSelectWindowOffset)=currentWindow;

    callTitleOptionsState(title,updateEvent);
    check("the game's Options state runs with the title",titleSeenByGame==title);
    check("after Options the title uses the UI manager's select window",
        field(title,titleSelectWindowOffset)==currentWindow);
    check("the change is logged",loggedText("Options replaced the title's select window"));

    logged.clear();
    callTitleOptionsState(title,updateEvent);
    check("the next frame changes and logs nothing",
        field(title,titleSelectWindowOffset)==currentWindow&&logged.empty());

    field(uiManager,uiManagerSelectWindowOffset)=nullptr;
    callTitleOptionsState(title,updateEvent);
    check("without a select window in the UI manager the title's is kept",
        field(title,titleSelectWindowOffset)==currentWindow);

    field(title,titleUiManagerOffset)=nullptr;
    callTitleOptionsState(title,updateEvent);
    check("without a UI manager nothing is read",field(title,titleSelectWindowOffset)==currentWindow);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only\n",tests,failures);
    return failures?1:0;
}
