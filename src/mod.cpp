// unofficial_patch: fixes crashes, bugs and rough spots of Way of the Samurai 4
// in memory only.
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
// Translucent depth mask: draws with alpha blending on and Z writing off
// (effects) do not write the depth texture that depth of field reads (render
// target 1), so effects no longer get a white or black box around them.
// Characters are blended too but write Z, so they keep writing it. Hooks the game's
// IDirect3DDevice9::SetRenderState, or IDirect3D9::CreateDevice to reach it
// when the device does not exist yet.
//
// Physical screen size check: the game is not DPI aware, so on a scaled
// monitor GetSystemMetrics gives it the scaled size, its "same resolution"
// check in 0x5383d0 never matches, and every return to the full screen window
// changes the display mode. While 0x5383d0 runs, the thread uses per-monitor
// DPI awareness so the check compares physical pixels.
//
// Full screen re-apply skip: on window activation, skip applying full screen
// again (0x538180) when the display mode and the window already match, so the
// window is not resized past the screen, does not show its white background,
// and the device is not reset. A really lost device is still reset by the
// renderer at the next frame.
//
// No white background: the game's window procedure (0x5384c0) answers
// WM_ERASEBKGND itself, so a repaint keeps the last frame instead of the
// window class's white brush.
//
// Night visit result reset: when a save file is loaded (0x4d9f50 with its
// "from a file" argument set), set the result of the last night visit back to
// "none", as saving does. Otherwise a save that still has a promise hands out
// the old reward and loses the promise at the first map change after loading.
// Retry after death restores the in-memory snapshot (same function, argument 0)
// and keeps the result, as the game does, so that visit's reward is handed out.
//
// Listener orientation: the 3D sound listener (0x6a2660) faces the way the
// camera looks instead of the opposite way, so sounds in view do not go to the
// back speakers of a 7.1 mix, where they are nearly silent on stereo output
// without a virtual surround such as Windows Sonic.
//
// Each fix is installed on its own, so a changed game or another Mod's hook
// on one of them does not stop the others. Each can be turned off in
// config.ini ([<section>] enabled=0).
#include <wots4/mod_api.h>
#include <MinHook.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include "follow_detach.h"
#include "property_return.h"
#include "translucent_depth.h"
#include "full_screen_reapply.h"
#include "yobai_result.h"
#include "listener_orientation.h"

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
    auto detached=unofficial_patch::detachJointFollowers(manager,model);
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
    auto nextScript=reinterpret_cast<const char*>(state+unofficial_patch::nextScriptOffset);
    auto taken=reinterpret_cast<const uint8_t*>(game+unofficial_patch::propertyTakenOffset);
    if(!unofficial_patch::shouldReturnProperty(nextScript,*taken))
        return result;
    reinterpret_cast<void (__cdecl*)()>(va(givePlayerProperty))();
    if(*taken)
        log("tried to return the player's weapons, items and outfit after Madara's rescue, "
            "but the game found no player character; they stay taken");
    else
        log("returned the player's weapons, items and outfit after Madara's rescue (BR_14_00_S210)");
    return result;
}

// ---- Translucent depth mask

constexpr uintptr_t rendererInstance = 0x9e0890;  // NiRenderer::ms_pkRenderer, set by the constructor (0x6e6466)
constexpr uint32_t rendererRenderState = 0x720;   // NiDX9Renderer -> NiDX9RenderState (0x88e0f0)
constexpr uint32_t renderStateDevice = 0x2238;    // NiDX9RenderState -> IDirect3DDevice9 (0x88e109)
constexpr size_t createDeviceSlot = 16;           // IDirect3D9::CreateDevice in the vtable
constexpr size_t setRenderStateSlot = 57;         // IDirect3DDevice9::SetRenderState in the vtable

using SetRenderStateFunction = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice9*, D3DRENDERSTATETYPE, DWORD);
using CreateDeviceFunction = HRESULT (STDMETHODCALLTYPE*)(IDirect3D9*, UINT, D3DDEVTYPE, HWND, DWORD,
    D3DPRESENT_PARAMETERS*, IDirect3DDevice9**);

unofficial_patch::TranslucentDepthMask depthMask;
SetRenderStateFunction originalSetRenderState;
CreateDeviceFunction originalCreateDevice;
bool setRenderStateHooked;

