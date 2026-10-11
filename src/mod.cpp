// bug_fix: fixes crashes and bugs of Way of the Samurai 4 in memory only.
//
// Follow-effect detach: after a character's effects are detached (0x48d520,
// virtual +0x68, called before the model at +0x18 is freed or replaced), also
// detach the effects that follow that model's joints. Prevents the read of a
// freed joint at 0x4a4cf8, where the ending after resuming at the jail
// photographer crashed. Why that model is freed during the scene only after
// resuming there is not known yet (U-CF-2, needs an in-game check).
//
// Property return after Madara's rescue: when SetNextScript (0x55e140) moves to
// BR_14_00_S210 while the player's property is still taken, call the game's
// PlayerGiveProperty (0x553f60), as BR_14_00_S140 does on the other branch.
//
// Each fix is installed on its own, so a changed game or another Mod's hook
// on one of them does not stop the other.
#include <wots4/mod_api.h>
#include <MinHook.h>
#include <cstdio>
#include <cstring>
#include "follow_detach.h"
#include "property_return.h"

namespace {

uintptr_t gameBase;
HMODULE self;
const wots4::HostApi* host;

uintptr_t va(uintptr_t address) { return gameBase+address-0x400000; }

bool same(uintptr_t address, const unsigned char* bytes, size_t count) {
    return !memcmp(reinterpret_cast<const void*>(va(address)),bytes,count);
}

// The 32-bit absolute address stored at address (an operand), or the value of a game global.
uintptr_t readGamePointer(uintptr_t address) { return *reinterpret_cast<const uintptr_t*>(va(address)); }

void log(const char* message) {
    if(host->log)
        host->log(self,message);
}

// ---- Follow-effect detach

constexpr uintptr_t releaseEffects = 0x48d520;  // Character, virtual +0x68
constexpr uint32_t releaseHookLength = 5;      // push ebx/esi/edi; mov edi,ecx
constexpr uintptr_t effectManager = 0x9e966c;

void (__fastcall* originalReleaseEffects)(void* character, void* unused);

// The code this fix relies on, as in the Steam version (PE time stamp 683d42eb).
bool followDetachCodeMatches() {
    const unsigned char release[] = {0x53,0x56,0x57,0x8b,0xf9,0x8b,0x87,0x84,0x00,0x00,0x00};
    const unsigned char detach[] = {0x85,0xdb,0x78,0x42,0x56,0x57,0x83,0xc2,0x1c,0xbe,0x5e,0x01,0x00,0x00};
    const unsigned char joint[] = {0x8b,0x74,0xbe,0x48};  // mov esi,[esi+edi*4+0x48]
    const unsigned char manager[] = {0x8b,0x15};          // mov edx,[0x9e966c]
    return same(releaseEffects,release,sizeof(release)) && same(0x4a8ec0,detach,sizeof(detach)) &&
        same(0x56590e,joint,sizeof(joint)) && same(0x48d54e,manager,sizeof(manager)) &&
        readGamePointer(0x48d550)==va(effectManager);
}

void __fastcall releaseEffectsHook(void* character, void* unused) {
    originalReleaseEffects(character,unused);
    auto manager=readGamePointer(effectManager);
    auto model=*reinterpret_cast<const uintptr_t*>(reinterpret_cast<uintptr_t>(character)+0x18);
    auto detached=bug_fix::detachJointFollowers(manager,model);
    if(detached&&host->log) {
        char line[96];
        sprintf_s(line,"detached %u effect(s) from the joints of a released model",detached);
        log(line);
    }
}

// ---- Property return after Madara's rescue

constexpr uintptr_t setNextScript = 0x55e140;       // Script command Parameter.SetNextScript
constexpr uint32_t setNextScriptHookLength = 5;    // push ebp; mov ebp,esp; push -1
constexpr uintptr_t scriptState = 0x9eb5f8;         // Holds the next script name (0x55e19e)
constexpr uintptr_t gameState = 0x9e967c;           // Game state with the taken-property flag
constexpr uintptr_t givePlayerProperty = 0x553f60;  // Body of Character.PlayerGiveProperty (0x554e40)

bool (__cdecl* originalSetNextScript)(void* script);

bool propertyReturnCodeMatches() {
    const unsigned char prologue[] = {0x55,0x8b,0xec,0x6a,0xff};
    // mov eax,[0x9eb5f8]; add eax,0x2c; push 0x80
    const unsigned char copyName[] = {0xa1,0,0,0,0,0x83,0xc0,0x2c,0x68,0x80,0x00,0x00,0x00};
    const unsigned char give[] = {0x55,0x8b,0xec,0x83,0xe4,0xf8,0x81,0xec,0xac,0x00,0x00,0x00};
    const unsigned char setTaken[] = {0xc6,0x81,0x9e,0x8b,0x04,0x00,0x01};    // mov byte [ecx+0x48b9e],1
    const unsigned char clearTaken[] = {0xc6,0x82,0x9e,0x8b,0x04,0x00,0x00};  // mov byte [edx+0x48b9e],0
    return same(setNextScript,prologue,sizeof(prologue)) && same(0x55e19e,copyName,1) &&
        same(0x55e1a3,copyName+5,sizeof(copyName)-5) && readGamePointer(0x55e19f)==va(scriptState) &&
        same(givePlayerProperty,give,sizeof(give)) && readGamePointer(0x553f7c)==va(gameState) &&
        same(0x553f50,setTaken,sizeof(setTaken)) && same(0x55408e,clearTaken,sizeof(clearTaken));
}

// Runs only when a script changes scene, not every frame.
bool __cdecl setNextScriptHook(void* script) {
    bool result=originalSetNextScript(script);
    auto state=readGamePointer(scriptState);
    auto game=readGamePointer(gameState);
    if(!state||!game)
        return result;
    auto nextScript=reinterpret_cast<const char*>(state+bug_fix::nextScriptOffset);
    auto taken=reinterpret_cast<const uint8_t*>(game+bug_fix::propertyTakenOffset);
    if(!bug_fix::shouldReturnProperty(nextScript,*taken))
        return result;
    reinterpret_cast<void (__cdecl*)()>(va(givePlayerProperty))();
    if(*taken)
        log("tried to return the player's weapons, items and outfit after Madara's rescue, "
            "but the game found no player character; they stay taken");
    else
        log("returned the player's weapons, items and outfit after Madara's rescue (BR_14_00_S210)");
    return result;
}

// ---- Installation

struct Fix {
    const char* name;
    uintptr_t target;
    uint32_t hookLength;
    void* detour;
    void** original;
    bool (*codeMatches)();
    bool selected;
};

Fix fixes[] = {
    {"follow-effect detach",releaseEffects,releaseHookLength,reinterpret_cast<void*>(&releaseEffectsHook),
        reinterpret_cast<void**>(&originalReleaseEffects),followDetachCodeMatches,false},
    {"property return after Madara's rescue",setNextScript,setNextScriptHookLength,
        reinterpret_cast<void*>(&setNextScriptHook),reinterpret_cast<void**>(&originalSetNextScript),
        propertyReturnCodeMatches,false},
};

// Checks the code and reserves the hook range. Logs why a fix is skipped.
bool select(Fix& fix) {
    char line[160];
    if(!fix.codeMatches()) {
        sprintf_s(line,"%s skipped: the game code at 0x%x is not the known version",fix.name,
            static_cast<unsigned>(fix.target));
        log(line);
        return false;
    }
    if(!host->claim(self,va(fix.target),fix.hookLength)) {
        sprintf_s(line,"%s skipped: 0x%x is used by another Mod",fix.name,static_cast<unsigned>(fix.target));
        log(line);
        return false;
    }
    return true;
}

// Creates and enables every selected hook, or none of them.
bool hookSelected() {
    if(MH_Initialize()!=MH_OK) {
        log("MinHook failed to initialize; nothing changed");
        return false;
    }
    for(auto& fix:fixes) {
        if(!fix.selected)
            continue;
        if(MH_CreateHook(reinterpret_cast<void*>(va(fix.target)),fix.detour,fix.original)!=MH_OK) {
            char line[160];
            sprintf_s(line,"failed to create the hook at 0x%x for %s; nothing changed",
                static_cast<unsigned>(fix.target),fix.name);
            log(line);
            MH_Uninitialize();
            return false;
        }
    }
    if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK) {
        log("failed to enable the hooks; nothing changed");
        MH_Uninitialize();
        return false;
    }
    return true;
}

bool installed;

}  // namespace

extern "C" BOOL WINAPI ModInitialize(const wots4::ModContext* context) {
    if(!context||context->size<sizeof(*context)||context->version!=wots4::modApiVersion||!context->host||
        context->host->size<sizeof(wots4::HostApi)||!context->host->claim||!context->host->releaseClaims)
        return FALSE;
    if(installed)
        return TRUE;
    host=context->host;
    self=context->module;
    gameBase=reinterpret_cast<uintptr_t>(context->game);
    bool anySelected=false;
    for(auto& fix:fixes)
        anySelected|=fix.selected=select(fix);
    if(!anySelected) {
        log("no fix can be installed; nothing changed");
        return FALSE;
    }
    if(!hookSelected()) {
        host->releaseClaims(self);
        for(auto& fix:fixes)
            fix.selected=false;
        return FALSE;
    }
    installed=true;
    for(auto& fix:fixes) {
        if(!fix.selected)
            continue;
        char line[160];
        sprintf_s(line,"%s installed at 0x%x",fix.name,static_cast<unsigned>(fix.target));
        log(line);
    }
    return TRUE;
}
