// Runs the game's own effect update (0x4a4c80) and character effect release
// (0x48d520) from a copy of the exe mapped into this process, with mod.dll
// installed on it. Never attaches to the game process or reads saves.
#include <cstdlib>
#include "mapped_exe.h"

using namespace mapped_exe;

static unsigned exceptionCode, exceptionAddress, exceptionReadAddress;

static int filter(EXCEPTION_POINTERS* e) {
    exceptionCode=e->ExceptionRecord->ExceptionCode;
    exceptionAddress=(unsigned)e->ExceptionRecord->ExceptionAddress-imageBase+0x400000;
    exceptionReadAddress=e->ExceptionRecord->NumberParameters>1?(unsigned)e->ExceptionRecord->ExceptionInformation[1]:0;
    return EXCEPTION_EXECUTE_HANDLER;
}
// Effect base update, virtual +0xc: thiscall, one byte argument.
static bool updateEffect(void* effect) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned function=va(0x4a4c80);
    __try {
        __asm { push 0 }
        __asm { mov ecx, effect }
        __asm { call function }
    } __except(filter(GetExceptionInformation())) { return false; }
    return true;
}
// Character virtual +0x68: thiscall, no arguments.
static bool releaseEffects(void* character) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned function=va(0x48d520);
    __try {
        __asm { mov ecx, character }
        __asm { call function }
    } __except(filter(GetExceptionInformation())) { return false; }
    return true;
}