bool deviceCodeMatches() {
    const unsigned char setInstance[] = {0x89,0x35};               // mov [0x9e0890],esi
    const unsigned char renderState[] = {0x8b,0x81,0x20,0x07,0x00,0x00};  // mov eax,[ecx+0x720]
    const unsigned char device[] = {0x8b,0x80,0x38,0x22,0x00,0x00};       // mov eax,[eax+0x2238]
    return same(0x6e6466,setInstance,sizeof(setInstance)) && readGamePointer(0x6e6468)==va(rendererInstance) &&
        same(0x88e0f0,renderState,sizeof(renderState)) && same(0x88e109,device,sizeof(device));
}

// The game's device, or nullptr while the renderer has not created it.
IDirect3DDevice9* gameDevice() {
    auto renderer=readGamePointer(rendererInstance);
    if(!renderer)
        return nullptr;
    auto renderState=*reinterpret_cast<const uintptr_t*>(renderer+rendererRenderState);
    if(!renderState)
        return nullptr;
    return *reinterpret_cast<IDirect3DDevice9* const*>(renderState+renderStateDevice);
}

void* vtableEntry(void* object, size_t slot) { return (*static_cast<void***>(object))[slot]; }

// Runs for every render state change of every draw, so it only compares and forwards.
HRESULT STDMETHODCALLTYPE setRenderStateHook(IDirect3DDevice9* device, D3DRENDERSTATETYPE state, DWORD value) {
    if(state==D3DRS_COLORWRITEENABLE1) {
        depthMask.requested=value;
        return originalSetRenderState(device,state,depthMask.effective());
    }
    HRESULT result=originalSetRenderState(device,state,value);
    if(FAILED(result))
        return result;
    if(state==D3DRS_ALPHABLENDENABLE)
        depthMask.blending=value!=FALSE;
    else if(state==D3DRS_ZWRITEENABLE)
        depthMask.zWriting=value!=FALSE;
    else
        return result;
    originalSetRenderState(device,D3DRS_COLORWRITEENABLE1,depthMask.effective());
    return result;
}

// Hooks SetRenderState of device's class. Returns why it could not, or nullptr.
// When the device already exists, the mask takes effect at the next change of blending or Z writing.
const char* hookSetRenderState(IDirect3DDevice9* device) {
    D3DCAPS9 caps;
    if(FAILED(device->GetDeviceCaps(&caps)))
        return "the device capabilities could not be read";
    if(!(caps.PrimitiveMiscCaps&D3DPMISCCAPS_INDEPENDENTWRITEMASKS))
        return "the GPU cannot set the write mask of each render target separately";
    auto target=vtableEntry(device,setRenderStateSlot);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&setRenderStateHook),
        reinterpret_cast<void**>(&originalSetRenderState))!=MH_OK)
        return "IDirect3DDevice9::SetRenderState could not be hooked";
    if(MH_EnableHook(target)!=MH_OK) {
        MH_RemoveHook(target);
        return "the hook of IDirect3DDevice9::SetRenderState could not be enabled";
    }
    setRenderStateHooked=true;
    return nullptr;
}

void logDepthMaskResult(const char* failure) {
    char line[200];
    if(failure)
        sprintf_s(line,"translucent depth mask skipped: %s; effects may keep a box around them",failure);
    else
        sprintf_s(line,"translucent depth mask installed on IDirect3DDevice9::SetRenderState");
    log(line);
}

// Runs once when the game creates its device.
HRESULT STDMETHODCALLTYPE createDeviceHook(IDirect3D9* direct3d, UINT adapter, D3DDEVTYPE type, HWND window,
    DWORD flags, D3DPRESENT_PARAMETERS* parameters, IDirect3DDevice9** device) {
    HRESULT result=originalCreateDevice(direct3d,adapter,type,window,flags,parameters,device);
    if(FAILED(result)||!device||!*device||setRenderStateHooked)
        return result;
    logDepthMaskResult(hookSetRenderState(*device));
    return result;
}

