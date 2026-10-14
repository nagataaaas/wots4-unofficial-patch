// Installs mod.dll on the mapped exe copy. Only font registration and the shared
// key converter run; allocation is local and no textures, GPU or game process are used.
#include <array>
#include <cstdlib>
#include "mapped_exe.h"
#include "fullwidth_digits.h"

using namespace mapped_exe;
using namespace unofficial_patch;

struct Font {
    FontDatabase header{21,32,2,0};
    FontGlyph glyphs[21]{};
};
static Font makeFont() {
    Font font;
    for(unsigned digit=0;digit<digitCount;++digit) {
        font.glyphs[digit]={static_cast<uint16_t>(fullwidthDigitFirst+digit),
            static_cast<uint16_t>(300+digit*20),110,19,0};
        font.glyphs[digit+10]={static_cast<uint16_t>(fullwidthDigitFirst+digit),
            static_cast<uint16_t>(digit==0?480:192+(digit-1)*32),32,32,1};
    }
    font.glyphs[20]={L'2',193,2,16,0};
    return font;
}

static void jumpTo(unsigned address, const void* function) {
    auto bytes=reinterpret_cast<unsigned char*>(va(address));
    bytes[0]=0xe9;
    *reinterpret_cast<int32_t*>(bytes+1)=reinterpret_cast<unsigned>(function)-(va(address)+5);
}
static void* __cdecl allocateFont(size_t bytes, unsigned) { return malloc(bytes); }
static bool rejectKeyRange;
static BOOL WINAPI claimRange(HMODULE owner, uintptr_t address, uint32_t length) {
    return rejectKeyRange&&address==va(0x68a330)?FALSE:hostClaim(owner,address,length);
}

static uint16_t convertKey(unsigned scanCode) {
    auto function=va(0x68a330);
    unsigned result;
    __asm {
        mov edx, scanCode
        call function
        mov result, eax
    }
    return static_cast<uint16_t>(result);
}

static unsigned assignedScanCode;
static __declspec(naked) void assignedActionKey() {
    __asm {
        mov eax, assignedScanCode
        ret
    }
}
static void convertTutorial(wchar_t* text) {
    auto function=va(0x68a370);
    __asm {
        push ebx
        mov ebx, text
        call function
        pop ebx
    }
}

struct KeyRegisters { unsigned code, ecx, edx, ebx, esi, flags; };
static KeyRegisters convertKeyRegisters(unsigned scanCode) {
    auto function=va(0x68a330);
    KeyRegisters returned{};
    __asm {
        push ebx
        push esi
        mov ebx, 0x12345678
        mov esi, 0x87654321
        mov edx, scanCode
        call function
        mov returned.code, eax
        mov returned.ecx, ecx
        mov returned.edx, edx
        mov returned.ebx, ebx
        mov returned.esi, esi
        pushfd
        pop returned.flags
        pop esi
        pop ebx
    }
    return returned;
}

