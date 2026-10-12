// Installs mod.dll on a copy of the exe mapped into this process and runs the
// game's own 3D listener update (0x6a2660) with a fake DirectSound that records
// what IDirectSound3DListener gets. No audio device is opened, and the game
// process and saves are never touched.
#include "mapped_exe.h"

using namespace mapped_exe;

// What the fake listener was given.
struct Recorded {
    float position[3], velocity[3], front[3], up[3];
    unsigned orientationCalls, positionCalls, velocityCalls, releases;
};
static Recorded recorded;

static HRESULT STDMETHODCALLTYPE listenerQueryInterface(void*, const IID&, void**) { return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE addReference(void*) { return 1; }
static ULONG STDMETHODCALLTYPE release(void*) { ++recorded.releases; return 0; }
static HRESULT STDMETHODCALLTYPE setOrientation(void*, float frontX, float frontY, float frontZ,
    float upX, float upY, float upZ, DWORD) {
    ++recorded.orientationCalls;
    recorded.front[0]=frontX; recorded.front[1]=frontY; recorded.front[2]=frontZ;
    recorded.up[0]=upX; recorded.up[1]=upY; recorded.up[2]=upZ;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE setPosition(void*, float x, float y, float z, DWORD) {
    ++recorded.positionCalls;
    recorded.position[0]=x; recorded.position[1]=y; recorded.position[2]=z;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE setVelocity(void*, float x, float y, float z, DWORD) {
    ++recorded.velocityCalls;
    recorded.velocity[0]=x; recorded.velocity[1]=y; recorded.velocity[2]=z;
    return S_OK;
}

// IDirectSound3DListener: SetOrientation +0x34, SetPosition +0x38, SetVelocity +0x40.
static void* listenerTable[18];
static void* listenerObject[]={listenerTable};

// The primary buffer: QueryInterface (+0) gives the listener, Release (+8).
static HRESULT STDMETHODCALLTYPE bufferQueryInterface(void*, const IID&, void** out) {
    *out=listenerObject;
    return S_OK;
}
static void* bufferTable[]={(void*)bufferQueryInterface,(void*)addReference,(void*)release};
static void* bufferObject[]={bufferTable};

// IDirectSound: CreateSoundBuffer (+0xc) gives the primary buffer (0x6a3780).
static HRESULT STDMETHODCALLTYPE createSoundBuffer(void*, const void*, void** out, void*) {
    *out=bufferObject;
    return S_OK;
}
static void* soundTable[]={nullptr,(void*)addReference,(void*)release,(void*)createSoundBuffer};
static void* soundObject[]={soundTable};

// AudioLib (the first argument): [0] points at the holder of the IDirectSound pointer; +5 is set
// when the listener changed.
static void* soundHolder[]={soundObject};
static unsigned char audioLib[16];

struct Registers { unsigned eax, ebx, edi; };

// Calls 0x6a2660 as 0x59ff28 does: eax position, ebx front, edi up, (AudioLib, velocity) on the
// stack. Returns eax, ebx and edi after the call.
static Registers setListener(const float* position, const float* front, const float* up, const float* velocity) {
    auto function=va(0x6a2660);
    auto lib=audioLib;
    Registers after{};
    __asm {
        push ebx
        push edi
        push velocity
        push lib
        mov eax, position
        mov ebx, front
        mov edi, up
        call function
        mov after.eax, eax
        mov after.ebx, ebx
        mov after.edi, edi
        pop edi
        pop ebx
    }
    return after;
}

static bool equals(const float* actual, float x, float y, float z) {
    return actual[0]==x&&actual[1]==y&&actual[2]==z;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    listenerTable[0]=(void*)listenerQueryInterface;
    listenerTable[1]=(void*)addReference;
    listenerTable[2]=(void*)release;
    listenerTable[0x34/4]=(void*)setOrientation;
    listenerTable[0x38/4]=(void*)setPosition;
    listenerTable[0x40/4]=(void*)setVelocity;
    *(void**)audioLib=soundHolder;

    // A camera looking along +X and slightly down, with its up axis.
    const float look[]={0.96f,-0.25f,0.0f}, up[]={0.25f,0.96f,0.0f};
    const float position[]={-9.5f,38.5f,-99.5f}, velocity[]={1.0f,0.0f,-2.0f};

    auto unmodded=setListener(position,look,up,velocity);
    check("without the Mod the listener gets the look axis negated and the up axis as is (the bug)",
        recorded.orientationCalls==1&&equals(recorded.front,-0.96f,0.25f,-0.0f)&&equals(recorded.up,0.25f,0.96f,0.0f));
    check("without the Mod the call succeeds (eax 0)",unmodded.eax==0);

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }

    // Only the follow-effect detach code differs: the listener orientation is still installed.
    auto joint=(unsigned char*)va(0x56590e);
    joint[3]=0x4c;
    check("with the follow-effect detach code changed, the Mod still initializes",initialize(&context)!=FALSE);
    check("0x6a2660 is claimed (5 bytes) and 0x48d520 is not",claimed(0x6a2660,5)&&!claimed(0x48d520,5));
    check("the fix is logged as installed",loggedText("listener orientation installed at 0x6a2660"));
    check("the hook is written at 0x6a2660",*(unsigned char*)va(0x6a2660)==0xe9);
    joint[3]=0x48;

    // Different values than before, so the game's cache of the last listener does not skip the calls.
    const float position2[]={10.0f,20.0f,30.0f}, velocity2[]={0.5f,0.0f,0.25f};
    volatile unsigned espBefore=0, espAfter=0;
    __asm mov espBefore, esp
    auto modded=setListener(position2,look,up,velocity2);
    __asm mov espAfter, esp
    check("with the Mod the listener faces the look axis",recorded.orientationCalls==2&&equals(recorded.front,0.96f,-0.25f,0.0f));
    check("with the Mod the up axis is negated, which keeps the right axis",equals(recorded.up,-0.25f,-0.96f,-0.0f));
    check("the position is passed on unchanged",recorded.positionCalls==2&&equals(recorded.position,10.0f,20.0f,30.0f));
    check("the velocity is passed on unchanged",recorded.velocityCalls==2&&equals(recorded.velocity,0.5f,0.0f,0.25f));
    check("the call succeeds (eax 0) and the primary buffer and listener are released",
        modded.eax==0&&recorded.releases==4);
    check("the caller's ebx and edi are given back",modded.ebx==(unsigned)look&&modded.edi==(unsigned)up);
    check("the stack is balanced (ret 8)",espBefore==espAfter);
    check("the camera axes the game passed are not changed",
        equals(look,0.96f,-0.25f,0.0f)&&equals(up,0.25f,0.96f,0.0f));
    check("AudioLib is marked as changed (+5), as the game does",audioLib[5]==1);

    printf("RESULT tests=%u failures=%u; mapped exe copy and fake DirectSound in this process only\n",tests,failures);
    return failures?1:0;
}
