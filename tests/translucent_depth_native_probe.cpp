// Installs mod.dll on a copy of the exe mapped into this process, puts a fake
// IDirect3DDevice9 where the game's renderer keeps its device
// ([[[0x9e0890]+0x720]+0x2238]), and calls SetRenderState as the game does.
// The fake device records what reaches the driver. No GPU, window or real
// device is used, and the game process and saves are never touched.
//
// mod.dll keeps its state per loaded module, so each case loads its own copy
// of mod.dll (written next to this probe).
#include <string>
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr unsigned alphaBlendEnable = 27;     // D3DRS_ALPHABLENDENABLE
constexpr unsigned zWriteEnable = 14;         // D3DRS_ZWRITEENABLE
constexpr unsigned colorWriteEnable1 = 190;   // D3DRS_COLORWRITEENABLE1
constexpr unsigned independentWriteMasks = 0x4000;  // D3DPMISCCAPS_INDEPENDENTWRITEMASKS
constexpr unsigned primitiveMiscCapsOffset = 0x20;  // D3DCAPS9::PrimitiveMiscCaps
constexpr unsigned capsSize = 304;                  // sizeof(D3DCAPS9)
constexpr unsigned deviceMethodCount = 119;         // IDirect3DDevice9
constexpr unsigned getDeviceCapsSlot = 7;
constexpr unsigned setRenderStateSlot = 57;

struct StateCall { unsigned state, value; };

// A device class: its own vtable, so each case hooks a different SetRenderState.
struct FakeDevice {
    void** vtable;
    unsigned miscCaps;
    std::vector<StateCall> calls;
    unsigned unexpectedCalls = 0;
};

static long __stdcall unexpectedMethod(FakeDevice* device) { ++device->unexpectedCalls; return E_NOTIMPL; }
static long __stdcall getDeviceCaps(FakeDevice* device, unsigned char* caps) {
    memset(caps,0,capsSize);
    *(unsigned*)(caps+primitiveMiscCapsOffset)=device->miscCaps;
    return S_OK;
}
// Two device classes need different SetRenderState code to hook. The counters keep the
// linker from folding the two identical functions into one.
static unsigned callsToA, callsToB;
static __declspec(noinline) long __stdcall setRenderStateA(FakeDevice* device, unsigned state, unsigned value) {
    ++callsToA; device->calls.push_back({state,value}); return S_OK;
}
static __declspec(noinline) long __stdcall setRenderStateB(FakeDevice* device, unsigned state, unsigned value) {
    ++callsToB; device->calls.push_back({state,value}); return S_OK;
}

static void* vtableA[deviceMethodCount];
static void* vtableB[deviceMethodCount];
static void makeDeviceClass(void** vtable, void* setRenderState) {
    for(unsigned slot=0;slot<deviceMethodCount;++slot) vtable[slot]=(void*)&unexpectedMethod;
    vtable[getDeviceCapsSlot]=(void*)&getDeviceCaps;
    vtable[setRenderStateSlot]=setRenderState;
}

// The renderer's chain to the device; nullptr leaves [0x9e0890] empty (no renderer yet).
static void placeDevice(FakeDevice* device) {
    if(!device) { *(void**)va(0x9e0890)=nullptr; return; }
    static unsigned char renderer[0x800], renderState[0x2240];
    *(void**)(renderer+0x720)=renderState;
    *(void**)(renderState+0x2238)=device;
    *(void**)va(0x9e0890)=renderer;
}

// Calls through the vtable, as the game's NiDX9RenderState does.
static void setRenderState(FakeDevice& device, unsigned state, unsigned value) {
    // volatile: the compiler must not inline the fake body past the hook written at run time.
    auto function=(long (__stdcall*)(FakeDevice*,unsigned,unsigned))((void* volatile*)device.vtable)[setRenderStateSlot];
    function(&device,state,value);
}

static bool callsAre(FakeDevice& device, std::initializer_list<StateCall> expected) {
    bool same=device.calls.size()==expected.size();
    for(size_t i=0;same&&i<expected.size();++i)
        same=device.calls[i].state==expected.begin()[i].state&&device.calls[i].value==expected.begin()[i].value;
    if(!same)
        for(auto& call:device.calls)
            printf("  reached the device: state %u = 0x%x\n",call.state,call.value);
    device.calls.clear();
    return same;
}

static std::string probeDirectory;
static wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};

