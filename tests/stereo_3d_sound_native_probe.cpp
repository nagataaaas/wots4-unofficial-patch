// Installs mod.dll on a copy of the exe mapped into this process and runs the
// game's 3D sound play (0x6a3270) on fake Sounds whose DirectSound buffer does or
// does not give IDirectSound3DBuffer (a stereo stream has no 3D control). No audio
// device is opened, and the game process and saves are never touched.
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr HRESULT bufferLostError = 0x88780096;  // DSERR_BUFFERLOST
constexpr unsigned playingState = 6;             // Sound +0x20 after a successful Play
constexpr DWORD loopingPlay = 1;                 // DSBPLAY_LOOPING

// What the fake buffers were asked.
struct Recorded {
    unsigned queries, plays, setParameters, releases3d;
    DWORD playFlags;
};
static Recorded recorded;
static bool bufferHas3dControl;
static HRESULT playResult;

static ULONG STDMETHODCALLTYPE addReference(void*) { return 1; }
static ULONG STDMETHODCALLTYPE release3d(void*) { ++recorded.releases3d; return 0; }
static HRESULT STDMETHODCALLTYPE setAllParameters(void*, const void*, DWORD) {
    ++recorded.setParameters;
    return S_OK;
}
// IDirectSound3DBuffer: Release +8, SetAllParameters +0x30.
static void* buffer3dTable[13];
static void* buffer3dObject[]={buffer3dTable};

static HRESULT STDMETHODCALLTYPE queryInterface(void*, const IID&, void** out) {
    ++recorded.queries;
    if(!bufferHas3dControl)
        return E_NOINTERFACE;
    *out=buffer3dObject;
    return S_OK;
}
static ULONG STDMETHODCALLTYPE release(void*) { return 0; }
static HRESULT STDMETHODCALLTYPE getStatus(void*, DWORD* status) { *status=0; return S_OK; }
static HRESULT STDMETHODCALLTYPE play(void*, DWORD, DWORD, DWORD flags) {
    ++recorded.plays;
    recorded.playFlags=flags;
    return playResult;
}
static HRESULT STDMETHODCALLTYPE restore(void*) { return bufferLostError; }
// IDirectSoundBuffer: QueryInterface +0, Release +8, GetStatus +0x24, Play +0x30, Restore +0x50.
static void* bufferTable[21];
static void* bufferObject[]={bufferTable};
static void* buffers[]={bufferObject};

// AudioLib Sound: +0xc buffers, +0x18 buffer count, +0x1c flags (0: no frequency or pan reset),
// +0x20 state.
struct FakeSound { unsigned char bytes[0x40]; };
static void prepare(FakeSound& sound) {
    memset(&sound,0,sizeof(sound));
    *(void***)(sound.bytes+0xc)=buffers;
    *(unsigned*)(sound.bytes+0x18)=1;
}
static unsigned state(const FakeSound& sound) { return *(const unsigned*)(sound.bytes+0x20); }

struct Call { HRESULT result; unsigned ebx, esi, edi, espBefore, espAfter; };

// Calls 0x6a3270 as 0x6a2c6d does: (Sound, parameters, play flags) on the stack, ret 0xc.
static Call playSound3d(FakeSound& sound, DWORD playFlags) {
    auto function=va(0x6a3270);
    unsigned char parameters[0x40]={};
    auto soundPointer=sound.bytes;
    auto parametersPointer=parameters;
    Call outcome{};
    __asm {
        push ebx
        push esi
        push edi
        mov ebx, 0x11111111
        mov esi, 0x22222222
        mov edi, 0x33333333
        mov outcome.espBefore, esp
        push playFlags
        push parametersPointer
        push soundPointer
        call function
        mov outcome.espAfter, esp
        mov outcome.result, eax
        mov outcome.ebx, ebx
        mov outcome.esi, esi
        mov outcome.edi, edi
        pop edi
        pop esi
        pop ebx
    }
    return outcome;
}

static bool registersKept(const Call& call) {
    return call.ebx==0x11111111&&call.esi==0x22222222&&call.edi==0x33333333&&call.espBefore==call.espAfter;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    bufferTable[0]=(void*)queryInterface;
    bufferTable[1]=(void*)addReference;
    bufferTable[2]=(void*)release;
    bufferTable[0x24/4]=(void*)getStatus;
    bufferTable[0x30/4]=(void*)play;
    bufferTable[0x50/4]=(void*)restore;
    buffer3dTable[1]=(void*)addReference;
    buffer3dTable[2]=(void*)release3d;
    buffer3dTable[0x30/4]=(void*)setAllParameters;
    FakeSound sound;

    prepare(sound);
    bufferHas3dControl=false;
    auto unmodded=playSound3d(sound,loopingPlay);
    check("without the Mod a buffer without 3D control is not played and E_NOINTERFACE is returned (the bug)",
        unmodded.result==E_NOINTERFACE&&recorded.queries==1&&recorded.plays==0&&state(sound)==0);
    check("without the Mod the registers and stack are kept",registersKept(unmodded));

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x6a3270 is claimed (6 bytes)",claimed(0x6a3270,6));
    check("the fix is logged as installed",loggedText("stereo 3D sound fallback installed at 0x6a3270"));
    check("the hook is written at 0x6a3270",*(unsigned char*)va(0x6a3270)==0xe9);

    recorded={};
    prepare(sound);
    auto stereo=playSound3d(sound,loopingPlay);
    check("with the Mod a buffer without 3D control is played in 2D once, with the play flags",
        recorded.queries==1&&recorded.plays==1&&recorded.playFlags==loopingPlay);
    check("the Sound is marked as playing (+0x20 = 6) and S_OK is returned",
        stereo.result==S_OK&&state(sound)==playingState);
    check("the registers and stack are kept (ret 0xc)",registersKept(stereo));
    check("the first fallback is logged",loggedText("stereo 3D sound fallback: a sound without 3D control was played in 2D"));

    logged.clear();
    recorded={};
    prepare(sound);
    auto again=playSound3d(sound,0);
    check("a later fallback plays too and is not logged",
        again.result==S_OK&&recorded.plays==1&&recorded.playFlags==0&&logged.empty());

    recorded={};
    prepare(sound);
    bufferHas3dControl=true;
    auto mono=playSound3d(sound,0);
    check("a buffer with 3D control is still played in 3D",
        mono.result==S_OK&&recorded.setParameters==1&&recorded.plays==1&&recorded.releases3d==1&&
        state(sound)==playingState);
    check("the 3D play keeps the registers and stack",registersKept(mono));

    recorded={};
    prepare(sound);
    // A failed SetAllParameters would reach the game's printf, which the mapped copy does not import.
    playResult=E_OUTOFMEMORY;
    auto failed=playSound3d(sound,0);
    check("another 3D failure is passed on without a 2D play",
        failed.result==E_OUTOFMEMORY&&recorded.plays==1&&state(sound)==0);

    printf("RESULT tests=%u failures=%u; mapped exe copy and fake DirectSound in this process only\n",tests,failures);
    return failures?1:0;
}
