// Runs the original death drop, candidate builder and corpse search on synthetic characters
// in a private exe copy, before and after loading the real Mod. No game assets or saves.
#include "mapped_exe.h"
#include "empty_corpse_loot.h"
#include <array>
#include <memory>

using namespace mapped_exe;
using namespace unofficial_patch;
constexpr unsigned automaticDropPendingOffset=0xf8;

template<class T> static T& field(void* object, unsigned offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(object)+offset);
}
static void jump(unsigned address, void* target) {
    auto code=reinterpret_cast<uint8_t*>(va(address));
    code[0]=0xe9;
    field<int32_t>(code,1)=static_cast<int32_t>(reinterpret_cast<unsigned>(target)-va(address)-5);
}
struct Node {
    unsigned unused=0;
    Node* parent=nullptr;
    Node* left=nullptr;
    Node* right=nullptr;
    int16_t key=0, padding=0;
    void* value=nullptr;
};
struct Tree {
    Node header;
    std::vector<std::unique_ptr<Node>> nodes;
    Tree() { header.left=&header; header.right=&header; }
    void add(int16_t key, void* value) {
        auto node=std::make_unique<Node>();
        node->key=key; node->value=value;
        Node** link=&header.parent;
        Node* parent=&header;
        while(*link) { parent=*link; link=key<parent->key?&parent->left:&parent->right; }
        node->parent=parent; *link=node.get();
        if(header.left==&header||key<header.left->key) header.left=node.get();
        if(header.right==&header||key>header.right->key) header.right=node.get();
        nodes.push_back(std::move(node));
    }
};

static void* playerActor;
static unsigned randomCalls, playerLookups, spawnedItems, spawnedRow;
static int randomValue=40;
static void* __cdecl lookupPlayer() { ++playerLookups; return playerActor; }
static int __cdecl controlledRandom() { ++randomCalls; return randomValue; }
static bool __cdecl ignoreAssertion() { return false; }
__declspec(naked) static void recordSpawn() {
    __asm {
        movzx eax, word ptr [ebx]
        mov spawnedRow, eax
        inc spawnedItems
        ret 8
    }
}
static void automaticDrop(void* actor) {
    unsigned function=va(0x49c030);
    __asm { mov esi, actor
        call function }
}
static void* findCorpse(void* player) {
    unsigned function=va(0x4a0560);
    void* result;
    __asm { mov ebx, player
        push 0x41200000
        call function
        add esp, 4
        mov result, eax }
    return result;
}
static bool draw(void* manager, void* actor, void* output) {
    unsigned function=va(0x4f50b0), result;
    __asm { mov ecx, output
        mov edx, actor
        push manager
        call function
        mov result, eax }
    return (result&255)!=0;
}

struct Fixture {
    std::vector<uint8_t> state=std::vector<uint8_t>(0x50000);
    std::array<uint8_t,0x400> character{}, player{};
    std::array<uint8_t,0x220> actor{}, playerBody{};
    std::array<uint8_t,0x40> sect{};
    std::array<uint8_t,250> dropRecord{};
    std::array<uint8_t,0xd00> definitions{};
    std::array<uintptr_t,10> definitionManager{};
    void* definitionTable=nullptr;
    void* activeDefinitionManager=nullptr;
    Tree dropTree, itemTree, dataTree;
    struct { unsigned unused=0; Tree characters; } characterManager;
    struct Item { int16_t row, kind, id; };
    std::array<Item,9> items{{{255,2,78},{265,2,154},{352,4,165},{397,3,166},
        {675,10,0},{476,10,71},{36,6,37},{15,6,16},{1,6,2}}};
    std::array<std::array<uint8_t,64>,3> itemObjects{};
    struct HeldItem { int16_t id, count; uint32_t kind; };
    std::array<HeldItem,5> inventory{{{37,1,6},{16,1,6},{2,1,6},{2,1,6},{2,1,6}}};