// Hooks CreateDevice of the IDirect3D9 class the game will use. Returns why it could not, or nullptr.
const char* hookCreateDevice() {
    // Same name as the game's LoadLibraryA("D3D9.DLL") (0x88bc1c), so a d3d9.dll wrapper in the game
    // folder is found first here too.
    auto d3d9=LoadLibraryW(L"d3d9.dll");
    if(!d3d9)
        return "d3d9.dll could not be loaded";
    auto create=reinterpret_cast<IDirect3D9* (WINAPI*)(UINT)>(GetProcAddress(d3d9,"Direct3DCreate9"));
    if(!create)
        return "d3d9.dll has no Direct3DCreate9";
    auto direct3d=create(D3D_SDK_VERSION);
    if(!direct3d)
        return "Direct3DCreate9 failed";
    auto target=vtableEntry(direct3d,createDeviceSlot);
    direct3d->Release();
    if(MH_CreateHook(target,reinterpret_cast<void*>(&createDeviceHook),
        reinterpret_cast<void**>(&originalCreateDevice))!=MH_OK)
        return "IDirect3D9::CreateDevice could not be hooked";
    if(MH_EnableHook(target)!=MH_OK) {
        MH_RemoveHook(target);
        return "the hook of IDirect3D9::CreateDevice could not be enabled";
    }
    return nullptr;
}

// Hooks the existing device now, or its creation. Needs MinHook initialized.
bool installTranslucentDepthMask() {
    if(!deviceCodeMatches()) {
        log("translucent depth mask skipped: the game code at 0x88e0f0 is not the known version");
        return false;
    }
    if(auto device=gameDevice()) {
        auto failure=hookSetRenderState(device);
        logDepthMaskResult(failure);
        return !failure;
    }
    if(auto failure=hookCreateDevice()) {
        logDepthMaskResult(failure);
        return false;
    }
    log("translucent depth mask waits for the game to create its Direct3D device");
    return true;
}

// ---- Physical screen size check

// Changes the display mode to (ebx = width, height) unless the screen already has that size.
// Called by the full screen setup (0x538180) on every WM_ACTIVATEAPP and by 0x4ddc70.
constexpr uintptr_t displayModeCheck = 0x5383d0;
constexpr uint32_t displayModeCheckHookLength = 9;  // push ebp; mov ebp,esp; sub esp,0xa4
constexpr uintptr_t getSystemMetricsImport = 0x905404;
// DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE; the SDK defines it only for WINVER >= 0x0605.
const HANDLE perMonitorDpiAwareness = reinterpret_cast<HANDLE>(-3);

using SetThreadDpiAwarenessContextFunction = HANDLE (WINAPI*)(HANDLE);

// Resolved at runtime: missing before Windows 10 version 1607, where the hook changes nothing.
SetThreadDpiAwarenessContextFunction setThreadDpiAwarenessContext;
HANDLE dpiAwarenessBeforeCheck;
void* originalDisplayModeCheck;

bool displayModeCheckCodeMatches() {
    const unsigned char prologue[] = {0x55,0x8b,0xec,0x81,0xec,0xa4,0x00,0x00,0x00};
    const unsigned char loadMetrics[] = {0x8b,0x35};  // mov esi,[GetSystemMetrics]
    // cmp ebx,edi; jne; cmp [ebp+0xc],eax; je (the screen size check)
    const unsigned char compare[] = {0x3b,0xdf,0x75,0x05,0x39,0x45,0x0c,0x74,0x7f};
    return same(displayModeCheck,prologue,sizeof(prologue)) && same(0x5383e7,loadMetrics,sizeof(loadMetrics)) &&
        readGamePointer(0x5383e9)==va(getSystemMetricsImport) && same(0x5383fe,compare,sizeof(compare));
}

// Runs once or twice each time the window is activated, not every frame, so it may log.
void __cdecl usePhysicalPixels(int wantedWidth, int wantedHeight) {
    if(!setThreadDpiAwarenessContext)
        return;
    dpiAwarenessBeforeCheck=setThreadDpiAwarenessContext(perMonitorDpiAwareness);
    if(!host->log)
        return;
    // Through the game's import, so the log shows what the game's check sees.
    auto getSystemMetrics=reinterpret_cast<int (WINAPI*)(int)>(readGamePointer(getSystemMetricsImport));
    char line[200];
    int screenWidth=getSystemMetrics(SM_CXSCREEN);
    int screenHeight=getSystemMetrics(SM_CYSCREEN);
    sprintf_s(line,"display mode check: the game wants %dx%d, the primary screen is %dx%d (physical pixels); %s",
        wantedWidth,wantedHeight,screenWidth,screenHeight,
        wantedWidth==screenWidth&&wantedHeight==screenHeight?"no display mode change":"the game changes the display mode");
    log(line);
}

