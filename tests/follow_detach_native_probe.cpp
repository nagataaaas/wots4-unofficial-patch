// Runs the game's own effect update (0x4a4c80) and character effect release
// (0x48d520) from a copy of the exe mapped into this process, with mod.dll
// installed on it. Never attaches to the game process or reads saves.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <wots4/mod_api.h>

static unsigned imageBase;
static unsigned va(unsigned address) { return imageBase+address-0x400000; }
static unsigned tests, failures;
static unsigned exceptionCode, exceptionAddress, exceptionReadAddress;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

// Host recorded by the fake ModLoader.
static unsigned claimAddress, claimLength, claims, releases;
static std::string logged;
static void WINAPI hostLog(HMODULE, const char* message) { logged+=message; logged+="\n"; printf("  log: %s\n",message); }
static BOOL WINAPI hostClaim(HMODULE, uintptr_t address, uint32_t length) {
    ++claims; claimAddress=(unsigned)address; claimLength=length; return TRUE;
}
static void WINAPI hostRelease(HMODULE) { ++releases; }

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

static bool mapImage(const char* path) {
    std::ifstream f(path,std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});
    if(bytes.size()<4096) return false;
    auto dos=(IMAGE_DOS_HEADER*)bytes.data();
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || (unsigned)dos->e_lfanew>bytes.size()-sizeof(IMAGE_NT_HEADERS32)) return false;
    auto nt=(IMAGE_NT_HEADERS32*)(bytes.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.ImageBase!=0x400000) return false;
    auto base=(unsigned char*)VirtualAlloc(nullptr,nt->OptionalHeader.SizeOfImage,
        MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    if(!base) return false;
    imageBase=(unsigned)base;
    memcpy(base,bytes.data(),nt->OptionalHeader.SizeOfHeaders);
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        auto& s=sections[i];
        if((size_t)s.PointerToRawData+s.SizeOfRawData>bytes.size() ||
            (size_t)s.VirtualAddress+s.SizeOfRawData>nt->OptionalHeader.SizeOfImage) return false;
        memcpy(base+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);
    }
    auto reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for(unsigned pos=reloc.VirtualAddress;pos<reloc.VirtualAddress+reloc.Size;) {
        auto block=(IMAGE_BASE_RELOCATION*)(base+pos);
        if(block->SizeOfBlock<8 || pos+block->SizeOfBlock>nt->OptionalHeader.SizeOfImage) return false;
        auto entries=(WORD*)(block+1);
        for(unsigned i=0;i<(block->SizeOfBlock-8)/2;++i) {
            if((entries[i]>>12)==IMAGE_REL_BASED_HIGHLOW)
                *(unsigned*)(base+block->VirtualAddress+(entries[i]&0xfff))+=imageBase-0x400000;
            else if(entries[i]>>12) return false;
        }
        pos+=block->SizeOfBlock;
    }
    // No PE entry point, TLS, Steam imports, save, or game assets are executed.
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
    check("an incompatible context is refused",!initialize(&invalid)&&claims==0);

    // A changed exe is refused before anything is claimed or written.
    auto code=(unsigned char*)va(0x56590e);
    code[3]=0x4c;
    check("an unknown exe is refused without changes",
        !initialize(&context)&&claims==0&&!memcmp((void*)va(0x48d520),original,sizeof(original)));
    code[3]=0x48;

    check("the Mod initializes on the known exe",initialize(&context)!=FALSE);
    check("the Mod claims 5 bytes at 0x48d520",claims==1&&claimAddress==va(0x48d520)&&claimLength==5&&releases==0);
    check("the hook is written at 0x48d520",*(unsigned char*)va(0x48d520)==0xe9);

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
