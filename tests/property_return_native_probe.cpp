// Installs mod.dll on a copy of the exe mapped into this process and runs the
// hooked SetNextScript (0x55e140). The rest of the game's SetNextScript and the
// body of PlayerGiveProperty (0x553f60) need the running game, so on the copy
// they are replaced after installation: SetNextScript returns right after its
// first 5 bytes (the part MinHook moves), and PlayerGiveProperty only records
// the call. Never attaches to the game process or reads saves.
#include <cstdlib>
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr unsigned propertyTakenOffset = 0x48b9e;
static unsigned char* gameState;     // [0x9e967c]
static unsigned char* scriptState;   // [0x9eb5f8]
static unsigned giveCalls;
static bool giveFindsPlayer = true;

// Stands in for 0x553f60: the game clears the flag only when it finds the player character.
static void __cdecl recordGive() {
    ++giveCalls;
    if(giveFindsPlayer)
        gameState[propertyTakenOffset]=0;
}

static void jumpTo(unsigned from, void* to) {
    auto code=(unsigned char*)va(from);
    code[0]=0xe9;
    *(int*)(code+1)=(int)((unsigned)to-(va(from)+5));
}

// SetNextScript as a script calls it: cdecl, the script context as the only argument.
static bool callSetNextScript(const char* name) {
    if(scriptState)
        strcpy_s((char*)scriptState+0x2c,0x80,name);  // What the game's own copy at 0x55e1ac leaves
    auto function=(bool (__cdecl*)(void*))va(0x55e140);
    return function(nullptr);
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    const unsigned char originalRelease[]={0x53,0x56,0x57,0x8b,0xf9};
    gameState=(unsigned char*)calloc(1,propertyTakenOffset+0x100);
    scriptState=(unsigned char*)calloc(1,0x100);
    *(void**)va(0x9e967c)=gameState;
    *(void**)va(0x9eb5f8)=scriptState;

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }

    // Only the follow-effect detach code differs: the property return is still installed.
    auto joint=(unsigned char*)va(0x56590e);
    joint[3]=0x4c;
    check("with the follow-effect detach code changed, the Mod still initializes",initialize(&context)!=FALSE);
    check("0x55e140 is claimed (5 bytes) and 0x48d520 is not",
        claimed(0x55e140,5)&&!claimed(0x48d520,5)&&releases==0);
    check("0x48d520 is left unchanged",!memcmp((void*)va(0x48d520),originalRelease,sizeof(originalRelease)));
    check("the skipped fix is logged with the reason",
        loggedText("follow-effect detach skipped: the game code at 0x48d520 is not the known version"));
    check("the hook is written at 0x55e140",*(unsigned char*)va(0x55e140)==0xe9);
    joint[3]=0x48;

    // mov esp,ebp; pop ebp; mov al,1; ret: undoes the moved prologue and reports success.
    const unsigned char setNextScriptReturn[]={0x8b,0xe5,0x5d,0xb0,0x01,0xc3};
    memcpy((void*)va(0x55e145),setNextScriptReturn,sizeof(setNextScriptReturn));
    jumpTo(0x553f60,(void*)&recordGive);

    gameState[propertyTakenOffset]=1;
    check("the hook returns the game's result",callSetNextScript("scenario/p420/BR_14_00_S060"));
    check("S060 (fight branch) with the property taken: not given back",giveCalls==0&&gameState[propertyTakenOffset]==1);
    callSetNextScript("scenario/p420/BR_14_00_S140");
    check("S140 (gives back by itself): not given back by the Mod",giveCalls==0);

    callSetNextScript("scenario/p420/BR_14_00_S210");
    check("S210 with the property taken: PlayerGiveProperty is called once",
        giveCalls==1&&gameState[propertyTakenOffset]==0);
    check("the return is logged",loggedText("returned the player's weapons, items and outfit after Madara's rescue"));
    callSetNextScript("scenario/p420/BR_14_00_S210");
    check("S210 again after the return: not called again",giveCalls==1);

    giveFindsPlayer=false;
    gameState[propertyTakenOffset]=1;
    callSetNextScript("scenario/p420/BR_14_00_S210");
    check("when the game finds no player, the failure is logged",
        giveCalls==2&&loggedText("found no player character; they stay taken"));

    *(void**)va(0x9eb5f8)=nullptr;
    auto savedScriptState=scriptState;
    scriptState=nullptr;
    check("no script state: nothing is called",callSetNextScript("")&&giveCalls==2);
    scriptState=savedScriptState;
    *(void**)va(0x9eb5f8)=scriptState;
    *(void**)va(0x9e967c)=nullptr;
    check("no game state: nothing is called",callSetNextScript("scenario/p420/BR_14_00_S210")&&giveCalls==2);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only\n",tests,failures);
    return failures?1:0;
}