void __cdecl restoreDpiAwareness() {
    if(!dpiAwarenessBeforeCheck)
        return;
    setThreadDpiAwarenessContext(dpiAwarenessBeforeCheck);
    dpiAwarenessBeforeCheck=nullptr;
}

// 0x5383d0 takes the width in ebx and (window, height) on the stack, returns al and pops 8 bytes.
// The C++ calls in between keep ebx, so the original gets the same arguments.
__declspec(naked) void displayModeCheckHook() {
    __asm {
        push dword ptr [esp+8]
        push ebx
        call usePhysicalPixels
        add esp, 8
        push dword ptr [esp+8]
        push dword ptr [esp+8]
        call dword ptr [originalDisplayModeCheck]
        push eax
        call restoreDpiAwareness
        pop eax
        ret 8
    }
}

void resolveSetThreadDpiAwarenessContext() {
    auto user32=GetModuleHandleW(L"user32.dll");
    if(user32)
        setThreadDpiAwarenessContext=reinterpret_cast<SetThreadDpiAwarenessContextFunction>(
            GetProcAddress(user32,"SetThreadDpiAwarenessContext"));
    if(!setThreadDpiAwarenessContext)
        log("physical screen size check does nothing: SetThreadDpiAwarenessContext is missing "
            "(Windows older than 10 version 1607)");
}

// ---- Installation

// ---- Full screen re-apply skip

// Window.cpp: applies full screen (true) or windowed mode (false). eax = the window object,
// the bool on the stack, ret 4. Called with true on every WM_ACTIVATEAPP (0x538561).
constexpr uintptr_t applyFullScreen = 0x538180;
constexpr uint32_t applyFullScreenHookLength = 7;  // push ebp; mov ebp,esp; cmp byte [ebp+8],0
constexpr uintptr_t settingsWidth = 0x9d30d4;
constexpr uintptr_t settingsHeight = 0x9d30d8;
constexpr uint32_t windowHandleOffset = 0x4;   // HWND in the window object
constexpr uint32_t fullScreenOffset = 0xec;     // bool, set by 0x538180

void* originalApplyFullScreen;

bool applyFullScreenCodeMatches() {
    const unsigned char prologue[] = {0x55,0x8b,0xec,0x80,0x7d,0x08,0x00,0x53,0x56,0x57,0x8b,0xf0};
    const unsigned char loadSize[] = {0xa1,0,0,0,0,0x8b,0x1d};               // mov eax,[height]; mov ebx,[width]
    const unsigned char setFullScreen[] = {0xc6,0x86,0xec,0x00,0x00,0x00,0x01};  // mov byte [esi+0xec],1
    const unsigned char hwndUse[] = {0x8b,0x56,0x04};                           // mov edx,[esi+4] (SetWindowLongA)
    return same(applyFullScreen,prologue,sizeof(prologue)) && same(0x53818e,loadSize,1) &&
        same(0x538193,loadSize+5,2) && readGamePointer(0x53818f)==va(settingsHeight) &&
        readGamePointer(0x538195)==va(settingsWidth) && same(0x53819b,setFullScreen,sizeof(setFullScreen)) &&
        same(0x5381c9,hwndUse,sizeof(hwndUse));
}

// The primary screen in physical pixels, or 0x0 without per-thread DPI awareness.
void physicalScreenSize(int& width, int& height) {
    width=height=0;
    if(!setThreadDpiAwarenessContext)
        return;
    auto getSystemMetrics=reinterpret_cast<int (WINAPI*)(int)>(readGamePointer(getSystemMetricsImport));
    HANDLE previous=setThreadDpiAwarenessContext(perMonitorDpiAwareness);
    width=getSystemMetrics(SM_CXSCREEN);
    height=getSystemMetrics(SM_CYSCREEN);
    setThreadDpiAwarenessContext(previous);
}

