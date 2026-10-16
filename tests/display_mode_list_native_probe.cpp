// Installs mod.dll on a copy of the exe mapped into this process and fills the game's
// resolution list (0x536a90) with and without the Mod. Its imports are replaced on the copy:
// EnumDisplaySettingsA lists synthetic modes in 8, 16 and 32 bits like Windows does for old
// programs, ChangeDisplaySettingsA only records the test (the display mode is never changed),
// and memset is the test's own. Never attaches to the game process or reads saves.
#include "mapped_exe.h"

using namespace mapped_exe;

constexpr unsigned fillResolutionList = 0x536a90;
constexpr unsigned listedModeCount = 0x9ecf8c;
constexpr unsigned listedModes = 0x9e9ad8;
constexpr unsigned listCapacity = 31;  // cmp edx,0x1f (0x536b5b)

struct Mode { DWORD bitsPerPixel, width, height, frequency; };
// 3840x2160 at 144 Hz fails the test, as a mode the monitor cannot show would.
constexpr Mode failingMode{32,3840,2160,144};
static std::vector<Mode> modes;
static void listModes() {
    const Mode sizes[] = {{0,640,480,60},{0,640,480,144},{0,800,600,60},{0,1024,768,144},{0,1280,720,59},
        {0,1920,1080,1},{0,1920,1080,60},{0,1920,1080,144},{0,2560,1440,120},{0,3840,2160,60},{0,3840,2160,144},
        {0,320,200,70}};
    for(DWORD bits:{8u,16u,32u})
        for(auto size:sizes)
            modes.push_back({bits,size.width,size.height,size.frequency});
}

static BOOL WINAPI fakeEnumDisplaySettings(const char*, DWORD index, DEVMODEA* mode) {
    if(index>=modes.size()) return FALSE;
    mode->dmFields=DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT|DM_DISPLAYFREQUENCY;
    mode->dmBitsPerPel=modes[index].bitsPerPixel;
    mode->dmPelsWidth=modes[index].width;
    mode->dmPelsHeight=modes[index].height;
    mode->dmDisplayFrequency=modes[index].frequency;
    return TRUE;
}

static std::vector<Mode> testedModes;
static std::vector<DWORD> testFlags;
static LONG WINAPI recordChangeDisplaySettings(DEVMODEA* mode, DWORD flags) {
    Mode tested{mode->dmBitsPerPel,mode->dmPelsWidth,mode->dmPelsHeight,mode->dmDisplayFrequency};
    testedModes.push_back(tested);
    testFlags.push_back(flags);
    bool fails=tested.bitsPerPixel==failingMode.bitsPerPixel&&tested.width==failingMode.width&&
        tested.frequency==failingMode.frequency;
    return fails?DISP_CHANGE_BADMODE:DISP_CHANGE_SUCCESSFUL;
}

// The list as the game keeps it: the count and each entry's (bits, width, height, frequency).
static std::vector<Mode> fillList() {
    *(int*)va(listedModeCount)=0;
    memset((void*)va(listedModes),0,listCapacity*sizeof(DEVMODEA));
    testedModes.clear();
    testFlags.clear();
    auto function=(void (__cdecl*)())va(fillResolutionList);
    function();
    std::vector<Mode> list;
    auto entries=(const DEVMODEA*)va(listedModes);
    for(int i=0;i<*(int*)va(listedModeCount);++i)
        list.push_back({entries[i].dmBitsPerPel,entries[i].dmPelsWidth,entries[i].dmPelsHeight,
            entries[i].dmDisplayFrequency});
    return list;
}

static bool sameModes(const std::vector<Mode>& a, const std::vector<Mode>& b) {
    return a.size()==b.size()&&!memcmp(a.data(),b.data(),a.size()*sizeof(Mode));
}

static bool onlyTests() {
    for(auto flags:testFlags) if(flags!=CDS_TEST) return false;
    return true;
}

static size_t testedWithBits(DWORD bits) {
    size_t count=0;
    for(auto& mode:testedModes) count+=mode.bitsPerPixel==bits;
    return count;
}

static bool failingModeTested() {
    for(auto& mode:testedModes)
        if(mode.bitsPerPixel==failingMode.bitsPerPixel&&mode.width==failingMode.width&&
            mode.frequency==failingMode.frequency) return true;
    return false;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    listModes();
    *(void**)va(0x905410)=(void*)&fakeEnumDisplaySettings;
    *(void**)va(0x90547c)=(void*)&recordChangeDisplaySettings;
    *(void**)va(0x905228)=(void*)&memset;  // msvcr memset, through the jump at 0x8d4b06

    auto withoutMod=fillList();
    check("without the Mod, every listed mode is tested (36 modes in 8, 16 and 32 bits)",
        testedModes.size()==modes.size()&&onlyTests());
    check("without the Mod, the list has one entry per width of 640 or more (6 widths)",
        withoutMod.size()==6);
    bool only32=true;
    for(auto& mode:withoutMod) only32&=mode.bitsPerPixel==32;
    check("without the Mod, the list has only 32-bit modes",only32);

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,L".\\",&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }
    check("the Mod initializes",initialize(&context)!=FALSE);
    check("0x536af9 is claimed (6 bytes)",claimed(0x536af9,6));
    check("a direct call is written at 0x536af9",*(unsigned char*)va(0x536af9)==0xe8&&
        *(unsigned char*)va(0x536afe)==0x90);
    check("the installation is logged",loggedText("display mode list installed at 0x536af9"));

    auto withMod=fillList();
    check("with the Mod, the list is the same as without it",sameModes(withMod,withoutMod));
    check("with the Mod, no 8-bit or 16-bit mode is tested",testedWithBits(8)==0&&testedWithBits(16)==0);
    check("with the Mod, only the 32-bit modes of 640 pixels and 60 Hz or more are tested (9 modes)",
        testedModes.size()==9&&onlyTests());
    check("with the Mod, a 32-bit mode that fails the test is still tested and left out",
        failingModeTested());

    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only, display mode not changed\n",
        tests,failures);
    return failures?1:0;
}
