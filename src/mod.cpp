// bug_fix: fixes crashes and bugs of Way of the Samurai 4 in memory only.
//
// Follow-effect detach: after a character's effects are detached (0x48d520,
// virtual +0x68, called before the model at +0x18 is freed or replaced), also
// detach the effects that follow that model's joints. Prevents the read of a
// freed joint at 0x4a4cf8, where the ending after resuming at the jail
// photographer crashed. Why that model is freed during the scene only after
// resuming there is not known yet (U-CF-2, needs an in-game check).
#include <wots4/mod_api.h>
#include <MinHook.h>
#include <cstdio>
#include <cstring>
#include "follow_detach.h"

namespace {

constexpr uintptr_t releaseEffects = 0x48d520;  // Character, virtual +0x68
constexpr uint32_t releaseHookLength = 5;      // push ebx/esi/edi; mov edi,ecx
constexpr uintptr_t effectManager = 0x9e966c;

uintptr_t gameBase;
HMODULE self;
const wots4::HostApi* host;
void (__fastcall* originalReleaseEffects)(void* character, void* unused);

uintptr_t va(uintptr_t address) { return gameBase+address-0x400000; }

bool same(uintptr_t address, const unsigned char* bytes, size_t count) {
    return !memcmp(reinterpret_cast<const void*>(va(address)), bytes, count);
}

// The code this fix relies on, as in the Steam version (PE time stamp 683d42eb).
bool knownGame() {
    const unsigned char release[] = {0x53,0x56,0x57,0x8b,0xf9,0x8b,0x87,0x84,0x00,0x00,0x00};
    const unsigned char detach[] = {0x85,0xdb,0x78,0x42,0x56,0x57,0x83,0xc2,0x1c,0xbe,0x5e,0x01,0x00,0x00};
    const unsigned char joint[] = {0x8b,0x74,0xbe,0x48};  // mov esi,[esi+edi*4+0x48]
    const unsigned char manager[] = {0x8b,0x15};          // mov edx,[0x9e966c]
    return same(releaseEffects,release,sizeof(release)) && same(0x4a8ec0,detach,sizeof(detach)) &&
        same(0x56590e,joint,sizeof(joint)) && same(0x48d54e,manager,sizeof(manager)) &&
        *reinterpret_cast<const uintptr_t*>(va(0x48d550))==va(effectManager);
}

void __fastcall releaseEffectsHook(void* character, void* unused) {
    originalReleaseEffects(character,unused);
    auto manager=*reinterpret_cast<const uintptr_t*>(va(effectManager));
    auto model=*reinterpret_cast<const uintptr_t*>(reinterpret_cast<uintptr_t>(character)+0x18);
    auto detached=bug_fix::detachJointFollowers(manager,model);
    if(detached&&host->log) {
        char line[96];
        sprintf_s(line,"detached %u effect(s) from the joints of a released model",detached);
        host->log(self,line);
    }
}

bool installed;

BOOL refuse(const char* reason, bool claimed) {
    if(claimed)
        host->releaseClaims(self);
    if(host->log)
        host->log(self,reason);
    return FALSE;
}

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
    if(!knownGame())
        return refuse("unknown game version; nothing changed",false);
    if(!host->claim(self,va(releaseEffects),releaseHookLength))
        return refuse("0x48d520 is used by another Mod; nothing changed",false);
    auto target=reinterpret_cast<void*>(va(releaseEffects));
    if(MH_Initialize()!=MH_OK)
        return refuse("MinHook failed to initialize; nothing changed",true);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&releaseEffectsHook),
           reinterpret_cast<void**>(&originalReleaseEffects))!=MH_OK) {
        MH_Uninitialize();
        return refuse("failed to create the hook at 0x48d520; nothing changed",true);
    }
    if(MH_EnableHook(target)!=MH_OK) {
        MH_RemoveHook(target);
        MH_Uninitialize();
        return refuse("failed to enable the hook at 0x48d520; nothing changed",true);
    }
    installed=true;
    if(host->log)
        host->log(self,"follow-effect detach installed at 0x48d520");
    return TRUE;
}
