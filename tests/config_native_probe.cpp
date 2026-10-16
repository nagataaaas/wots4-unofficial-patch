// Installs mod.dll on a copy of the exe mapped into this process with a
// config.ini written to a temporary folder, and checks which fixes are
// installed. Never attaches to the game process or reads saves.
#include "mapped_exe.h"

using namespace mapped_exe;

static wchar_t modDirectory[MAX_PATH];
static wchar_t configPath[MAX_PATH];

static bool writeConfig(const char* text) {
    FILE* file=nullptr;
    if(_wfopen_s(&file,configPath,L"wb")||!file) return false;
    fputs(text,file);
    fclose(file);
    return true;
}

int main(int argc, char** argv) {
    setvbuf(stdout,nullptr,_IONBF,0);
    if(argc!=3) return 2;
    if(!mapImage(argv[1])) return 3;
    wchar_t temp[MAX_PATH];
    if(!GetTempPathW(MAX_PATH,temp)) return 4;
    swprintf_s(modDirectory,L"%sunofficial_patch_config_probe_%lu\\",temp,GetCurrentProcessId());
    swprintf_s(configPath,L"%sconfig.ini",modDirectory);
    if(!CreateDirectoryW(modDirectory,nullptr)) return 5;
    const unsigned char originalRelease[]={0x53,0x56,0x57,0x8b,0xf9};

    auto mod=LoadLibraryA(argv[2]);
    auto initialize=mod?(wots4::Initialize)GetProcAddress(mod,"Wots4ModInitialize"):nullptr;
    wots4::HostApi host{sizeof(host),wots4::modApiVersion,hostLog,hostClaim,hostRelease};
    wots4::ModContext context{sizeof(context),wots4::modApiVersion,nullptr,mod,(HMODULE)imageBase,modDirectory,&host};
    check("mod.dll exports Wots4ModInitialize",initialize!=nullptr);
    if(!initialize) { printf("RESULT tests=%u failures=%u\n",tests,failures+1); return 1; }

    writeConfig("[follow_detach]\r\nenabled=0\r\n[property_return]\r\nenabled=0\r\n[translucent_depth]\r\nenabled=0\r\n"
        "[physical_screen_check]\r\nenabled=0\r\n[skip_full_screen_reapply]\r\nenabled=0\r\n"
        "[no_white_background]\r\nenabled=0\r\n[yobai_result_reset]\r\nenabled=0\r\n"
        "[listener_orientation]\r\nenabled=0\r\n[fullwidth_digits]\r\nenabled=0\r\n"
        "[text_input_keyboard]\r\nenabled=0\r\n[text_input_characters]\r\nenabled=0\r\n"
        "[player_name_characters]\r\nenabled=0\r\n[sound_slot_release]\r\nenabled=0\r\n"
        "[title_select_window]\r\nenabled=0\r\n[display_mode_list]\r\nenabled=0\r\n");
    check("every fix disabled: the Mod refuses to load",!initialize(&context));
    check("every fix disabled: nothing is claimed or written",
        claims.empty()&&!memcmp((void*)va(0x48d520),originalRelease,sizeof(originalRelease)));
    check("every disabled fix is logged",loggedText("[follow_detach] disabled in config.ini (enabled=0); not installed")&&
        loggedText("[property_return] disabled in config.ini")&&loggedText("[translucent_depth] disabled in config.ini")&&
        loggedText("[physical_screen_check] disabled in config.ini")&&
        loggedText("[skip_full_screen_reapply] disabled in config.ini")&&
        loggedText("[no_white_background] disabled in config.ini")&&
        loggedText("[yobai_result_reset] disabled in config.ini")&&
        loggedText("[listener_orientation] disabled in config.ini")&&
        loggedText("[fullwidth_digits] disabled in config.ini")&&
        loggedText("[text_input_keyboard] disabled in config.ini")&&
        loggedText("[text_input_characters] disabled in config.ini")&&
        loggedText("[player_name_characters] disabled in config.ini")&&
        loggedText("[sound_slot_release] disabled in config.ini")&&
        loggedText("[title_select_window] disabled in config.ini")&&
        loggedText("[display_mode_list] disabled in config.ini"));
    check("the text input and the player name input are left unchanged",
        *(unsigned char*)va(0x4f4b10)==0x55&&*(unsigned char*)va(0x4f4be7)==0xff&&*(unsigned char*)va(0x5f7608)==0x8e);
    check("font loading and numeric key conversion are left unchanged",
        *(unsigned char*)va(0x4c5530)==0x55&&*(unsigned char*)va(0x68a330)==0xb8);
    check("the display mode check is left unchanged",*(unsigned char*)va(0x5383d0)==0x55);
    check("the 3D sound listener is left unchanged",*(unsigned char*)va(0x6a2660)==0x55);
    check("the sound stop and slot pick are left unchanged",
        *(unsigned char*)va(0x5a1210)==0x55&&*(unsigned char*)va(0x5a19e0)==0x55);
    check("the save data load is left unchanged",*(unsigned char*)va(0x4d9f50)==0x55);
    check("the title's Options state is left unchanged",*(unsigned char*)va(0x4ee380)==0x55);
    check("the depth mask is not set up when disabled",!loggedText("translucent depth mask"));

    logged.clear();
    writeConfig("[follow_detach]\r\nenabled=0\r\n[property_return]\r\nenabled=yes\r\n[translucent_depth]\r\nenabled=0\r\n"
        "[physical_screen_check]\r\nenabled=0\r\n[skip_full_screen_reapply]\r\nenabled=0\r\n"
        "[no_white_background]\r\nenabled=0\r\n[yobai_result_reset]\r\nenabled=0\r\n"
        "[listener_orientation]\r\nenabled=0\r\n[fullwidth_digits]\r\nenabled=0\r\n"
        "[text_input_keyboard]\r\nenabled=0\r\n[text_input_characters]\r\nenabled=0\r\n"
        "[player_name_characters]\r\nenabled=0\r\n[sound_slot_release]\r\nenabled=0\r\n"
        "[title_select_window]\r\nenabled=0\r\n[display_mode_list]\r\nenabled=0\r\n");
    check("one fix enabled: the Mod initializes",initialize(&context)!=FALSE);
    check("only the enabled fix is claimed (0x55e140)",claims.size()==1&&claimed(0x55e140,5));
    check("the disabled fix leaves 0x48d520 unchanged",
        !memcmp((void*)va(0x48d520),originalRelease,sizeof(originalRelease)));
    check("an invalid value is logged and treated as enabled",
        loggedText("config.ini: [property_return] enabled=yes is not a valid value (use 1 or 0); using 1")&&
        loggedText("property return after Madara's rescue installed at 0x55e140"));
    check("the depth mask is still not set up",!loggedText("translucent depth mask"));

    DeleteFileW(configPath);
    RemoveDirectoryW(modDirectory);
    printf("RESULT tests=%u failures=%u; mapped exe copy in this process only, config.ini in %%TEMP%% (removed)\n",
        tests,failures);
    return failures?1:0;
}