    Fixture(int difficulty=2, bool ownsStyle=true, int definition=195, int table=20) {
        field<void*>(actor.data(),0xc)=character.data();
        field<void*>(playerBody.data(),0xc)=player.data(); playerActor=playerBody.data();
        characterManager.characters.add(1,actor.data());
        *(void**)va(0x9e9658)=&characterManager;
        *(void**)va(0x9e967c)=state.data(); state[0x18]=static_cast<uint8_t>(difficulty);
        *(unsigned*)va(0x9ecbb8)=0;
        field<int32_t>(character.data(),4)=1;
        field<int32_t>(character.data(),characterDefinitionOffset)=definition;
        field<unsigned>(character.data(),0x114)=0x400000;
        field<unsigned>(character.data(),0x2fc)=0x400101;
        field<float>(character.data(),0xdc)=1.0f;
        field<float>(player.data(),0xc4)=1.0f;
        field<void*>(character.data(),0x284)=sect.data();
        if(ownsStyle) field<void*>(player.data(),0x284)=sect.data();
        field<int16_t>(sect.data(),4)=71;
        field<int16_t>(sect.data(),0xc)=1;
        field<int16_t>(sect.data(),0x1e)=6;
        field<int16_t>(sect.data(),0x20)=50;
        field<int16_t>(sect.data(),0x22)=-1;
        for(unsigned index=0;index<20;++index) field<int16_t>(state.data(),0x48e4+index*24)=-1;
        field<void*>(character.data(),0x1ec)=inventory.data();
        field<void*>(character.data(),0x1f0)=inventory.data()+inventory.size();
        for(auto& item:items) itemTree.add(item.row,&item.kind);
        for(unsigned index=0;index<itemObjects.size();++index) {
            const int16_t id[]={37,16,2};
            field<int16_t>(itemObjects[index].data(),4)=id[index];
            dataTree.add(id[index],itemObjects[index].data());
        }
        *(void**)va(0x9e96a0)=&itemTree.header;
        *(void**)va(0x9e96ac)=&dataTree.header;
        for(unsigned level=0;level<4;++level) {
            auto row=dropRecord.data()+2+level*62;
            row[0]=100;
            auto entries=reinterpret_cast<CorpseLootCandidate*>(row+2);
            for(unsigned index=0;index<10;++index) entries[index]={-1,0,0};
            if(level<2) {
                entries[0]={36,10,1}; entries[1]={255,5,1}; entries[2]={352,5,1};
                entries[3]={397,5,1}; entries[4]={675,75,1};
            } else {
                entries[0]={255,10,1}; entries[1]={265,20,1}; entries[2]={352,20,1};
                entries[3]={397,20,1}; entries[4]={675,30,1};
            }
            entries[5]={15,0,1}; entries[6]={1,0,3};
        }
        dropTree.add(static_cast<int16_t>(table),dropRecord.data());
        *(void**)va(0x9e9690)=&dropTree.header;
        constexpr unsigned definitionRecord=0xc80;
        field<unsigned>(definitions.data(),0)=796;
        field<unsigned>(definitions.data(),12+definition*4)=definitionRecord;
        field<int16_t>(definitions.data(),definitionRecord+0x26)=static_cast<int16_t>(table);
        definitionTable=definitions.data(); definitionManager[0]=4;
        definitionManager[9]=reinterpret_cast<uintptr_t>(&definitionTable);
        activeDefinitionManager=definitionManager.data();
        *(void**)va(0x9e9640)=&activeDefinitionManager;
        actor[automaticDropPendingOffset]=1;
        randomCalls=playerLookups=spawnedItems=spawnedRow=0; randomValue=40;
    }
};