// Base effect (0x20 bytes) as made by 0x565590; the blood effect is larger (0x70).
static unsigned char* newEffect(unsigned char type, void* node, void* follow, unsigned size=0x20) {
    auto e=(unsigned char*)calloc(1,size);
    *(unsigned*)e=va(0x9837bc);
    *(void**)(e+4)=node; *(void**)(e+8)=follow;
    *(float*)(e+0xc)=-1.0f;  // No life limit, as in the dump
    e[0x18]=type; e[0x19]=0x1e;
    return e;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    const unsigned char original[]={0x53,0x56,0x57,0x8b,0xf9};

    // Joints of one character model, each a separate page so it can be freed.
    const unsigned jointCount=0x17;
    unsigned char* joints[jointCount];
    for(auto& j:joints) {
        j=(unsigned char*)VirtualAlloc(nullptr,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        *(float*)(j+0x88)=1.0f; *(float*)(j+0x8c)=2.0f; *(float*)(j+0x90)=3.0f;
    }
    static unsigned char model[0x100], otherJoint[0x100], parameter[0x400], character[0x400];
    static unsigned char ownNodes[8][0x100];
    for(unsigned j=0;j<jointCount;++j) *(void**)(model+0x48+j*4)=joints[j];
    *(unsigned*)(parameter+4)=18;          // Runtime character ID (Kinugawa)
    *(void**)(character+0xc)=parameter;
    *(void**)(character+0x18)=model;
    *(int*)(character+0x84)=-1;

    static unsigned char manager[0x1c+350*4];
    *(void**)va(0x9e966c)=manager;
    auto slot=[&](unsigned i,void* e) { *(void**)(manager+0x1c+i*4)=e; };
    unsigned char* aura[6];
    const unsigned auraJoints[6]={1,20,21,7,6,5};
    for(unsigned i=0;i<6;++i) slot(275+i,aura[i]=newEffect(14,ownNodes[i],joints[auraJoints[i]]));
    auto blood=newEffect(0x84,ownNodes[6],joints[2],0x70);
    *(unsigned*)(blood+0x28)=18;
    slot(10,blood);
    auto foreign=newEffect(14,ownNodes[7],otherJoint);
    slot(11,foreign);

    check("effect update copies the joint position while the joint is alive",
        updateEffect(aura[1])&&*(float*)(ownNodes[1]+0x54)==1.0f&&*(float*)(ownNodes[1]+0x5c)==3.0f);

    // Without the Mod: the game detaches only the blood effect, and the aura reads a freed joint.
    auto stale=newEffect(14,ownNodes[0],nullptr);
    {
        unsigned char* page=(unsigned char*)VirtualAlloc(nullptr,0x1000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
        *(void**)(stale+8)=page;
        VirtualFree(page,0x1000,MEM_DECOMMIT);
    }
    bool faulted=!updateEffect(stale);
    check("without the Mod, a freed joint faults at 0x4a4cf8 (same as the crash log)",
        faulted&&exceptionCode==EXCEPTION_ACCESS_VIOLATION&&exceptionAddress==0x4a4cf8&&
        exceptionReadAddress==(unsigned)*(void**)(stale+8)+0x88);

    // Install the Mod on the mapped copy.
    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext invalid{}, context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("an incompatible context is refused",!initialize(&invalid)&&claims.empty());

    // An exe where the code of every fix changed is refused before anything is claimed or written.
    auto joint=(unsigned char*)va(0x56590e);
    auto setTaken=(unsigned char*)va(0x553f50);
    auto deviceOffset=(unsigned char*)va(0x88e10b);  // 0x2238 in mov eax,[eax+0x2238]
    auto screenCompare=(unsigned char*)va(0x5383fe);  // cmp ebx,edi of the screen size check
    auto setFullScreen=(unsigned char*)va(0x53819b);  // mov byte [esi+0xec],1 of the full screen setup
    auto brushIndex=(unsigned char*)va(0x5388cb);     // push 0 (WHITE_BRUSH) of the window class
    auto markHandled=(unsigned char*)va(0x6a06c8);    // 1 in mov word [edi+0xa],1 of the night visit result
    auto upAxis=(unsigned char*)va(0x59feb8);         // 0x68 in fld [edi+0x68] of the 3D listener's up axis
    auto fontStore=(unsigned char*)va(0x4c5628);      // Indexed glyph store of the font database
    auto keyMapping=(unsigned char*)va(0x4f285c);     // 0x20 in cmp byte [ebx+0x20],0 of the keyboard buttons
    auto typedResult=(unsigned char*)va(0x4f4bf0);    // test eax,eax after the text input's isalnum
    auto nameLimit=(unsigned char*)va(0x5f760f);      // 0x1f in cmp esi,0x1f of the player name input
    auto soundFade=(unsigned char*)va(0x5a124e);      // 0x41 in test ah,0x41 of the sound stop's fade test
    auto selectWindow=(unsigned char*)va(0x4ed252);   // 0x4c in mov [edi+0x4c],eax of the title's select window
    auto listedBits=(unsigned char*)va(0x536b0a);     // 0x20 in cmp [ebp-0x40],0x20 of the resolution list
    auto corpseParts=(unsigned char*)va(0x4f5ad1);   // +0x326 in the drop candidate builder
    joint[3]=0x4c; setTaken[6]=2; deviceOffset[0]=0x3c; screenCompare[1]=0xde; setFullScreen[6]=2; brushIndex[0]=4;
    markHandled[0]=2; upAxis[0]=0x6c; fontStore[0]=0x86; keyMapping[0]=0x24; typedResult[0]=0x84; nameLimit[0]=0x20; soundFade[0]=0x40;
    selectWindow[0]=0x50; listedBits[0]=0x10; corpseParts[0]=0x27;
    check("an unknown exe is refused without changes",
        !initialize(&context)&&claims.empty()&&!memcmp((void*)va(0x48d520),original,sizeof(original)));
    joint[3]=0x48; setTaken[6]=1; deviceOffset[0]=0x38; screenCompare[1]=0xdf; setFullScreen[6]=1; brushIndex[0]=0;
    markHandled[0]=1; upAxis[0]=0x68; fontStore[0]=0x87; keyMapping[0]=0x20; typedResult[0]=0x85; nameLimit[0]=0x1f; soundFade[0]=0x41;
    selectWindow[0]=0x4c; listedBits[0]=0x20; corpseParts[0]=0x26;

    // This host has no onFrame, so the text input keyboard fix is skipped (text_input_native_probe has it).
    check("the Mod initializes on the known exe",initialize(&context)!=FALSE);
    check("the Mod claims the seventeen game ranges after releasing the refused initialization",
        claims.size()==17&&claimed(0x4a0560,6)&&claimed(0x4f57f0,6)&&claimed(0x4ee380,6)&&claimed(0x4c5530,5)&&claimed(0x68a330,5)&&
        claimed(0x48d520,5)&&claimed(0x55e140,5)&&claimed(0x5383d0,9)&&
        claimed(0x538180,7)&&claimed(0x5384c0,6)&&claimed(0x4d9f50,5)&&claimed(0x6a2660,5)&&
        claimed(0x4f4be7,6)&&claimed(0x5f7607,6)&&claimed(0x5a1210,6)&&claimed(0x5a19e0,7)&&
        claimed(0x536af9,6)&&releases==1);
    check("the text input keyboard fix is skipped without onFrame",
        loggedText("text input keyboard buttons skipped: the ModLoader does not call this Mod every frame (onFrame)"));
    check("the hook is written at 0x48d520",*(unsigned char*)va(0x48d520)==0xe9);
    check("both fixes are logged as installed",
        loggedText("follow-effect detach installed at 0x48d520")&&
        loggedText("property return after Madara's rescue installed at 0x55e140"));

    check("character effect release runs through the hook",releaseEffects(character));
    bool auraDetached=true;
    for(auto e:aura) auraDetached=auraDetached&&*(void**)(e+8)==nullptr;
    check("the six auras on the released model are detached",auraDetached);
    check("the game still detaches the blood effect by ID",
        *(void**)(blood+8)==nullptr&&*(int*)(blood+0x28)==-1);
    check("an effect on another model is kept",*(void**)(foreign+8)==otherJoint);
    check("the detach is logged",logged.find("detached 6 effect(s)")!=std::string::npos);

    // The model is freed after 0x48d520 returns (0x48d4b4, 0x48df40).
    for(auto j:joints) VirtualFree(j,0,MEM_RELEASE);
    *(float*)(ownNodes[0]+0x54)=9.0f;
    bool survived=true;
    for(auto e:aura) survived=survived&&updateEffect(e);
    check("with the Mod, updating the auras after the model is freed does not fault",survived);
    check("a detached aura stays where it was",*(float*)(ownNodes[0]+0x54)==9.0f);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only\n",tests,failures);
    return failures?1:0;
}