unofficial_patch::FullScreenState readFullScreenState(uintptr_t window, bool requestedFullScreen) {
    unofficial_patch::FullScreenState state{};
    state.requestedFullScreen=requestedFullScreen;
    state.alreadyFullScreen=*reinterpret_cast<const uint8_t*>(window+fullScreenOffset)!=0;
    state.wantedWidth=static_cast<int>(readGamePointer(settingsWidth));
    state.wantedHeight=static_cast<int>(readGamePointer(settingsHeight));
    physicalScreenSize(state.physicalWidth,state.physicalHeight);
    auto handle=*reinterpret_cast<const HWND*>(window+windowHandleOffset);
    state.minimized=IsIconic(handle)!=FALSE;
    LONG style=GetWindowLongW(handle,GWL_STYLE);
    state.popupStyle=(style&WS_POPUP)&&!(style&(WS_CAPTION|WS_THICKFRAME));
    RECT rect{};
    GetWindowRect(handle,&rect);
    state.windowLeft=rect.left; state.windowTop=rect.top; state.windowRight=rect.right; state.windowBottom=rect.bottom;
    // In the thread's current DPI awareness, the window's own, like GetWindowRect.
    auto getSystemMetrics=reinterpret_cast<int (WINAPI*)(int)>(readGamePointer(getSystemMetricsImport));
    state.screenWidth=getSystemMetrics(SM_CXSCREEN);
    state.screenHeight=getSystemMetrics(SM_CYSCREEN);
    return state;
}

// Runs when the window is activated or the display mode is switched, not every frame, so it may log.
bool __cdecl skipFullScreenReapply(uintptr_t window, int requestedFullScreen) {
    auto state=readFullScreenState(window,(requestedFullScreen&0xff)!=0);
    if(!state.requestedFullScreen)
        return false;  // Leaving full screen is not a re-apply; nothing to log
    auto reason=unofficial_patch::fullScreenReapplyReason(state);
    char line[220];
    if(reason)
        sprintf_s(line,"full screen applied again: %s",reason);
    else
        sprintf_s(line,"full screen re-apply skipped: the display is already %dx%d and the window covers it",
            state.physicalWidth,state.physicalHeight);
    log(line);
    return reason==nullptr;
}

// 0x538180 takes the window object in eax. When the call is not skipped, jump to the original
// with the caller's stack and eax unchanged, so it returns to the caller itself.
__declspec(naked) void applyFullScreenHook() {
    __asm {
        push eax
        push dword ptr [esp+8]
        push eax
        call skipFullScreenReapply
        add esp, 8
        test al, al
        pop eax
        jnz skip
        jmp dword ptr [originalApplyFullScreen]
    skip:
        ret 4
    }
}

// ---- No white background

// The window procedure registered for the game's window (0x538896). The class brush is WHITE_BRUSH
// (0x5388cf) and WM_ERASEBKGND goes to DefWindowProcA, so any repaint before the next frame is white.
constexpr uintptr_t windowProcedure = 0x5384c0;
constexpr uint32_t windowProcedureHookLength = 6;  // push ebp; mov ebp,esp; push ecx; xor eax,eax
constexpr uintptr_t windowObject = 0x9ecf90;
constexpr uintptr_t getStockObjectImport = 0x905010;

WNDPROC originalWindowProcedure;

bool windowProcedureCodeMatches() {
    const unsigned char prologue[] = {0x55,0x8b,0xec,0x51,0x33,0xc0,0x39,0x05};  // ...; cmp [0x9ecf90],eax
    const unsigned char registerProcedure[] = {0xc7,0x45,0xd4};                // mov [ebp-0x2c],0x5384c0
    const unsigned char whiteBrushIndex[] = {0x6a,0x00};  // push 0 (WHITE_BRUSH)
    const unsigned char getStockObject[] = {0xff,0x15};   // call [GetStockObject]
    return same(windowProcedure,prologue,sizeof(prologue)) && readGamePointer(0x5384c8)==va(windowObject) &&
        same(0x538896,registerProcedure,sizeof(registerProcedure)) &&
        readGamePointer(0x538899)==va(windowProcedure) && same(0x5388ca,whiteBrushIndex,sizeof(whiteBrushIndex)) &&
        same(0x5388cf,getStockObject,sizeof(getStockObject)) && readGamePointer(0x5388d1)==va(getStockObjectImport);
}

