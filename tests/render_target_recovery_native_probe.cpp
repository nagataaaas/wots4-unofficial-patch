// Installs mod.dll on a copy of the exe mapped into this process and runs the game's
// BeginUsingRenderTargetGroup (0x88f300) and LostDeviceRestore (0x891f80) with a synthetic renderer,
// render target groups whose buffers cannot be set, and a fake device. The game's Recreate
// (0x890e20) is replaced on the copy by a stub that counts calls. No GPU, the game is not started.
#include <d3d9.h>
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr unsigned beginRenderTargetGroup = 0x88f300;
constexpr unsigned restoreLostDevice = 0x891f80;
constexpr unsigned recreateRenderer = 0x890e20;
constexpr unsigned twoDBufferDataType = 0x9e2d70;  // Compared at 0x88f370
constexpr unsigned rendererDeviceOffset = 0x124;
constexpr unsigned rendererDeviceLostOffset = 0x570;
constexpr unsigned rendererDefaultGroupOffset = 0x6f8;
constexpr unsigned rendererBeginGroupSlot = 0x178/4;
constexpr unsigned clearAll = 7;

// ---- Fakes

static unsigned setRenderTargetCalls;
static void* __fastcall bufferTypePointer(void*) { return (void*)va(twoDBufferDataType); }
static bool __fastcall setRenderTarget(void*, void*, void*, unsigned) { ++setRenderTargetCalls; return false; }
static void* bufferVtable[8];
static struct { void** vtable; } bufferData={bufferVtable};

static unsigned __fastcall bufferCount(void*) { return 1; }
static void* __fastcall bufferAt(void*, void*, unsigned) { return &bufferData; }
static void* groupVtable[0x84/4];
static struct { void** vtable; } sceneGroup={groupVtable}, defaultGroup={groupVtable};

static HRESULT cooperativeLevel;
static HRESULT __stdcall testCooperativeLevel(void*) { return cooperativeLevel; }
static void* deviceVtable[4];
static struct { void** vtable; } device={deviceVtable};

static void* rendererVtable[0x180/4];
alignas(4) static unsigned char renderer[0x1000];

// mov eax,[recreateResult]; inc dword [recreateCalls]; ret
static unsigned recreateResult, recreateCalls;
static void writeRecreateStub() {
    unsigned char stub[]={0xa1,0,0,0,0,0xff,0x05,0,0,0,0,0xc3};
    *(unsigned*)(stub+1)=(unsigned)&recreateResult;
    *(unsigned*)(stub+7)=(unsigned)&recreateCalls;
    memcpy((void*)va(recreateRenderer),stub,sizeof(stub));
}

static void prepareFakes() {
    bufferVtable[8/4]=(void*)&bufferTypePointer;
    bufferVtable[0x1c/4]=(void*)&setRenderTarget;
    groupVtable[0x60/4]=(void*)&bufferCount;
    groupVtable[0x7c/4]=(void*)&bufferAt;
    deviceVtable[3]=(void*)&testCooperativeLevel;
    rendererVtable[rendererBeginGroupSlot]=(void*)va(beginRenderTargetGroup);
    *(void***)renderer=rendererVtable;
    *(void**)(renderer+rendererDeviceOffset)=&device;
    *(void**)(renderer+rendererDefaultGroupOffset)=&defaultGroup;
}

static bool callBeginRenderTargetGroup(void* group) {
    auto function=va(beginRenderTargetGroup);
    auto self=renderer;
    bool begun;
    __asm {
        push clearAll
        push group
        mov ecx, self
        call function
        mov begun, al
    }
    return begun;
}

static bool callRestoreLostDevice() {
    auto function=va(restoreLostDevice);
    auto self=renderer;
    bool usable;
    __asm {
        mov ecx, self
        call function
        mov usable, al
    }
    return usable;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    prepareFakes();

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }

    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x88f300 (11 bytes) and 0x891f80 (9 bytes) are claimed and hooked",
        claimed(beginRenderTargetGroup,11)&&claimed(restoreLostDevice,9)&&
        *(unsigned char*)va(beginRenderTargetGroup)==0xe9&&*(unsigned char*)va(restoreLostDevice)==0xe9);
    check("the install is logged",loggedText("render target recovery installed at 0x88f300 and 0x891f80"));
    writeRecreateStub();

    // Without the fix the default group's fallback calls itself until the stack overflows.
    logged.clear();
    bool begun=callBeginRenderTargetGroup(&defaultGroup);
    check("the default group that cannot be set returns false",!begun);
    check("its buffer is tried once: the fallback into it is stopped",setRenderTargetCalls==1);
    check("the stop is logged",loggedText("stopped the game's endless fallback"));

    setRenderTargetCalls=0;
    logged.clear();
    begun=callBeginRenderTargetGroup(&sceneGroup);
    check("a scene group falls back to the default group once, then returns false",
        !begun&&setRenderTargetCalls==2);
    check("later failures before the reset are not logged",logged.empty());

    // The next frame: the device reports no loss, so the fix resets it.
    cooperativeLevel=D3D_OK;
    recreateResult=0;
    logged.clear();
    check("a failed reset skips the frame",!callRestoreLostDevice()&&recreateCalls==1);
    check("the failed reset is logged",loggedText("the device reset failed"));
    logged.clear();
    check("the reset is tried again at the next frame, without another log line",
        !callRestoreLostDevice()&&recreateCalls==2&&logged.empty());
    recreateResult=1;
    check("a successful reset draws the frame",callRestoreLostDevice()&&recreateCalls==3);
    check("the reset is logged with the failed attempts",
        loggedText("the device was reset by this fix after 2 failed reset(s)"));
    logged.clear();
    check("afterwards a frame does not reset the device",callRestoreLostDevice()&&recreateCalls==3&&logged.empty());

    // The device is lost: the game skips the frame, and the reset waits.
    callBeginRenderTargetGroup(&defaultGroup);
    cooperativeLevel=D3DERR_DEVICELOST;
    check("while the device is lost the frame is skipped without a reset",
        !callRestoreLostDevice()&&recreateCalls==3);
    renderer[rendererDeviceLostOffset]=0;

    // The game resets the device itself: no second reset.
    cooperativeLevel=D3DERR_DEVICENOTRESET;
    logged.clear();
    check("when the game resets the device, the fix does not reset it again",
        callRestoreLostDevice()&&recreateCalls==4);
    check("the game's reset is logged",loggedText("the device was reset by the game after 0 failed reset(s)"));
    cooperativeLevel=D3D_OK;
    check("afterwards a frame does not reset the device",callRestoreLostDevice()&&recreateCalls==4);

    setRenderTargetCalls=0;
    callBeginRenderTargetGroup(&defaultGroup);
    check("after the reset the default group is tried again",setRenderTargetCalls==1);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only\n",tests,failures);
    return failures?1:0;
}
