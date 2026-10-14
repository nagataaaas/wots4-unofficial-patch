// Installs mod.dll on a copy of the exe mapped into this process and runs the
// game's own sound slot code: the stop (0x5a1210), the end notification check of
// the slot update (0x6a2480, which calls 0x5a1950) and the slot pick (0x5a19e0),
// with fake DirectSound buffers and a recording free(). No audio device is
// opened, and the game process and saves are never touched.
#include "mapped_exe.h"
#include <cstddef>

using namespace mapped_exe;

constexpr unsigned slotTable = 0x9f02c0, slotSize = 0x74, slotCount = 24;
constexpr unsigned freeImport = 0x9052c0;
constexpr unsigned nextFreeSlot = 0x9ed00c, nextHandle = 0x9ed010;
constexpr DWORD playingStatus = 1;  // DSBSTATUS_PLAYING

// What the fakes were asked to do.
static unsigned stops, destroyed;
static void* freedBlocks[8];
static unsigned freedCount;

// IDirectSoundBuffer: GetStatus (+0x24) reports whether the fake plays, Stop (+0x48) stops it.
struct FakeBuffer { void** table; bool playing; };
static HRESULT STDMETHODCALLTYPE getStatus(FakeBuffer* buffer, DWORD* status) {
    *status=buffer->playing?playingStatus:0;
    return S_OK;
}
static HRESULT STDMETHODCALLTYPE stopBuffer(FakeBuffer* buffer) {
    ++stops;
    buffer->playing=false;
    return S_OK;
}
static void* bufferTable[0x4c/4];

// AudioLib Sound: virtual destructor at [0], buffers +0xc, buffer count +0x18, state +0x20.
static void __fastcall destroySound(void*, void*, int) { ++destroyed; }
static void* soundTable[]={(void*)destroySound};
struct FakeSound {
    void** table; unsigned char head[8]; FakeBuffer** buffers; unsigned char gap[8];
    unsigned bufferCount, unused, state, flags; unsigned char tail[0x30];
};
static_assert(offsetof(FakeSound,buffers)==0xc&&offsetof(FakeSound,bufferCount)==0x18&&offsetof(FakeSound,state)==0x20,
    "AudioLib Sound layout (0x6a3370, 0x6a3400)");

static void __cdecl recordFree(void* block) {
    if(freedCount<ARRAYSIZE(freedBlocks)) freedBlocks[freedCount]=block;
    ++freedCount;
}

// One playable sound: a channel (0x20 bytes, 0x6a2420) on a Sound with one buffer.
struct Sound {
    FakeBuffer buffer{bufferTable,false};
    FakeBuffer* buffers[1]={&buffer};
    FakeSound sound{};
    unsigned char channel[0x20]{};
};
static Sound sounds[2];

static unsigned char* slot(unsigned index) { return (unsigned char*)va(slotTable+index*slotSize); }
template<typename T> static T& at(void* base, unsigned offset) { return *(T*)((unsigned char*)base+offset); }

enum Kind { sample=1, stream=2 };

// A slot playing (or paused) as 0x5a2900 and 0x5a0f30 leave it.
static void prepare(unsigned index, Sound& s, Kind kind, bool paused, bool playedLastUpdate) {
    s.buffer.playing=!paused;
    s.sound={soundTable};
    s.sound.buffers=s.buffers;
    s.sound.bufferCount=1;
    memset(s.channel,0,sizeof(s.channel));
    at<void*>(s.channel,4)=&s.sound;
    at<unsigned char>(s.channel,0xc)=paused;
    at<unsigned>(s.channel,0x14)=va(0x5a1950);  // End notification, as 0x5a10c0 sets it
    at<unsigned char>(s.channel,0x19)=playedLastUpdate;
    auto p=slot(index);
    memset(p,0,slotSize);
    at<unsigned>(p,4)=kind;
    at<unsigned>(p,0xc)=3;          // Stream state: playing
    at<void*>(p,0x14)=s.channel;
    at<void*>(p,0x18)=&s.sound;
    at<int>(p,0x28)=100+index;      // Handle
    at<unsigned char>(p,0x34)=1;    // In use
    at<unsigned char>(p,0x38)=1;    // Loop
    at<int>(p,0x4c)=6;              // Priority
    at<unsigned>(p,0x50)=2;         // SE group
    stops=destroyed=freedCount=0;
}

// The sound manager's slot manager: only its group list (+0x10, empty) is read by the fade.
static unsigned char manager[0x40];

static unsigned char stopSlot(unsigned char* p, float fade) {
    auto function=va(0x5a1210);
    auto m=manager;
    unsigned char result;
    __asm {
        push fade
        push m
        mov eax, p
        call function
        mov result, al
    }
    return result;
}

// What the slot update does first for each slot in use with a channel (0x5a0684).
static void updateNotification(unsigned char* p) {
    auto function=va(0x6a2480);
    auto channel=at<void*>(p,0x14);
    if(!channel) return;
    __asm {
        push esi
        mov esi, channel
        call function
        pop esi
    }
}

static unsigned char* acquireSlot(int priority) {
    auto function=va(0x5a19e0);
    auto m=manager;
    unsigned char* result;
    __asm {
        push ebx
        push 0
        mov ebx, m
        mov eax, priority
        call function
        mov result, eax
        pop ebx
    }
    return result;
}

static bool streamMarkedForRelease(unsigned char* p) { return at<unsigned>(p,0x10)==2; }
static bool replayedByMenu(unsigned char* p) { return at<unsigned char>(at<unsigned char*>(p,0x14),0xc)!=0; }

