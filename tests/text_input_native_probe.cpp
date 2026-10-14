// Loads mod.dll into a copy of the exe mapped into this process and runs the game's shared text
// input poll (0x4f4b10, used for the sect name and weapon names) with typed characters in the
// game's character queue, a fake input system and a fake mouse and UI state. Never attaches to
// the game process or reads saves.
#include "mapped_exe.h"

using namespace mapped_exe;

namespace {

constexpr unsigned pollTextInput = 0x4f4b10;
constexpr unsigned inputSystemGlobal = 0x9e968c;
constexpr unsigned uiStateGlobal = 0x9ecf30;
constexpr unsigned characterQueue = 0x9e96d8;  // 512 UTF-16 code units
constexpr unsigned queueWriteIndex = 0x9ecf84;
constexpr unsigned queueReadIndex = 0x9ecf88;
constexpr unsigned isalnumImport = 0x905330;
constexpr unsigned wcscatImport = 0x905334;
constexpr unsigned cursorReportImport = 0x905300;  // Called by 0x4f4960 when the mouse is not usable

constexpr unsigned keyboardMappingOffset = 0x20;
constexpr unsigned pausedButtonsOffset = 0x21;  // Nonzero: the poll reads no pressed buttons (0x4f4b45)

// Text input engine (0x4f4ab0): +0 cursor blink time, +4 buffer length, +0xc text, +0x100c blink timer.
struct Engine {
    float blinkTime;
    unsigned bufferLength;
    unsigned unused;
    wchar_t text[0x400];
    wchar_t original[0x400];
    float blinkTimer;
};
static_assert(offsetof(Engine,text)==0xc&&offsetof(Engine,original)==0x80c&&offsetof(Engine,blinkTimer)==0x100c,
    "Unexpected text input engine layout");

unsigned char inputSystem[0x100];
unsigned char inputVtable[0x100];
unsigned char mouseState[0x10];
unsigned char uiState[0x2000];
Engine engine;
int mappingDuringPoll=-1;

// 0x4f4960 reports through this import and gives up on the mouse, so no cursor or window is read.
int __cdecl reportCursor(const char*) {
    mappingDuringPoll=inputSystem[keyboardMappingOffset];
    return 0;
}

wots4::GameEventCallback frameCallback;
void* frameContext;
BOOL WINAPI hostOnFrame(HMODULE, wots4::GameEventCallback callback, void* context) {
    frameCallback=callback;
    frameContext=context;
    return TRUE;
}

template<class T> T& game(unsigned address) { return *reinterpret_cast<T*>(va(address)); }

void setUpGame() {
    *reinterpret_cast<unsigned char**>(inputSystem)=inputVtable;
    *reinterpret_cast<unsigned char**>(inputVtable+0x40)=mouseState;
    *reinterpret_cast<unsigned*>(mouseState+0xc)=1;
    inputSystem[pausedButtonsOffset]=1;
    game<unsigned char*>(inputSystemGlobal)=inputSystem;
    game<unsigned char*>(uiStateGlobal)=uiState;
    errno_t (__cdecl* appendText)(wchar_t*, rsize_t, const wchar_t*)=wcscat_s;
    game<const void*>(wcscatImport)=reinterpret_cast<const void*>(appendText);
    game<const void*>(cursorReportImport)=reinterpret_cast<const void*>(&reportCursor);
}

void resetEngine() {
    memset(&engine,0,sizeof(engine));
    engine.blinkTime=100.0f;
    engine.blinkTimer=100.0f;
    engine.bufferLength=32;
}

void type(const wchar_t* characters) {
    for(auto c=characters;*c;++c) {
        unsigned& write=game<unsigned>(queueWriteIndex);
        game<wchar_t[512]>(characterQueue)[write]=*c;
        write=(write+1)%512;
    }
}

bool poll() {
    mappingDuringPoll=-1;
    return reinterpret_cast<bool (__stdcall*)(Engine*)>(va(pollTextInput))(&engine);
}

// While the cursor blinks on, the poll ends the text with '_' (0x4f4ee4) and removes it at the next poll.
std::wstring typedText() {
    std::wstring text=engine.text;
    if(!text.empty()&&text.back()==L'_')
        text.pop_back();
    return text;
}

void runFrame() { frameCallback(frameContext); }

// The game's own C runtime under the user's locale, as the game sets it (setlocale(LC_ALL, "")).
bool useGameRuntimeIsalnum() {
    auto runtime=LoadLibraryA("msvcr100.dll");
    if(!runtime) return false;
    auto setLocale=reinterpret_cast<char* (__cdecl*)(int,const char*)>(GetProcAddress(runtime,"setlocale"));
    auto isAlnum=GetProcAddress(runtime,"isalnum");
    if(!setLocale||!isAlnum) return false;
    setLocale(0,"");
    game<const void*>(isalnumImport)=reinterpret_cast<const void*>(isAlnum);
    return true;
}

}  // namespace

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    setUpGame();

    // Without the Mod: the shared text input drops some Japanese characters (needs the Japanese code page).
    if(GetACP()==932&&useGameRuntimeIsalnum()) {
        resetEngine();
        type(L"a\x4f8d\x5200\x30fc\x6b66");  // a侍刀ー武
        inputSystem[keyboardMappingOffset]=1;
        poll();
        check("without the Mod, 侍・刀・ー are dropped and a・武 are taken (cp932)",typedText()==L"a\x6b66");
        check("without the Mod, the keyboard buttons stay on while typing",mappingDuringPoll==1);
    } else {
        printf("SKIP without-the-Mod checks: needs code page 932 and msvcr100.dll\n");
    }

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease,nullptr,hostOnFrame};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("the Mod initializes on the known exe",initialize(&context)!=FALSE);
    check("the three text input ranges are claimed",
        claimed(0x4f4b10,6)&&claimed(0x4f4be7,6)&&claimed(0x5f7607,6));
    check("the frame callback is registered",frameCallback!=nullptr);
    check("the three fixes are logged as installed",
        loggedText("text input keyboard buttons installed at 0x4f4b10")&&
        loggedText("text input characters installed at 0x4f4be7")&&
        loggedText("player name characters installed at 0x5f7607"));

    auto call=&game<unsigned char>(0x4f4be7);
    auto callTarget=va(0x4f4be7)+5+*reinterpret_cast<const int*>(call+1);
    auto modBase=reinterpret_cast<unsigned>(mod);
    check("isalnum at 0x4f4be7 is replaced by a call into mod.dll",
        call[0]==0xe8&&call[5]==0x90&&callTarget>modBase&&
        callTarget<modBase+((IMAGE_NT_HEADERS32*)(modBase+((IMAGE_DOS_HEADER*)modBase)->e_lfanew))->OptionalHeader.SizeOfImage);
    const unsigned char jumpIfZero[]={0x66,0x85,0xc0,0x0f,0x84,0x71,0x01,0x00,0x00,0x83,0xfe,0x1f};
    check("the player name input skips only 0 (jle became je, same target)",
        !memcmp(&game<unsigned char>(0x5f7604),jumpIfZero,sizeof(jumpIfZero)));

    // Characters
    resetEngine();
    inputSystem[keyboardMappingOffset]=1;
    type(L"a\x4f8d\x5200\x30fc\x6b66\x9b3c\xff21\xff11\xff71\x3000 Z9-_!\xe010\xfffd");
    bool finished=poll();
    check("Japanese, fullwidth and halfwidth kana are taken; ASCII stays letters, digits and space",
        !finished&&typedText()==std::wstring(L"a\x4f8d\x5200\x30fc\x6b66\x9b3c\xff21\xff11\xff71\x3000 Z9"));
    check("the character queue is emptied",game<unsigned>(queueReadIndex)==game<unsigned>(queueWriteIndex));

    // Keyboard buttons
    check("the keyboard buttons are off during the poll",mappingDuringPoll==0);
    check("an unfinished poll leaves them off",inputSystem[keyboardMappingOffset]==0);
    runFrame();
    check("a frame with a poll leaves them off",inputSystem[keyboardMappingOffset]==0);
    logged.clear();
    runFrame();
    check("a frame without a poll turns them on again",inputSystem[keyboardMappingOffset]==1&&
        loggedText("text input was closed without finishing; keyboard keys act as buttons again"));

    poll();
    check("the next poll turns them off again",inputSystem[keyboardMappingOffset]==0);
    type(L"\r");
    finished=poll();
    check("Enter accepts the text through the game's own code",finished);
    check("finishing turns the keyboard buttons on again at once",inputSystem[keyboardMappingOffset]==1);
    logged.clear();
    runFrame();
    runFrame();
    check("frames after a finished input change nothing and log nothing",
        inputSystem[keyboardMappingOffset]==1&&logged.empty());

    resetEngine();
    inputSystem[keyboardMappingOffset]=0;  // As the player name input leaves it
    poll();
    runFrame();
    runFrame();
    check("buttons turned off by the player name input are left off",inputSystem[keyboardMappingOffset]==0);

    resetEngine();
    inputSystem[keyboardMappingOffset]=1;
    type(L"\x1b");
    finished=poll();
    check("Esc still cancels, and the keyboard buttons are on again",finished&&inputSystem[keyboardMappingOffset]==1);

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only\n",tests,failures);
    return failures?1:0;
}
