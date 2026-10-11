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
static void check(const char* label, bool passed);
static unsigned attachmentCalls, attachmentActor, attachmentObject;
static void __declspec(naked) attachmentFixture() {
    __asm { inc attachmentCalls }
    __asm { mov attachmentActor, ecx }
    __asm { mov eax, dword ptr [esp + 8] }
    __asm { mov attachmentObject, eax }
    __asm { ret }
}
static bool invokeTimeline(void* cursor) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned routine=va(0x58c1f0);
    __try {
        __asm { push cursor }
        __asm { push 0 }
        __asm { call routine }
        __asm { add esp, 8 }
        return true;
    } __except(capture(GetExceptionInformation())) { return false; }
}
static void timelineChecks() {
    // Real type-77 handler and real float-to-int conversion. Stub only the
    // attachment operation, so the negative-ID branch itself stays original.
    unsigned char original[5]; memcpy(original,(void*)va(0x550be0),5);
    jump(0x550be0,(void*)attachmentFixture);
    unsigned row[12]={}, next[12]={};
    row[0]=(unsigned)next; row[3]=17;
    row[5]=0x3f800000; // first attachment flag = 1.0f
    attachmentCalls=0;
    *(unsigned*)va(0x9eb5fc)=0; // negative IDs must not touch the scene manager
    for(int id: {-999,-1}) {
        float asFloat=(float)id; memcpy(&row[7],&asFloat,sizeof(asFloat));
        unsigned cursor=(unsigned)row;
        bool ok=invokeTimeline(&cursor);
        check(id==-999?"type 77 skips failed creation ID -999 and advances cursor":
            "type 77 skips negative object ID -1 and advances cursor",
            ok&&attachmentCalls==0&&cursor==(unsigned)next);
    }
    row[7]=0; unsigned cursor=(unsigned)row;
    bool ok=invokeTimeline(&cursor);
    check("type 77 forwards valid object ID zero to attachment and advances cursor",
        ok&&attachmentCalls==1&&attachmentActor==17&&attachmentObject==0&&
        cursor==(unsigned)next);
    memcpy((void*)va(0x550be0),original,5);
}
static bool invokeActorQuery(unsigned address, unsigned id, unsigned* result) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned routine=va(address);
    __try {
        __asm { push esi }
        __asm { mov esi, id }
        __asm { call routine }
        __asm { mov ecx, result }
        __asm { mov dword ptr [ecx], eax }
        __asm { pop esi }
        return true;
    } __except(capture(GetExceptionInformation())) { return false; }
}
static void definitionFallbackChecks() {
    // Original helpers used by RegisterEventChara: exact definition first,
    // then character identity. A loaded definition 640 can satisfy 444's
    // identity query (both have character identity 14 in AcNPCDefine.l).
    unsigned manager[8]={}, node[6]={}, actor[8]={}, param[0x340/4]={};
    manager[3]=(unsigned)node;
    node[1]=(unsigned)manager+4; node[5]=(unsigned)actor;
    actor[3]=(unsigned)param; param[1]=17;
    param[0x9c/4]=640; param[0x338/4]=14;
    *(unsigned*)va(0x9e9658)=(unsigned)manager;
    unsigned result=0;
    bool ok=invokeActorQuery(0x4a0720,444,&result);
    check("exact definition 444 query does not match loaded definition 640",ok&&result==0);
    ok=invokeActorQuery(0x4a0720,640,&result);
    check("exact definition 640 query finds loaded actor",ok&&result==(unsigned)actor);
    ok=invokeActorQuery(0x4a07a0,14,&result);
    check("character identity fallback finds same actor despite definition mismatch",
        ok&&result==(unsigned)actor);
    ok=invokeActorQuery(0x4a07a0,11,&result);
    check("character identity fallback does not match a different character",ok&&result==0);
}
// Copied achievement instructions with a synthetic Steam interface (no network).
// This proves local fault behavior, not the cause of the reported ending crash.
static unsigned statsTable[11], statsObject, getterCalls, nullGetterCall;
static unsigned setCalls, storeCalls, getCalls;
static bool statsSuccess;
static void* __cdecl statsGetter() {
    ++getterCalls;
    return getterCalls==nullGetterCall?nullptr:&statsObject;
}
static bool __fastcall setAchievement(void*, void*, const char*) {
    ++setCalls; return statsSuccess;
}
static bool __fastcall storeStats(void*, void*) {
    ++storeCalls; return statsSuccess;
}
static bool __fastcall getAchievement(void*, void*, const char*, bool* achieved) {
    ++getCalls; *achieved=false; return statsSuccess;
}
static bool invokeSync(void* notice) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned routine=va(0x406b10);
    __try {
        __asm { push notice }
        __asm { call routine }
        return true;
    } __except(capture(GetExceptionInformation())) { return false; }
}
static bool invokeAchievement(unsigned id, void* notice) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned routine=va(0x406a30);
    __try {
        __asm { push edi }
        __asm { mov edi, id }
        __asm { push notice }
        __asm { call routine }
        __asm { pop edi }
        return true;
    } __except(capture(GetExceptionInformation())) { return false; }
}
static void achievementChecks() {
    std::vector<unsigned char> param(0x49c00,0);
    unsigned notice[64]={};
    *(unsigned*)va(0x9e967c)=(unsigned)param.data();
    *(unsigned*)va(0x905560)=(unsigned)statsGetter;
    statsObject=(unsigned)statsTable;
    statsTable[0x18/4]=(unsigned)getAchievement;
    statsTable[0x1c/4]=(unsigned)setAchievement;
    statsTable[0x28/4]=(unsigned)storeStats;
    statsSuccess=true; getterCalls=setCalls=storeCalls=0; nullGetterCall=1;
    bool ok=invokeAchievement(1,notice);
    check("absent stats crashes before SetAchievement after local bit update",
        !ok&&exceptionAddress==0x406acd&&exceptionReadAddress==0&&
        param[0x48b51]==1&&setCalls==0&&storeCalls==0);
    getterCalls=0;
    ok=invokeAchievement(1,notice);
    check("already unlocked local bit skips Steam even with absent stats",
        ok&&getterCalls==0);
    param[0x48b51]=0; getterCalls=0; nullGetterCall=2;
    ok=invokeAchievement(1,notice);
    check("stats disappearing before StoreStats crashes after SetAchievement",
        !ok&&exceptionAddress==0x406aef&&setCalls==1&&storeCalls==0);
    param[0x48b51]=0; getterCalls=setCalls=storeCalls=0; nullGetterCall=0;
    statsSuccess=false;
    ok=invokeAchievement(1,notice);
    check("Steam false responses alone do not crash or revert local bit",
        ok&&param[0x48b51]==1&&setCalls==1&&storeCalls==1&&notice[0]!=0);
    param[0x48b51]=0; getterCalls=setCalls=storeCalls=0; statsSuccess=true;
    ok=invokeAchievement(1,notice);
    check("valid Steam interface completes achievement notification",
        ok&&getterCalls==2&&setCalls==1&&storeCalls==1);
    memset(param.data()+0x48b50,1,50); param[0x48b51]=0;
    getterCalls=setCalls=storeCalls=0; nullGetterCall=2;
    ok=invokeAchievement(1,notice);
    check("all-achievements second SetAchievement also dereferences absent stats",
        !ok&&exceptionAddress==0x406ae3&&setCalls==1&&storeCalls==0);
    param[0x48b51]=0; getterCalls=setCalls=storeCalls=0; nullGetterCall=0;
    ok=invokeAchievement(1,notice);
    check("all-achievements valid path makes two sets and one store",
        ok&&getterCalls==3&&setCalls==2&&storeCalls==1);
    getterCalls=getCalls=0; nullGetterCall=1;
    ok=invokeSync(notice);
    check("load-time achievement sync also crashes with absent stats",
        !ok&&exceptionAddress==0x406b2a&&exceptionReadAddress==0&&getCalls==0);
    getterCalls=getCalls=setCalls=storeCalls=0; nullGetterCall=0; statsSuccess=false;
    ok=invokeSync(notice);
    check("load-time false GetAchievement response alone completes all 50 queries",
        ok&&getCalls==50&&getterCalls==50&&setCalls==0&&storeCalls==0);
}
static unsigned objectDeletes, deletedObject;
static void __declspec(naked) objectDeleteFixture() {
    __asm { inc objectDeletes }
    __asm { mov deletedObject, eax }
    __asm { ret }
}
static bool invokeCleanup(unsigned address, void* manager, int id) {
    exceptionCode=exceptionAddress=exceptionReadAddress=0;
    unsigned routine=va(address);
    __try {
        __asm { push edi }
        __asm { mov edi, manager }
        __asm { mov edx, id }
        __asm { call routine }
        __asm { pop edi }
        return true;
    } __except(capture(GetExceptionInformation())) { return false; }
}
static void mapObjectCleanupChecks() {
    // Static evidence: map loading reaches 0x52cc70 through 0x4c3c50 and
    // 0x5248e0. Execute its real 200-slot cleanup with only engine deletion
    // replaced. This does not execute the map loader or establish U-CF-5.
    unsigned objects[0x7100/4]={}, definitions[0x3000/4]={};
    unsigned collision[0x2900/4]={};
    objects[1]=(unsigned)collision;
    objects[2]=(unsigned)definitions;
    for(unsigned i=0;i<200;++i) objects[(i*0x88+0x58)/4]=~0u;
    objects[(37*0x88+0x58)/4]=7;
    objects[(37*0x88+0x5c)/4]=112;
    unsigned char original[5]; memcpy(original,(void*)va(0x513110),5);
    jump(0x513110,(void*)objectDeleteFixture);
    objectDeletes=0;
    bool ok=invokeCleanup(0x52cc70,objects,0);
    check("map dynamic-object cleanup deletes fixture definition 112 and clears its slot",
        ok&&objectDeletes==1&&deletedObject==7&&objects[(37*0x88+0x58)/4]==~0u);
    memcpy((void*)va(0x513110),original,5);

    // Real DeleteCollisionSquare helper. Its lifecycle callback is inspected
    // statically; this fixture only proves the removal semantics.
    auto ids=(int*)va(0x9eb5a0);
    for(unsigned i=0;i<16;++i) ids[i]=i<6?(int)i:-1;
    *(unsigned*)va(0x9ecf48)=(unsigned)collision;
    // 0x8d4b06 is the copied executable's memset import thunk.
    *(unsigned*)va(0x905228)=(unsigned)&memset;
    ok=invokeCleanup(0x565020,nullptr,-1);
    bool cleared=true;
    for(unsigned i=0;i<16;++i) cleared&=ids[i]==-1;
    for(unsigned i=0;i<6;++i) cleared&=(collision[(i*40+0x1c)/4]&1)!=0;
    check("collision removal marks all six fixtures deleted and clears all 16 IDs",ok&&cleared);
    ok=invokeCleanup(0x565020,nullptr,-1);
    check("collision removal remains safe when invoked again",ok);
}
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
    achievementChecks();
    timelineChecks();
    definitionFallbackChecks();
    mapObjectCleanupChecks();
    printf("RESULT tests=%u failures=%u; synthetic isolated process only\n",tests,failures);
    VirtualFree(base,0,MEM_RELEASE);
    return failures?1:0;
}