// Runs for every message of the game's window, so it only compares and forwards.
// Leaving the background unpainted keeps the last frame on the screen until the game presents again.
LRESULT CALLBACK windowProcedureHook(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    if(message==WM_ERASEBKGND)
        return 1;
    return originalWindowProcedure(window,message,wParam,lParam);
}

// ---- Night visit result reset

// Copies save data into the game state (SaveDataWin32Manager.cpp). __stdcall (game state, save data,
// from a file), ret 0xc; reads no register arguments. Loading a save file calls it with 1 (0x53eae9,
// the data read into [0x9ecf9c]); retry after death with 0 (0x4e91b6, the snapshot [0x9ecfa8] taken
// at the last map exit by 0x53e3e0). Not called every frame.
constexpr uintptr_t loadSaveData = 0x4d9f50;
constexpr uint32_t loadSaveDataHookLength = 5;  // push ebp; mov ebp,esp; push ebx; push esi
constexpr uintptr_t yobaiProgress = 0x9ecbb8;
constexpr uintptr_t fileSaveData = 0x9ecf9c;
constexpr uintptr_t retrySnapshot = 0x9ecfa8;

uintptr_t (__stdcall* originalLoadSaveData)(void* gameState, const void* saveData, uint32_t fromFile);

bool yobaiResultCodeMatches() {
    const unsigned char prologue[] = {0x55,0x8b,0xec,0x53,0x56,0x57,0x8b,0x7d,0x08};
    const unsigned char readFromFile[] = {0x80,0x7d,0x10,0x00};  // cmp byte [ebp+0x10],0 (file tail)
    // mov ecx,[0x9ecf9c]; mov edx,[0x9e967c]; push 1; push ecx; push edx; call 0x4d9f50
    const unsigned char loadFile[] = {0x8b,0x0d,0,0,0,0,0x8b,0x15,0,0,0,0,0x6a,0x01,0x51,0x52,0xe8};
    // push 0; push edx (the snapshot, read at 0x4e918e: mov edx,[0x9ecfa8]); push edi
    const unsigned char loadRetrySnapshot[] = {0x6a,0x00,0x52,0x57};
    const unsigned char readSnapshot[] = {0x8b,0x15};
    const unsigned char readResult[] = {0x0f,0xbe,0x47,0x09};               // movsx eax,byte [edi+9] (0x6a0560)
    const unsigned char markHandled[] = {0x66,0xc7,0x47,0x0a,0x01,0x00};    // mov word [edi+0xa],1
    // mov byte [eax+9],-1; mov byte [eax+0xa],0 (after saving)
    const unsigned char clearOnSave[] = {0xc6,0x40,0x09,0xff,0xc6,0x40,0x0a,0x00};
    return same(loadSaveData,prologue,sizeof(prologue)) && same(0x4d9f8b,readFromFile,sizeof(readFromFile)) &&
        same(0x53eadd,loadFile,2) && readGamePointer(0x53eadf)==va(fileSaveData) &&
        same(0x53eae3,loadFile+6,2) && readGamePointer(0x53eae5)==va(gameState) &&
        same(0x53eae9,loadFile+12,sizeof(loadFile)-12) &&
        static_cast<uintptr_t>(0x53eaf2+*reinterpret_cast<const int32_t*>(va(0x53eaee)))==loadSaveData &&
        same(0x4e91b6,loadRetrySnapshot,sizeof(loadRetrySnapshot)) &&
        same(0x4e918e,readSnapshot,sizeof(readSnapshot)) && readGamePointer(0x4e9190)==va(retrySnapshot) &&
        same(0x6a05f3,readResult,sizeof(readResult)) && same(0x6a06c4,markHandled,sizeof(markHandled)) &&
        same(0x53e43a,clearOnSave,sizeof(clearOnSave)) && readGamePointer(0x53e42c)==va(yobaiProgress);
}