static void fillSlotsForPick() {
    for(unsigned i=0;i<slotCount;++i) {
        auto p=slot(i);
        memset(p,0,slotSize);
        at<unsigned char>(p,0x34)=1;
        at<int>(p,0x4c)=i==slotCount-1?6:2;
    }
    prepare(slotCount-1,sounds[0],stream,false,true);
    *(unsigned*)va(nextFreeSlot)=0;
    *(unsigned*)va(nextHandle)=500;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    bufferTable[0x24/4]=(void*)getStatus;
    bufferTable[0x48/4]=(void*)stopBuffer;
    *(void**)va(freeImport)=(void*)recordFree;
    at<void*>(manager,0x10)=manager+0x10;
    at<void*>(manager,0x14)=manager+0x10;
    at<unsigned>(manager,8)=1;  // 0x5a11b0 looks up handles only while +8 is set
    auto p=slot(3);

    // Without the Mod: the two ways a slot leaks.
    prepare(3,sounds[0],stream,true,false);
    stopSlot(p,0.0f);
    updateNotification(p);
    check("without the Mod a stream stopped while paused by a menu is never freed (the bug)",
        at<unsigned char>(p,0x34)==1&&!streamMarkedForRelease(p));
    check("without the Mod that slot stays paused, so closing a menu plays it again (the bug)",replayedByMenu(p));

    prepare(3,sounds[0],stream,false,false);
    stopSlot(p,0.0f);
    updateNotification(p);
    check("without the Mod a stream stopped before the update saw it playing is never freed (the bug)",
        stops==1&&!streamMarkedForRelease(p));

    fillSlotsForPick();
    auto taken=acquireSlot(3);
    check("without the Mod a full table gives away a playing slot without stopping it (the bug)",
        taken==slot(slotCount-1)&&stops==0&&freedCount==0&&destroyed==0&&at<void*>(taken,0x14)==sounds[0].channel);

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x5a1210 (6 bytes) and 0x5a19e0 (7 bytes) are claimed",claimed(0x5a1210,6)&&claimed(0x5a19e0,7));
    check("the fix is logged as installed",loggedText("sound slot release installed at 0x5a1210 and 0x5a19e0"));
    check("the hooks are written",*(unsigned char*)va(0x5a1210)==0xe9&&*(unsigned char*)va(0x5a19e0)==0xe9);

    // With the Mod.
    prepare(3,sounds[0],stream,true,false);
    volatile unsigned espBefore=0, espAfter=0;
    __asm mov espBefore, esp
    auto stopped=stopSlot(p,0.0f);
    __asm mov espAfter, esp
    check("the stop returns what the game returns (1) and the stack is balanced (ret 8)",stopped==1&&espBefore==espAfter);
    check("a stream stopped while paused is no longer paused, so closing a menu does not play it",!replayedByMenu(p));
    updateNotification(p);
    check("the next update marks that stream for release (+0x10 = 2)",streamMarkedForRelease(p));

    prepare(3,sounds[0],stream,false,false);
    stopSlot(p,0.0f);
    updateNotification(p);
    check("a stream stopped before the update saw it playing is marked for release",stops==1&&streamMarkedForRelease(p));

    prepare(3,sounds[1],sample,true,false);
    stopSlot(p,0.0f);
    updateNotification(p);
    check("a sample stopped while paused is freed by the game's end notification",
        at<unsigned char>(p,0x34)==0&&at<void*>(p,0x14)==nullptr&&freedCount==1&&freedBlocks[0]==sounds[1].channel&&destroyed==1);

    prepare(3,sounds[0],stream,false,true);
    stopSlot(p,1.5f);
    updateNotification(p);
    check("a fade out of a playing stream is left to the game (not stopped, not marked yet)",
        stops==0&&at<unsigned>(p,8)==5&&!streamMarkedForRelease(p)&&at<unsigned char>(p,0x34)==1);

    prepare(3,sounds[0],stream,true,false);
    stopSlot(p,1.5f);
    updateNotification(p);
    check("a fade out of a paused stream is freed like an immediate stop",!replayedByMenu(p)&&streamMarkedForRelease(p));

    prepare(3,sounds[0],stream,true,false);
    at<int>(p,0x28)=777;
    auto byHandle=va(0x5a11b0);
    float noFade=0.0f;
    __asm {
        push esi
        push noFade
        mov esi, offset manager
        mov edx, 777
        call byHandle
        pop esi
    }
    updateNotification(p);
    check("a stop by handle (0x5a11b0, as the torture and fishing code do) goes through the fix too",
        streamMarkedForRelease(p));

    fillSlotsForPick();
    __asm mov espBefore, esp
    taken=acquireSlot(3);
    __asm mov espAfter, esp
    check("with a full table the same slot is picked and the stack is balanced (ret 4)",
        taken==slot(slotCount-1)&&espBefore==espAfter);
    check("the sound playing in the taken slot is stopped and freed",
        stops==1&&freedCount==1&&freedBlocks[0]==sounds[0].channel&&destroyed==1);
    check("the taken slot is reset for the new sound and keeps its new handle",
        at<void*>(taken,0x14)==nullptr&&at<void*>(taken,0x18)==nullptr&&at<unsigned>(taken,0x10)==0&&
        at<unsigned>(taken,0xc)==0&&at<int>(taken,0x28)==500);
    check("taking a slot is logged",loggedText("all 24 sound slots were in use; stopped the sound in slot 23"));

    at<unsigned char>(slot(5),0x34)=0;
    *(unsigned*)va(nextFreeSlot)=0;
    stops=freedCount=destroyed=0;
    taken=acquireSlot(3);
    check("a free slot is given as before, without stopping anything",
        taken==slot(5)&&stops==0&&freedCount==0&&destroyed==0);

    printf("RESULT tests=%u failures=%u; mapped exe copy, fake DirectSound and free() in this process only\n",
        tests,failures);
    return failures?1:0;
}