// Loads a fresh copy of mod.dll and initializes it. Returns its result, or -1 if it could not be loaded.
static int initializeCopy(const char* modPath, const char* copyName) {
    auto copy=probeDirectory+copyName;
    if(!CopyFileA(modPath,copy.c_str(),FALSE)) return -1;
    auto mod=LoadLibraryA(copy.c_str());
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    if(!initialize) return -1;
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    logged.clear();
    return initialize(&context);
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    probeDirectory=argv[0];
    probeDirectory.erase(probeDirectory.find_last_of("\\/")+1);

    // The device exists before the Mod loads.
    makeDeviceClass(vtableA,(void*)&setRenderStateA);
    FakeDevice device{vtableA,independentWriteMasks};
    placeDevice(&device);
    check("mod.dll initializes with the device present",initializeCopy(argv[2],"translucent_depth_probe_a.dll")==TRUE);
    check("the mask is logged as installed",
        loggedText("translucent depth mask installed on IDirect3DDevice9::SetRenderState"));
    check("SetRenderState of the device class is hooked",vtableA[setRenderStateSlot]==(void*)&setRenderStateA&&
        *(unsigned char*)&setRenderStateA==0xe9);
    check("installing does not call the device besides GetDeviceCaps",device.calls.empty()&&!device.unexpectedCalls);

    // A character: blended, writes Z.
    setRenderState(device,alphaBlendEnable,TRUE);
    check("blending on with Z writing (a character): render target 1 keeps all channels",
        callsAre(device,{{alphaBlendEnable,TRUE},{colorWriteEnable1,0xf}}));
    // An effect: blended, no Z writing.
    setRenderState(device,zWriteEnable,FALSE);
    check("Z writing off while blending (an effect): render target 1 gets no channels",
        callsAre(device,{{zWriteEnable,FALSE},{colorWriteEnable1,0}}));
    setRenderState(device,alphaBlendEnable,FALSE);
    check("blending off without Z writing: render target 1 gets all channels back",
        callsAre(device,{{alphaBlendEnable,FALSE},{colorWriteEnable1,0xf}}));
    setRenderState(device,zWriteEnable,TRUE);
    check("Z writing on while opaque: all channels",callsAre(device,{{zWriteEnable,TRUE},{colorWriteEnable1,0xf}}));
    setRenderState(device,4,1);  // D3DRS_SHADEMODE
    check("other states pass through unchanged",callsAre(device,{{4,1}}));

    setRenderState(device,colorWriteEnable1,0x7);
    check("a mask the game asks for while opaque is passed on",callsAre(device,{{colorWriteEnable1,0x7}}));
    setRenderState(device,alphaBlendEnable,TRUE);
    setRenderState(device,zWriteEnable,FALSE);
    check("an effect hides that mask",callsAre(device,{{alphaBlendEnable,TRUE},{colorWriteEnable1,0x7},
        {zWriteEnable,FALSE},{colorWriteEnable1,0}}));
    setRenderState(device,colorWriteEnable1,0x3);
    check("a mask asked for during an effect stays hidden",callsAre(device,{{colorWriteEnable1,0}}));
    setRenderState(device,zWriteEnable,TRUE);
    check("Z writing on restores the last mask the game asked for",
        callsAre(device,{{zWriteEnable,TRUE},{colorWriteEnable1,0x3}}));
    check("no other device method was called",!device.unexpectedCalls);

    // A GPU without separate write masks: the mask is skipped, nothing is hooked.
    makeDeviceClass(vtableB,(void*)&setRenderStateB);
    FakeDevice oldDevice{vtableB,0};
    placeDevice(&oldDevice);
    int result=initializeCopy(argv[2],"translucent_depth_probe_b.dll");
    check("without separate write masks the skip is logged with the reason",
        loggedText("translucent depth mask skipped: the GPU cannot set the write mask of each render target separately"));
    check("without separate write masks SetRenderState is not hooked",*(unsigned char*)&setRenderStateB!=0xe9);
    // The game-code fixes are already hooked by the first copy, so this copy has nothing to install.
    check("with nothing to install the copy refuses to load",result==FALSE&&loggedText("no fix can be installed"));
    setRenderState(oldDevice,alphaBlendEnable,TRUE);
    check("without the hook the game's state reaches the device alone",callsAre(oldDevice,{{alphaBlendEnable,TRUE}}));

    // No renderer yet: the Mod hooks IDirect3D9::CreateDevice of the system d3d9.dll and waits.
    placeDevice(nullptr);
    result=initializeCopy(argv[2],"translucent_depth_probe_c.dll");
    check("without a device the Mod waits for its creation",
        result==TRUE&&loggedText("translucent depth mask waits for the game to create its Direct3D device"));

    printf("RESULT tests=%u failures=%u; mapped exe copy and fake devices in this process only\n",tests,failures);
    return failures?1:0;
}