// Runs once per load or retry, so it may log.
uintptr_t __stdcall loadSaveDataHook(void* gameState, const void* saveData, uint32_t fromFile) {
    auto returned=originalLoadSaveData(gameState,saveData,fromFile);
    // The game tests only the low byte (0x4d9f8b).
    if(!(fromFile&0xff))
        return returned;
    auto progress=reinterpret_cast<uint8_t*>(readGamePointer(yobaiProgress));
    auto discarded=unofficial_patch::discardYobaiResult(progress);
    if(discarded==unofficial_patch::noYobaiResult)
        return returned;
    char line[160];
    sprintf_s(line,"discarded the night visit result %d left in memory while loading a save file; "
        "the loaded promise is no longer used up by it",discarded);
    log(line);
    return returned;
}

// ---- Listener orientation

// AudioLib: sets the 3D listener. eax = position, ebx = front (the camera's look axis), edi = up,
// (AudioLib, velocity) on the stack, ret 8. Called once per frame by 0x59fc60 (0x59ff28).
constexpr uintptr_t setListener = 0x6a2660;
constexpr uint32_t setListenerHookLength = 5;  // push ebp; mov ebp,esp; push ecx; push esi

void* originalSetListener;
float passedFront[3];  // Read by the original before the hook returns; only the game's sound update calls it
float passedUp[3];

bool listenerOrientationCodeMatches() {
    const unsigned char prologue[] = {0x55,0x8b,0xec,0x51,0x56,0x8b,0xf0,0x8b,0x45,0x08};
    const unsigned char negate[] = {0xd9,0xe0};  // fchs on each front component
    const unsigned char setOrientation[] = {0x8b,0x51,0x34};  // mov edx,[ecx+0x34] (SetOrientation)
    // lea edi,[esp+0x1c] (up); lea ebx,[esp+0x28] (front); lea eax,[esp+0x34] (position); call
    const unsigned char caller[] = {0x8d,0x7c,0x24,0x1c,0x8d,0x5c,0x24,0x28,0x8d,0x44,0x24,0x34,0xe8};
    const unsigned char lookAxis[] = {0xd9,0x47,0x64};  // fld [edi+0x64] (camera rotation column 0)
    const unsigned char upAxis[] = {0xd9,0x47,0x68};    // fld [edi+0x68] (column 1)
    return same(setListener,prologue,sizeof(prologue)) && same(0x6a281e,negate,sizeof(negate)) &&
        same(0x6a2837,negate,sizeof(negate)) && same(0x6a2848,negate,sizeof(negate)) &&
        same(0x6a2845,setOrientation,sizeof(setOrientation)) && same(0x59ff1c,caller,sizeof(caller)) &&
        static_cast<uintptr_t>(0x59ff2d+*reinterpret_cast<const int32_t*>(va(0x59ff29)))==setListener &&
        same(0x59fe89,lookAxis,sizeof(lookAxis)) && same(0x59feb6,upAxis,sizeof(upAxis));
}

void __cdecl writePassedAxes(const float* look, const float* up) {
    unofficial_patch::correctListenerAxes(look,up,passedFront,passedUp);
}

// Calls the original with ebx and edi pointing at the corrected axes, and gives the caller its
// ebx and edi back. eax (the position) and the stack arguments are passed on unchanged.
__declspec(naked) void setListenerHook() {
    __asm {
        push ebx
        push edi
        push eax
        push edi
        push ebx
        call writePassedAxes
        add esp, 8
        pop eax
        mov ebx, offset passedFront
        mov edi, offset passedUp
        push dword ptr [esp+0x10]
        push dword ptr [esp+0x10]
        call dword ptr [originalSetListener]
        pop edi
        pop ebx
        ret 8
    }
}

// ---- Settings

wchar_t configPath[1024];  // mods\unofficial_patch\config.ini

bool findConfig(const wchar_t* modDirectory) {
    return modDirectory&&swprintf_s(configPath,L"%sconfig.ini",modDirectory)>=0;
}

// [section] enabled in config.ini. A missing file or key enables the fix, so the Mod works without the file.
bool fixEnabled(const wchar_t* section) {
    wchar_t value[16];
    GetPrivateProfileStringW(section,L"enabled",L"1",value,ARRAYSIZE(value),configPath);
    if(!wcscmp(value,L"1"))
        return true;
    char line[200];
    if(!wcscmp(value,L"0")) {
        sprintf_s(line,"[%ls] disabled in config.ini (enabled=0); not installed",section);
        log(line);
        return false;
    }
    sprintf_s(line,"config.ini: [%ls] enabled=%ls is not a valid value (use 1 or 0); using 1",section,value);
    log(line);
    return true;
}

