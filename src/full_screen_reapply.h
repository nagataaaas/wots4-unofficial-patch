#pragma once
// On every WM_ACTIVATEAPP the game applies full screen again (0x538180): it
// changes the display mode, sizes its window to the settings' size and then to
// the screen size, and resets the Direct3D device. While that runs, the window
// is briefly larger than the screen, shows its white background, and the game
// stops. When nothing it would set has changed, the call can be skipped: a
// device that was really lost is still reset by the renderer at the start of the
// next frame (0x893d00 -> 0x891f80, TestCooperativeLevel).
namespace unofficial_patch {

struct FullScreenState {
    bool requestedFullScreen;          // The argument of 0x538180
    bool alreadyFullScreen;            // window+0xec
    int wantedWidth, wantedHeight;     // The settings' size ([0x9d30d4], [0x9d30d8])
    int physicalWidth, physicalHeight; // The primary screen in physical pixels, 0 if unknown
    bool minimized;
    bool popupStyle;                   // WS_POPUP without a caption or a sizing border
    int windowLeft, windowTop, windowRight, windowBottom;  // GetWindowRect, in the window's coordinates
    int screenWidth, screenHeight;     // The primary screen in the window's coordinates
};

// Why full screen must be applied again, or nullptr when the call can be skipped.
inline const char* fullScreenReapplyReason(const FullScreenState& state) {
    if(!state.requestedFullScreen)
        return "the game switches to windowed mode";
    if(!state.alreadyFullScreen)
        return "the window is not in full screen yet";
    if(!state.physicalWidth||!state.physicalHeight)
        return "the physical screen size is unknown";
    if(state.wantedWidth!=state.physicalWidth||state.wantedHeight!=state.physicalHeight)
        return "the display mode differs from the settings";
    if(state.minimized)
        return "the window is minimized";
    if(!state.popupStyle)
        return "the window has a frame";
    if(state.windowLeft!=0||state.windowTop!=0||state.windowRight!=state.screenWidth||
        state.windowBottom!=state.screenHeight)
        return "the window does not cover the screen";
    return nullptr;
}

}  // namespace unofficial_patch
