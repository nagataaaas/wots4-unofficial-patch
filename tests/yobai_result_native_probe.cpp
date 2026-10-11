// Installs mod.dll on a copy of the exe mapped into this process and runs the
// hooked end of the play session (0x4e18e0). Its body needs the running game,
// so on the copy, after installation, the code right after the part MinHook
// moves jumps to the game's own last lines (0x4e2070): they clear the handled
// mark of the night visit progress and return. Never attaches to the game
// process or reads saves.
#include <cstdlib>
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr unsigned yobaiProgressGlobal = 0x9ecbb8;
constexpr unsigned progressSize = 0x44;
constexpr unsigned resultOffset = 0x9;
constexpr unsigned handledOffset = 0xa;
static unsigned char* progress;

static void writeJump(unsigned from, unsigned to) {
    auto code=(unsigned char*)va(from);
    code[0]=0xe9;
    *(int*)(code+1)=(int)(va(to)-(va(from)+5));
}

static void endPlaySession() {
    auto function=(void (__stdcall*)(void*))va(0x4e18e0);
    function(nullptr);
}

static void setProgress(signed char result, unsigned char handled) {
    progress[resultOffset]=(unsigned char)result;
    progress[handledOffset]=handled;
}

static bool progressIs(signed char result, unsigned char handled) {
    return (signed char)progress[resultOffset]==result&&progress[handledOffset]==handled;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    progress=(unsigned char*)calloc(1,progressSize);
    *(void**)va(yobaiProgressGlobal)=progress;

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }

    // Without the Mod: the game's prologue and its last lines, from a copy outside the exe image.
    auto unmodded=(unsigned char*)VirtualAlloc(nullptr,0x20,MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    const unsigned char unmoddedBody[]={0x55,0x8b,0xec,0x83,0xe4,0xf8,0x53,0x56,0x57,0xe9};
    memcpy(unmodded,unmoddedBody,sizeof(unmoddedBody));
    *(int*)(unmodded+sizeof(unmoddedBody))=(int)(va(0x4e2070)-((unsigned)unmodded+sizeof(unmoddedBody)+4));
    setProgress(0,1);
    ((void (__stdcall*)(void*))unmodded)(nullptr);
    check("without the Mod the handled result 0 stays after the session ends (the bug)",progressIs(0,0));

    // Only the follow-effect detach code differs: the night visit result reset is still installed.
    auto joint=(unsigned char*)va(0x56590e);
    joint[3]=0x4c;
    check("with the follow-effect detach code changed, the Mod still initializes",initialize(&context)!=FALSE);
    check("0x4e18e0 is claimed (6 bytes) and 0x48d520 is not",
        claimed(0x4e18e0,6)&&!claimed(0x48d520,5)&&releases==0);
    check("the fix is logged as installed",loggedText("night visit result reset installed at 0x4e18e0"));
    check("the hook is written at 0x4e18e0",*(unsigned char*)va(0x4e18e0)==0xe9);
    joint[3]=0x48;

    // push ebx; push esi; push edi (as the game's body does), then the game's last lines at 0x4e2070:
    // mov ecx,[0x9ecbb8]; mov byte [ecx+0xa],0; pop edi; pop esi; pop ebx; mov esp,ebp; pop ebp; ret 4
    const unsigned char saveRegisters[]={0x53,0x56,0x57};
    memcpy((void*)va(0x4e18e6),saveRegisters,sizeof(saveRegisters));
    writeJump(0x4e18e9,0x4e2070);

    setProgress(0,1);
    endPlaySession();
    check("handled success (0) at the end of the session: result -1, mark 0",progressIs(-1,0));
    check("the discarded result is logged",
        loggedText("discarded the night visit result 0 left from the play session that ended"));

    setProgress(3,1);
    endPlaySession();
    check("handled failure (3): result -1, mark 0",progressIs(-1,0));
    setProgress(1,0);
    endPlaySession();
    check("consolation (1) not handed out yet: result -1, mark 0",progressIs(-1,0));

    size_t loggedBefore=logged.size();
    setProgress(-1,1);
    endPlaySession();
    check("no result: the game's own clear of the mark still runs",progressIs(-1,0));
    check("no result: nothing is logged",logged.size()==loggedBefore);

    // The caller's registers survive the hook and the game's code.
    unsigned ebxAfter=0, esiAfter=0, ediAfter=0;
    auto function=va(0x4e18e0);
    __asm {
        push ebx
        push esi
        push edi
        mov ebx, 0x11111111
        mov esi, 0x22222222
        mov edi, 0x33333333
        push 0
        call function
        mov ebxAfter, ebx
        mov esiAfter, esi
        mov ediAfter, edi
        pop edi
        pop esi
        pop ebx
    }
    check("ebx, esi and edi are kept for the caller",
        ebxAfter==0x11111111&&esiAfter==0x22222222&&ediAfter==0x33333333);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only\n",tests,failures);
    return failures?1:0;
}