// ---- Installation

struct Fix {
    const wchar_t* section;  // In config.ini
    const char* name;
    uintptr_t target;
    uint32_t hookLength;
    void* detour;
    void** original;
    bool (*codeMatches)();
    bool selected;
};

Fix fixes[] = {
    {L"follow_detach","follow-effect detach",releaseEffects,releaseHookLength,
        reinterpret_cast<void*>(&releaseEffectsHook),reinterpret_cast<void**>(&originalReleaseEffects),
        followDetachCodeMatches,false},
    {L"property_return","property return after Madara's rescue",setNextScript,setNextScriptHookLength,
        reinterpret_cast<void*>(&setNextScriptHook),reinterpret_cast<void**>(&originalSetNextScript),
        propertyReturnCodeMatches,false},
    {L"physical_screen_check","physical screen size check",displayModeCheck,displayModeCheckHookLength,
        reinterpret_cast<void*>(&displayModeCheckHook),&originalDisplayModeCheck,
        displayModeCheckCodeMatches,false},
    {L"skip_full_screen_reapply","full screen re-apply skip",applyFullScreen,applyFullScreenHookLength,
        reinterpret_cast<void*>(&applyFullScreenHook),&originalApplyFullScreen,
        applyFullScreenCodeMatches,false},
    {L"no_white_background","no white background",windowProcedure,windowProcedureHookLength,
        reinterpret_cast<void*>(&windowProcedureHook),reinterpret_cast<void**>(&originalWindowProcedure),
        windowProcedureCodeMatches,false},
    {L"yobai_result_reset","night visit result reset",loadSaveData,loadSaveDataHookLength,
        reinterpret_cast<void*>(&loadSaveDataHook),reinterpret_cast<void**>(&originalLoadSaveData),
        yobaiResultCodeMatches,false},
    {L"listener_orientation","listener orientation",setListener,setListenerHookLength,
        reinterpret_cast<void*>(&setListenerHook),&originalSetListener,
        listenerOrientationCodeMatches,false},
};

// Checks the setting and the code, and reserves the hook range. Logs why a fix is skipped.
bool select(Fix& fix) {
    if(!fixEnabled(fix.section))
        return false;
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

void removeSelectedHooks() {
    for(auto& fix:fixes)
        if(fix.selected)
            MH_RemoveHook(reinterpret_cast<void*>(va(fix.target)));
}

// Creates and enables every selected hook, or none of them. Needs MinHook initialized.
bool hookSelected() {
    for(auto& fix:fixes) {
        if(!fix.selected)
            continue;
        if(MH_CreateHook(reinterpret_cast<void*>(va(fix.target)),fix.detour,fix.original)!=MH_OK) {
            char line[160];
            sprintf_s(line,"failed to create the hook at 0x%x for %s; these fixes are not installed",
                static_cast<unsigned>(fix.target),fix.name);
            log(line);
            removeSelectedHooks();
            return false;
        }
    }
    for(auto& fix:fixes) {
        if(fix.selected&&MH_EnableHook(reinterpret_cast<void*>(va(fix.target)))!=MH_OK) {
            log("failed to enable the hooks of the game code; these fixes are not installed");
            removeSelectedHooks();
            return false;
        }
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
    if(!findConfig(context->directory)) {
        log("the path of config.ini is too long; nothing changed");
        return FALSE;
    }
    resolveSetThreadDpiAwarenessContext();
    bool anySelected=false;
    for(auto& fix:fixes)
        anySelected|=fix.selected=select(fix);
    if(MH_Initialize()!=MH_OK) {
        log("MinHook failed to initialize; nothing changed");
        host->releaseClaims(self);
        return FALSE;
    }
    if(anySelected&&!hookSelected()) {
        host->releaseClaims(self);
        for(auto& fix:fixes)
            fix.selected=false;
        anySelected=false;
    }
    bool depthMaskInstalled=fixEnabled(L"translucent_depth")&&installTranslucentDepthMask();
    if(!anySelected&&!depthMaskInstalled) {
        log("no fix can be installed; nothing changed");
        MH_Uninitialize();
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