struct FontState { FontDatabase* database; FontGlyph** lookup; unsigned state; unsigned fields[21]{}; };
struct Returned { unsigned eax, esi, stackBefore, stackAfter; };
static Returned prepare(FontState& state) {
    auto function=va(0x4c5530);
    auto argument=&state;
    Returned returned{};
    __asm {
        push esi
        mov returned.stackBefore, esp
        mov esi, argument
        call function
        mov returned.eax, eax
        mov returned.esi, esi
        mov returned.stackAfter, esp
        pop esi
    }
    return returned;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    wchar_t temporary[MAX_PATH], directory[MAX_PATH], config[MAX_PATH];
    if(!GetTempPathW(MAX_PATH,temporary)) return 4;
    swprintf_s(directory,L"%sfullwidth_digits_probe_%lu\\",temporary,GetCurrentProcessId());
    swprintf_s(config,L"%sconfig.ini",directory);
    if(!CreateDirectoryW(directory,nullptr)) return 5;
    FILE* file=nullptr;
    if(_wfopen_s(&file,config,L"wb")||!file) return 6;
    fputs("[follow_detach]\r\nenabled=0\r\n[property_return]\r\nenabled=0\r\n"
        "[translucent_depth]\r\nenabled=0\r\n[physical_screen_check]\r\nenabled=0\r\n"
        "[skip_full_screen_reapply]\r\nenabled=0\r\n[no_white_background]\r\nenabled=0\r\n"
        "[yobai_result_reset]\r\nenabled=0\r\n[listener_orientation]\r\nenabled=0\r\n"
        "[text_input_keyboard]\r\nenabled=0\r\n[text_input_characters]\r\nenabled=0\r\n"
        "[player_name_characters]\r\nenabled=0\r\n[sound_slot_release]\r\nenabled=0\r\n"
        "[title_select_window]\r\nenabled=0\r\n",file);
    fclose(file);

    // Keep the game's registration loop and its state/return handling. Skip texture construction
    // after the loop, and replace only its allocator and memset with local CRT functions.
    jumpTo(0x6c91a0,reinterpret_cast<const void*>(&allocateFont));
    jumpTo(0x8d4b06,reinterpret_cast<const void*>(&memset));
    jumpTo(0x4c5645,reinterpret_cast<const void*>(va(0x4c569f)));
    *reinterpret_cast<unsigned char*>(va(0x8d4545))=0xc3;
    *reinterpret_cast<unsigned*>(va(0x9ed018))=0;

    auto originalFont=makeFont();
    FontState originalState{&originalFont.header,nullptr,3};
    prepare(originalState);
    check("without the Mod fullwidth two selects the numeric key image",
        originalState.lookup[0xff12]==&originalFont.glyphs[12]);
    check("without the Mod the 2 scan code becomes fullwidth two",convertKey(3)==0xff12);
    free(originalState.lookup);
    std::array<unsigned,98> scanCodes{}, originalCodes{};
    std::array<KeyRegisters,98> originalRegisters{};
    auto keyEntries=reinterpret_cast<const unsigned*>(va(0x99c948));
    for(unsigned index=0;index<scanCodes.size();++index) {
        scanCodes[index]=keyEntries[index*2];
        originalCodes[index]=keyEntries[index*2+1];
        originalRegisters[index]=convertKeyRegisters(scanCodes[index]);
    }

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,claimRange,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,directory,&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) return 7;
    auto tableCode=reinterpret_cast<unsigned*>(va(0x99c95c));
    *tableCode=0xff13;
    check("an unexpected key mapping refuses the fix without hooks",!initialize(&context)&&claims.empty()&&
        *reinterpret_cast<unsigned char*>(va(0x4c5530))==0x55&&
        *reinterpret_cast<unsigned char*>(va(0x68a330))==0xb8);
    *tableCode=0xff12;
    rejectKeyRange=true;
    check("a conflict on the key hook leaves both functions untouched",!initialize(&context)&&claims.empty()&&
        *reinterpret_cast<unsigned char*>(va(0x4c5530))==0x55&&
        *reinterpret_cast<unsigned char*>(va(0x68a330))==0xb8);
    rejectKeyRange=false;
    check("the font and key hooks initialize together",initialize(&context)!=FALSE);
    check("only the two hook ranges are claimed",claims.size()==2&&claimed(0x4c5530,5)&&claimed(0x68a330,5));
    check("installation leaves the conversion table untouched",*tableCode==0xff12);
    for(unsigned index=0;index<scanCodes.size();++index)
        check("key conversion stays original before the font is ready",convertKey(scanCodes[index])==originalCodes[index]);

    auto font=makeFont();
    auto before=font;
    FontState state{&font.header,nullptr,2};
    prepare(state);
    check("waiting for the file leaves every record unchanged",!memcmp(&font,&before,sizeof(font))&&state.state==2);
    state.state=3;
    auto returned=prepare(state);
    check("font registration reaches state four and retains its false return",state.state==4&&(returned.eax&0xff)==0);
    check("the font hook retains esi and the caller's stack",returned.esi==reinterpret_cast<unsigned>(&state)&&
        returned.stackBefore==returned.stackAfter);
    for(unsigned digit=0;digit<digitCount;++digit) {
        check("a fullwidth digit resolves to the original normal glyph",
            state.lookup[fullwidthDigitFirst+digit]==&font.glyphs[digit]);
        auto key=state.lookup[privateDigitFirst+digit];
        auto expected=before.glyphs[digit+10]; expected.codepoint=privateDigitFirst+digit;
        check("a numeric key resolves to the same key image and width under its private code",
            key&&memcmp(key,&expected,sizeof(expected))==0);
    }
    for(unsigned index=0;index<scanCodes.size();++index) {
        auto code=originalCodes[index];
        auto expected=code>=fullwidthDigitFirst&&code<fullwidthDigitFirst+digitCount
            ? privateDigitFirst+code-fullwidthDigitFirst : code;
        check("reassignment and tutorial conversion remap only the numeric keys",convertKey(scanCodes[index])==expected);
    }
    bool registersKept=true;
    for(unsigned index=0;index<scanCodes.size();++index) {
        auto after=convertKeyRegisters(scanCodes[index]);
        auto& before=originalRegisters[index];
        registersKept&=after.ecx==before.ecx&&after.edx==before.edx&&after.ebx==before.ebx&&
            after.esi==before.esi&&after.flags==before.flags;
    }
    check("all key conversions retain the original registers and flags",registersKept);
    check("an unregistered key still returns the sentinel",convertKey(0x10000)==0xffff);
    check("ASCII two remains on the normal page",state.lookup[L'2']==&font.glyphs[20]);
    // Run the actual tutorial token preprocessing. Only the action-to-assigned-key query
    // is replaced; action tokens, literal digits, and the shared converter run unchanged.
    unsigned inputDevice[4]={0,0,0,1};
    void* devices[]={inputDevice};
    unsigned inputManager[7]{};
    inputManager[5]=reinterpret_cast<unsigned>(devices);
    inputManager[6]=reinterpret_cast<unsigned>(devices+1);
    *reinterpret_cast<void**>(va(0x9e968c))=inputManager;
    jumpTo(0x4f2ab0,reinterpret_cast<const void*>(&assignedActionKey));
    for(unsigned digit=0;digit<digitCount;++digit) {
        assignedScanCode=digit==0?11:digit+1;
        wchar_t tutorial[]={0x2460,L' ',0xff12,0};
        convertTutorial(tutorial);
        check("tutorial action token follows a numeric key reassignment while literal two stays normal",
            tutorial[0]==privateDigitFirst+digit&&tutorial[2]==fullwidthDigitFirst+2&&
            state.lookup[tutorial[0]]==&font.glyphs[digit+10]&&state.lookup[tutorial[2]]==&font.glyphs[2]);
    }
    inputDevice[3]=0;
    wchar_t controllerText[]={0x2460,L' ',0xff12,0};
    convertTutorial(controllerText);
    check("controller tutorial action tokens and literal digits are unchanged",
        controllerText[0]==0x2460&&controllerText[2]==0xff12);
    state.state=6;
    returned=prepare(state);
    check("an already prepared font retains its true return",(returned.eax&0xff)==1);
    check("the successful separation is logged",loggedText("separated 10 numeric key images"));

    auto conflicting=makeFont(); conflicting.glyphs[20].codepoint=privateDigitFirst;
    auto conflictingBefore=conflicting;
    FontState conflictingState{&conflicting.header,nullptr,3};
    prepare(conflictingState);
    check("a private code conflict retains the loaded font records",
        !memcmp(&conflicting,&conflictingBefore,sizeof(conflicting)));
    check("a rejected reload restores original numeric key conversion",convertKey(3)==0xff12);
    check("reload fallback retains a private glyph owned by the replacement font",
        conflictingState.lookup[privateDigitFirst]==&conflicting.glyphs[20]);
    check("reload fallback keeps previously converted key strings resolvable",
        conflictingState.lookup[privateDigitFirst+2]==conflictingState.lookup[fullwidthDigitFirst+2]);
    check("a rejected font load is logged",loggedText("original glyphs and key conversion are kept"));
    inputDevice[3]=1;
    assignedScanCode=3;
    wchar_t fallbackTutorial[]={0x2460,L' ',0xff12,0};
    convertTutorial(fallbackTutorial);
    check("an unsupported reload keeps the original tutorial key conversion",fallbackTutorial[0]==0xff12);

    auto reloaded=makeFont(); FontState reloadedState{&reloaded.header,nullptr,3};
    prepare(reloadedState);
    check("a supported font reload restores normal digits and numeric key images",
        convertKey(3)==privateDigitFirst+2&&reloadedState.lookup[fullwidthDigitFirst+2]==&reloaded.glyphs[2]&&
        reloadedState.lookup[privateDigitFirst+2]==&reloaded.glyphs[12]);
    free(state.lookup); free(conflictingState.lookup); free(reloadedState.lookup);
    DeleteFileW(config); RemoveDirectoryW(directory);
    printf("RESULT tests=%u failures=%u; copied exe and synthetic fonts in this process only, no game started\n",tests,failures);
    return failures?1:0;
}
