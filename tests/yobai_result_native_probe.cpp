// Installs mod.dll on a copy of the exe mapped into this process and runs the
// hooked copy of save data into the game state (0x4d9f50). Its body needs the
// running game, so on the copy, after installation, the code right after the
// part MinHook moves jumps to the game's own last lines (0x4da125: pop the
// saved registers, ret 0xc). Never attaches to the game process or reads saves.
#include <cstdlib>
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr unsigned yobaiProgressGlobal = 0x9ecbb8;
constexpr unsigned progressSize = 0x44;
constexpr unsigned resultOffset = 0x9;
constexpr unsigned handledOffset = 0xa;
constexpr unsigned fromFile = 1;       // Loading a save file (0x53eae9)
constexpr unsigned fromSnapshot = 0;   // Retry after death (0x4e91b6)
static unsigned char* progress;

static void writeJump(unsigned from, unsigned to) {
    auto code=(unsigned char*)va(from);
    code[0]=0xe9;
    *(int*)(code+1)=(int)(va(to)-(va(from)+5));
}

static void loadSaveData(unsigned source) {
    auto function=(void (__stdcall*)(void*, const void*, unsigned))va(0x4d9f50);
    function(nullptr,nullptr,source);
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
    const unsigned char unmoddedBody[]={0x55,0x8b,0xec,0x53,0x56,0x57,0xe9};
    memcpy(unmodded,unmoddedBody,sizeof(unmoddedBody));
    *(int*)(unmodded+sizeof(unmoddedBody))=(int)(va(0x4da125)-((unsigned)unmodded+sizeof(unmoddedBody)+4));
    setProgress(0,0);
    ((void (__stdcall*)(void*, const void*, unsigned))unmodded)(nullptr,nullptr,fromFile);
    check("without the Mod the result 0 stays after a save file is loaded (the bug)",progressIs(0,0));

    // Only the follow-effect detach code differs: the night visit result reset is still installed.
    auto joint=(unsigned char*)va(0x56590e);
    joint[3]=0x4c;
    check("with the follow-effect detach code changed, the Mod still initializes",initialize(&context)!=FALSE);
    check("0x4d9f50 is claimed (5 bytes) and neither 0x48d520 nor 0x4e18e0 is",
        claimed(0x4d9f50,5)&&!claimed(0x48d520,5)&&!claimed(0x4e18e0,6)&&releases==0);
    check("the fix is logged as installed",loggedText("night visit result reset installed at 0x4d9f50"));
    check("the hook is written at 0x4d9f50",*(unsigned char*)va(0x4d9f50)==0xe9);
    check("the end of the play session is left unchanged",*(unsigned char*)va(0x4e18e0)==0x55);
    joint[3]=0x48;

    // MinHook moved push ebp; mov ebp,esp; push ebx; push esi. Keep push edi (0x4d9f55), then the
    // game's last lines at 0x4da125: pop edi; pop esi; pop ebx; pop ebp; ret 0xc.
    writeJump(0x4d9f56,0x4da125);

    // The ending of the 0x4e18e0 session leaves the result with a cleared mark (0x4e2076).
    setProgress(0,0);
    loadSaveData(fromSnapshot);
    check("retry after death (snapshot) keeps the result 0, so its reward is handed out",progressIs(0,0));
    setProgress(3,0);
    loadSaveData(fromSnapshot);
    check("retry after death keeps the failure (3) too",progressIs(3,0));

    setProgress(0,0);
    loadSaveData(fromFile);
    check("loading a save file: result 0 becomes -1, mark 0",progressIs(-1,0));
    check("the discarded result is logged",
        loggedText("discarded the night visit result 0 left in memory while loading a save file"));
    setProgress(3,1);
    loadSaveData(fromFile);
    check("loading a save file: handled failure (3) becomes -1, mark 0",progressIs(-1,0));
    setProgress(2,0);
    loadSaveData(0x100|fromFile);
    check("only the low byte of the argument is read, as the game does",progressIs(-1,0));
    setProgress(1,0);
    loadSaveData(0x100);
    check("an argument with a zero low byte is a snapshot: the result stays",progressIs(1,0));

    size_t loggedBefore=logged.size();
    setProgress(-1,0);
    loadSaveData(fromFile);
    check("no result: nothing changes and nothing is logged",progressIs(-1,0)&&logged.size()==loggedBefore);

    // The caller's registers survive the hook and the game's code.
    unsigned ebxAfter=0, esiAfter=0, ediAfter=0;
    auto function=va(0x4d9f50);
    __asm {
        push ebx
        push esi
        push edi
        mov ebx, 0x11111111
        mov esi, 0x22222222
        mov edi, 0x33333333
        push 1
        push 0
        push 0
        call function
        mov ebxAfter, ebx
        mov esiAfter, esi
        mov ediAfter, edi
        pop edi
        pop esi
        pop ebx
    }
    check("ebx, esi and edi are kept for the caller, and the 3 arguments are popped",
        ebxAfter==0x11111111&&esiAfter==0x22222222&&ediAfter==0x33333333);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only\n",tests,failures);
    return failures?1:0;
}