static bool rejectClaim;
static BOOL WINAPI claim(HMODULE owner, uintptr_t address, uint32_t length) {
    return rejectClaim&&address==va(0x4f57f0)?FALSE:hostClaim(owner,address,length);
}
static bool writeSettings(const wchar_t* path, bool enabled) {
    FILE* file=nullptr;
    if(_wfopen_s(&file,path,L"wb")||!file) return false;
    const char* disabled[]={"follow_detach","property_return","translucent_depth","physical_screen_check",
        "skip_full_screen_reapply","no_white_background","yobai_result_reset","listener_orientation",
        "stereo_3d_sound","fullwidth_digits","sound_slot_release","text_input_keyboard","text_input_characters",
        "player_name_characters","title_select_window","display_mode_list"};
    for(auto section:disabled) fprintf(file,"[%s]\r\nenabled=0\r\n",section);
    // Missing keys enable fixes, so the successful installation also tests the default.
    if(!enabled) fputs("[empty_corpse_loot]\r\nenabled=0\r\n",file);
    fclose(file); return true;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3||!mapImage(argv[1])) return 2;
    jump(0x504c80,reinterpret_cast<void*>(&ignoreAssertion));
    jump(0x4f83b0,reinterpret_cast<void*>(&lookupPlayer));
    jump(0x4f5390,reinterpret_cast<void*>(&recordSpawn));
    *(uint8_t*)va(0x8d4545)=0xc3;
    *(void**)va(0x9052f8)=reinterpret_cast<void*>(&controlledRandom);
    *(void**)va(0x9052cc)=reinterpret_cast<void*>(&memmove);
    {
        Fixture fixture;
        automaticDrop(fixture.actor.data());
        check("original death update drops one part but does not mark the body looted",
            spawnedItems==1&&fixture.character[corpsePartsDroppedOffset]==1&&!fixture.character[corpseLootedOffset]);
        check("original corpse search selects the empty body",findCorpse(fixture.player.data())==fixture.actor.data());
        std::array<uint8_t,8> output{};
        check("original draw has nothing left to give",!draw(&fixture.dropTree.header,fixture.actor.data(),output.data()));
    }
    wchar_t temp[MAX_PATH], directory[MAX_PATH], settings[MAX_PATH];
    if(!GetTempPathW(MAX_PATH,temp)) return 3;
    swprintf_s(directory,L"%sunofficial_patch_empty_corpse_probe_%lu\\",temp,GetCurrentProcessId());
    swprintf_s(settings,L"%sconfig.ini",directory);
    if(!CreateDirectoryW(directory,nullptr)||!writeSettings(settings,false)) return 4;
    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?reinterpret_cast<wots4::Initialize>(GetProcAddress(mod,"Wots4ModInitialize")):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,claim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,reinterpret_cast<HMODULE>(imageBase),directory,&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) return 5;
    check("disabled fix installs nothing",!initialize(&context)&&claims.empty()&&*(uint8_t*)va(0x4a0560)==0x55);
    check("disabled fix is logged",loggedText("[empty_corpse_loot] disabled in config.ini"));
    writeSettings(settings,true);
    auto partsGuard=reinterpret_cast<uint8_t*>(va(0x4f5ad1));
    const auto originalGuard=*partsGuard; *partsGuard=0x27;
    check("changed part-state code refuses the fix before claiming",!initialize(&context)&&claims.empty());
    *partsGuard=originalGuard;
    rejectClaim=true;
    check("another Mod's reservation refuses the fix without changing the entry",
        !initialize(&context)&&claims.empty()&&*(uint8_t*)va(0x4a0560)==0x55&&*(uint8_t*)va(0x4f57f0)==0x55);
    rejectClaim=false;
    check("default-enabled fix initializes",initialize(&context)!=FALSE);
    check("both shared routines are reserved",claims.size()==2&&claimed(0x4a0560,6)&&claimed(0x4f57f0,6));
    check("successful installation is logged",loggedText("empty corpse loot installed at 0x4a0560 and 0x4f57f0"));
    for(int difficulty:{2,3}) for(int definition:{70,195,453}) {
        Fixture fixture(difficulty,true,definition);
        const auto inventory=fixture.inventory;
        automaticDrop(fixture.actor.data());
        check("part drop still spawns its original part",spawnedItems==1&&
            fixture.character[corpsePartsDroppedOffset]==1);
        check("search excludes the empty body and marks it looted",findCorpse(fixture.player.data())==nullptr&&
            fixture.character[corpseLootedOffset]==1);
        check("the patch adds no random draw or inventory consumption",randomCalls==2&&
            !memcmp(inventory.data(),fixture.inventory.data(),sizeof(inventory)));
        check("the empty body stays excluded",findCorpse(fixture.player.data())==nullptr);
        const auto lookups=playerLookups;
        automaticDrop(fixture.actor.data());
        check("later updates neither repeat the drop nor rebuild its candidates",
            randomCalls==2&&spawnedItems==1&&playerLookups==lookups);
    }
    for(int difficulty:{2,3}) {
        Fixture fixture(difficulty,false);
        automaticDrop(fixture.actor.data());
        check("an unowned style book keeps the body searchable",!fixture.character[corpseLootedOffset]&&
            findCorpse(fixture.player.data())==fixture.actor.data());
        randomValue=0; std::array<uint8_t,8> output{};
        check("the remaining style book is still obtainable",draw(&fixture.dropTree.header,fixture.actor.data(),output.data())&&
            field<int16_t>(output.data(),0)==476);
    }
    for(int difficulty:{0,1}) {
        Fixture fixture(difficulty);
        automaticDrop(fixture.actor.data());
        check("lower difficulties keep the corpse searchable",!fixture.character[corpseLootedOffset]&&
            findCorpse(fixture.player.data())==fixture.actor.data());
    }
    {
        Fixture fixture(2,true,196,7);
        automaticDrop(fixture.actor.data());
        check("other characters and drop tables also exclude empty bodies",findCorpse(fixture.player.data())==nullptr&&
            fixture.character[corpseLootedOffset]==1);
    }
    {
        Fixture fixture;
        fixture.character[corpsePartsDroppedOffset]=1;
        fixture.dropTree.header.parent=nullptr;
        automaticDrop(fixture.actor.data());
        check("a missing drop table is excluded from corpse search",findCorpse(fixture.player.data())==nullptr&&
            fixture.character[corpseLootedOffset]==1);
    }
    for(int difficulty:{0,1,2,3}) {
        Fixture fixture(difficulty,true,42,11);
        fixture.character[corpsePartsDroppedOffset]=1;
        field<void*>(fixture.character.data(),0x1f0)=fixture.inventory.data();
        check("an empty corpse is excluded at every difficulty",findCorpse(fixture.player.data())==nullptr&&
            fixture.character[corpseLootedOffset]==1&&randomCalls==0);
    }
    {
        Fixture fixture(2,true,42,11);
        fixture.character[corpsePartsDroppedOffset]=1;
        std::array<uint8_t,8> output{};
        check("a kick draw ends retries even before a corpse search",!draw(&fixture.dropTree.header,
            fixture.actor.data(),output.data())&&fixture.character[corpseLootedOffset]==1);
    }
    {
        Fixture fixture(2,true,42,11);
        fixture.character[corpsePartsDroppedOffset]=1;
        field<uint32_t>(fixture.character.data(),characterStateOffset)=0;
        std::array<uint8_t,8> output{};
        check("candidate preparation never marks a living character looted",!draw(&fixture.dropTree.header,
            fixture.actor.data(),output.data())&&!fixture.character[corpseLootedOffset]);
    }
    {
        Fixture fixture;
        fixture.character[corpsePartsDroppedOffset]=1;
        auto otherCharacter=fixture.character;
        auto otherActor=fixture.actor;
        field<void*>(otherActor.data(),0xc)=otherCharacter.data();
        field<int32_t>(otherCharacter.data(),4)=2;
        field<float>(otherCharacter.data(),0xdc)=2.0f;
        otherCharacter[corpsePartsDroppedOffset]=0;
        fixture.characterManager.characters.add(2,otherActor.data());
        check("search continues past the nearest empty corpse to one with loot",
            findCorpse(fixture.player.data())==otherActor.data()&&fixture.character[corpseLootedOffset]&&
            !otherCharacter[corpseLootedOffset]&&randomCalls==0);
    }
    for(unsigned location:{0u,1u}) {
        Fixture fixture(2,false,42,11);
        if(location==0) field<int16_t>(fixture.state.data(),0x48e4)=476;
        else {
            field<int16_t>(fixture.state.data(),0x4ac4)=476;
            field<int16_t>(fixture.state.data(),0x58d4)=1;
        }
        automaticDrop(fixture.actor.data());
        check("book ownership in carried inventory or storage excludes an exhausted corpse",
            findCorpse(fixture.player.data())==nullptr&&fixture.character[corpseLootedOffset]);
    }
    {
        Fixture fixture(0,true,42,11);
        fixture.character[corpsePartsDroppedOffset]=1;
        check("a different character's remaining consumable stays available",
            findCorpse(fixture.player.data())==fixture.actor.data()&&!fixture.character[corpseLootedOffset]);
    }
    {
        Fixture fixture;
        fixture.dropRecord[2+2*62]=1;
        randomValue=99;
        std::array<uint8_t,8> output{};
        check("an ordinary failed rate roll does not exhaust a body with candidates",
            !draw(&fixture.dropTree.header,fixture.actor.data(),output.data())&&!fixture.character[corpseLootedOffset]&&
            findCorpse(fixture.player.data())==fixture.actor.data());
    }
    {
        Fixture fixture;
        fixture.state[0x18]=0xff;
        check("an unsupported difficulty keeps the original search result",
            findCorpse(fixture.player.data())==fixture.actor.data()&&!fixture.character[corpseLootedOffset]);
    }
    {
        Fixture fixture;
        *(void**)va(0x9e9690)=nullptr;
        check("an unavailable item manager keeps the original search result",
            findCorpse(fixture.player.data())==fixture.actor.data()&&!fixture.character[corpseLootedOffset]);
    }
    {
        Fixture fixture(2,true,42,1);
        fixture.character[corpsePartsDroppedOffset]=1;
        field<unsigned>(fixture.definitions.data(),12+42*4)=0;
        check("missing definitions use the same default table as the original draw",
            findCorpse(fixture.player.data())==nullptr&&fixture.character[corpseLootedOffset]);
    }
    {
        Fixture fixture;
        field<int16_t>(fixture.definitions.data(),0xc80+0x26)=-1;
        check("a definition with no drop table is excluded from search",
            findCorpse(fixture.player.data())==nullptr&&fixture.character[corpseLootedOffset]);
    }
    DeleteFileW(settings); RemoveDirectoryW(directory);
    printf("RESULT tests=%u failures=%u; exe copy and synthetic characters only\n",tests,failures);
    puts("Game installation, saves and game processes were not modified.");
    return failures?1:0;
}
