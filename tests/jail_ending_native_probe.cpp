// Isolated execution of copied instructions; never attach to the game process.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>
#include <iterator>
#include <wots4/mod_api.h>

static unsigned imageBase, lookupAddress, continuation, modelNodeAddress, managerBinding;
static unsigned va(unsigned address) { return imageBase+address-0x400000; }
static unsigned exceptionCode, exceptionAddress, exceptionReadAddress;
static unsigned modelCalls;

static void jump(unsigned address, void* target) {
    address=va(address);
    auto p=(unsigned char*)address;
    p[0]=0xe9; *(int*)(p+1)=(int)((unsigned)target-address-5);
}
static unsigned __stdcall weaponModel(void*) { ++modelCalls; return 0x12345678; }
static unsigned __declspec(naked) modelNode() {
    __asm { mov eax, ecx }
    __asm { ret }
}
static void __declspec(naked) guardedActor() {
    __asm { mov ecx, managerBinding }
    __asm { mov ecx, dword ptr [ecx] }
    __asm { call lookupAddress }
    __asm { test eax, eax }
    __asm { jz absent }
    __asm { jmp continuation }
    __asm { absent: xor eax, eax }
    __asm { ret }
}
static void __declspec(naked) emptyArray() { __asm { ret 4 } }
static void __declspec(naked) guardedModel() {
    __asm { test eax, eax }
    __asm { jz absent }
    __asm { mov ecx, eax }
    __asm { jmp modelNodeAddress }
    __asm { absent: ret }
}
static int capture(EXCEPTION_POINTERS* e) {
    exceptionCode=e->ExceptionRecord->ExceptionCode;
    exceptionAddress=(unsigned)e->ExceptionRecord->ExceptionAddress-imageBase+0x400000;
    exceptionReadAddress=exceptionCode==EXCEPTION_ACCESS_VIOLATION?
        (unsigned)e->ExceptionRecord->ExceptionInformation[1]:0;
    return EXCEPTION_EXECUTE_HANDLER;
}
static bool invoke(unsigned actor, int selector, unsigned* result) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned routine=va(0x54a830);
    __try {
        __asm { push esi }
        __asm { push edi }
        __asm { mov esi, actor }
        __asm { mov edi, selector }
        __asm { call routine }
        __asm { mov ecx, result }
        __asm { mov dword ptr [ecx], eax }
        __asm { pop edi }
        __asm { pop esi }
        return true;
    } __except(capture(GetExceptionInformation())) { return false; }
}
static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s; exception=%08x at=%08x read=%08x\n",
        passed?"PASS":"FAIL",label,exceptionCode,exceptionAddress,exceptionReadAddress);
}
int main(int argc,char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=2 && argc!=3) return 2;
    std::ifstream f(argv[1],std::ios::binary);
    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),{});
    if(bytes.size()<4096) return 3;
    auto dos=(IMAGE_DOS_HEADER*)bytes.data();
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE || (unsigned)dos->e_lfanew>bytes.size()-sizeof(IMAGE_NT_HEADERS32)) return 4;
    auto nt=(IMAGE_NT_HEADERS32*)(bytes.data()+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE || nt->FileHeader.Machine!=IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.ImageBase!=0x400000) return 4;
    auto base=(unsigned char*)VirtualAlloc(nullptr,nt->OptionalHeader.SizeOfImage,
        MEM_RESERVE|MEM_COMMIT,PAGE_EXECUTE_READWRITE);
    if(!base) return 5;
    imageBase=(unsigned)base;
    memcpy(base,bytes.data(),nt->OptionalHeader.SizeOfHeaders);
    auto sections=IMAGE_FIRST_SECTION(nt);
    for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i) {
        auto& s=sections[i];
        if((size_t)s.PointerToRawData+s.SizeOfRawData>bytes.size() ||
            (size_t)s.VirtualAddress+s.SizeOfRawData>nt->OptionalHeader.SizeOfImage) return 6;
        memcpy(base+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);
    }
    auto reloc=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for(unsigned pos=reloc.VirtualAddress;pos<reloc.VirtualAddress+reloc.Size;) {
        auto block=(IMAGE_BASE_RELOCATION*)(base+pos);
        if(block->SizeOfBlock<8 || pos+block->SizeOfBlock>nt->OptionalHeader.SizeOfImage) return 8;
        auto entries=(WORD*)(block+1);
        for(unsigned i=0;i<(block->SizeOfBlock-8)/2;++i) {
            if((entries[i]>>12)==IMAGE_REL_BASED_HIGHLOW)
                *(unsigned*)(base+block->VirtualAddress+(entries[i]&0xfff))+=imageBase-0x400000;
            else if(entries[i]>>12) return 8;
        }
        pos+=block->SizeOfBlock;
    }
    lookupAddress=va(0x465590); continuation=va(0x54a83b);
    modelNodeAddress=va(0x6d41f0); managerBinding=va(0x9e9658);
    // No PE entry point, TLS, Steam imports, save, or game assets are executed.
    const unsigned char signature[]={0xe8,0x55,0xad,0xf1,0xff};
    if(memcmp((void*)va(0x54a836),signature,sizeof(signature)) ||
        *(unsigned*)va(0x54a832)!=va(0x9e9658)) return 7;
    if(argc==3) {
        auto diagnostic=LoadLibraryA(argv[2]);
        auto initialize=diagnostic?(wots4::Initialize)GetProcAddress(diagnostic,"Wots4ModInitialize"):nullptr;
        wots4::ModContext invalid={}, context{sizeof(context),wots4::modApiVersion,nullptr,
            diagnostic,(HMODULE)base,L".\\",nullptr};
        check("diagnostic export rejects incompatible context",initialize&&!initialize(&invalid));
        check("diagnostic module initializes in isolated process",initialize&&initialize(&context));
        check("diagnostic initialization is idempotent",initialize&&initialize(&context));
    }
    unsigned char originalModel[5], originalNode[5];
    memcpy(originalModel,(void*)va(0x42bc00),5); memcpy(originalNode,(void*)va(0x6d41f0),5);
    jump(0x42bc00,(void*)weaponModel); jump(0x6d41f0,(void*)modelNode);
    unsigned manager[8]={}, tree[6]={}, actor[8]={}, equipment[0x300/4]={};
    *(unsigned*)va(0x9e9658)=(unsigned)manager;
    unsigned result=0xeeeeeeee;
    bool ok=invoke(17,-1,&result);
    check("original missing actor / right hand crashes at read 0x0c",
        !ok&&exceptionCode==EXCEPTION_ACCESS_VIOLATION&&exceptionAddress==0x54a883&&exceptionReadAddress==0xc);
    ok=invoke(17,-2,&result);
    check("original missing actor / left hand crashes at read 0x0c",
        !ok&&exceptionAddress==0x54a848&&exceptionReadAddress==0xc);
    ok=invoke(17,0,&result);
    check("original nonnegative selector returns zero",ok&&result==0);
    manager[2]=(unsigned)tree; tree[4]=17; tree[5]=(unsigned)actor;
    actor[3]=(unsigned)equipment; equipment[0x2c4/4]=~0u; equipment[0x2c8/4]=~0u;
    ok=invoke(17,-1,&result);
    check("original existing actor / no weapon returns zero",ok&&result==0);
    equipment[0x2c4/4]=0; equipment[0x18c/4]=0x87654321;
    ok=invoke(17,-1,&result);
    check("original equipped actor passes weapon to model path",ok&&result==0x12345678&&modelCalls==1);
    unsigned char original[5]; memcpy(original,(void*)va(0x54a830),5);
    jump(0x54a830,(void*)guardedActor);
    manager[2]=0;
    ok=invoke(17,-1,&result);
    check("guarded missing actor / right hand returns zero",ok&&result==0);
    ok=invoke(17,-2,&result);
    check("guarded missing actor / left hand returns zero",ok&&result==0);
    manager[2]=(unsigned)tree;
    ok=invoke(17,-1,&result);
    check("guard preserves original equipped-actor path",ok&&result==0x12345678&&modelCalls==2);
    equipment[0x2c4/4]=~0u;
    ok=invoke(17,-1,&result);
    check("guard preserves original unarmed-actor result",ok&&result==0);
    memcpy((void*)va(0x54a830),original,5);
    manager[2]=0;
    ok=invoke(17,-1,&result);
    check("restoring original bytes restores fault",!ok&&exceptionAddress==0x54a883);
    // Real weapon-model helper, real model-node instructions. Only the temporary
    // array allocation is a fixture; it returns before the faulting dereference.
    memcpy((void*)va(0x42bc00),originalModel,5); memcpy((void*)va(0x6d41f0),originalNode,5);
    jump(0x48cb50,(void*)emptyArray);
    unsigned weapon[16]={};
    manager[2]=(unsigned)tree; equipment[0x2c4/4]=0; equipment[0x18c/4]=(unsigned)weapon;
    ok=invoke(17,-1,&result);
    check("original existing weapon without loaded model crashes at read zero",
        !ok&&exceptionCode==EXCEPTION_ACCESS_VIOLATION&&exceptionAddress==0x6d4202&&exceptionReadAddress==0);
    jump(0x54a8ab,(void*)guardedModel);
    ok=invoke(17,-1,&result);
    check("model guard returns zero when actual model helper returns null",ok&&result==0);
    jump(0x42bc00,(void*)weaponModel); jump(0x6d41f0,(void*)modelNode);
    ok=invoke(17,-1,&result);
    check("model guard preserves valid model-node path",ok&&result==0x12345678&&modelCalls==3);
    printf("RESULT tests=%u failures=%u; synthetic isolated process only\n",tests,failures);
    VirtualFree(base,0,MEM_RELEASE);
    return failures?1:0;
}
