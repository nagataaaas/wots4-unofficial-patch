# White screen and freeze when returning to the window

- Status: fixes 1 and 3 confirmed in game (returning is much faster, no white flash); fix 4 (safety net) confirmed in game
- Code: `src/mod.cpp` (`displayModeCheckHook`, `usePhysicalPixels`, `restoreDpiAwareness`; `applyFullScreenHook`, `skipFullScreenReapply`; `windowProcedureHook`), `src/full_screen_reapply.h` (`fullScreenReapplyReason`); hooks at `0x5383d0`, `0x538180` and `0x5384c0`
- Tests: `display_mode_check_native_probe` (18 passed, 0 failed, label `needs_exe`), `full_screen_reapply_probe` (9 passed, 0 failed), `full_screen_reapply_native_probe` (16 passed, 0 failed, label `needs_exe`), `window_background_native_probe` (8 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-12

## Problem

In full screen (`FullScreen=1` in `S4-SETTINGS.ini`), every time the game window gets the focus back (for example after Alt+Tab), the screen turns white and the game freezes for a moment. For a moment the picture also spreads past the edges of the display.

Seen on a 3840x2160 monitor with Windows scaling at 125%. Whether it also happens in windowed mode, or at 100% scaling, needs in-game confirmation.

## Cause

- The game's "full screen" is a borderless window that covers the screen, plus `ChangeDisplaySettingsA(CDS_FULLSCREEN)`. The Direct3D 9 device is always created in windowed mode.
- On `WM_ACTIVATEAPP` (the window procedure at `0x538510`), the game applies full screen again (`0x538180`): it changes the display mode (`0x5383d0`), sizes the window to the settings' size, sets up the renderer again (`0x4ddc70`, which resets the Direct3D device with all render resources released and created again), and then sizes the window to the screen size. All of it runs inside the message handler, so the game stops until it is done. On losing the focus it always calls `ChangeDisplaySettingsA(NULL, 0)`.
- `0x5383d0` skips the display mode change when the screen already has the wanted size, using `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)`. The exe is not DPI aware, so on a scaled monitor Windows gives it the scaled size (3072x1728 for 3840x2160 at 125%). The sizes never match, so the display mode is changed every time the window is activated.
- For the same reason, the first window size (3840x2160 in the window's scaled coordinates, 4800x2700 physical pixels at 125%) is larger than the screen until the second one (3072x1728) is set: the picture spreads past the display.
- The window class uses a white background brush (`WHITE_BRUSH`, `0x538870`), and the game leaves `WM_ERASEBKGND` and `WM_PAINT` to `DefWindowProcA`. When the window is resized and repainted before the game presents a new frame, it is white.

## Possible fixes

1. Make the size check in `0x5383d0` compare physical pixels.
   - Pros: no display mode change when the screen already has the wanted size; other sizes still change the mode as before. Small change in code that runs only on activation.
   - Cons / risks: the window resizes and the device reset stay.
2. Make the whole process DPI aware (`SetProcessDPIAware`).
   - Pros: would also fix the size check, and place the window in physical pixels instead of being scaled by Windows.
   - Cons / risks: Mods are loaded when the game first calls `DirectInput8Create`, after its window exists. The window keeps the DPI awareness it was created with, and its messages run with it, so this has no effect from a Mod.
3. Skip applying full screen again on activation when nothing changed.
   - Pros: removes the window resizes, the white repaint and the device reset.
   - Cons / risks: a device that was really lost (lock screen and so on) must still be recovered; the renderer does it at the start of every frame (`0x893d00` → `0x891f80`, `TestCooperativeLevel`, then reset when `D3DERR_DEVICENOTRESET`). Needs a reliable check that the display mode and the window are unchanged, which needs the physical screen size as in fix 1.
4. Do not paint the window background white (handle `WM_ERASEBKGND`, or a black brush).
   - Pros: no white flash, whatever repaints the window.
   - Cons / risks: the freeze stays.
5. Play in windowed mode, or set "Override high DPI scaling behavior: Application" in the exe's compatibility settings.
   - Pros: no Mod needed.
   - Cons / risks: not tested.

## Chosen fix

Fixes 1 and 3, and fix 4 as a safety net for the cases where fix 3 still applies full screen again (another resolution in the settings, a moved window, Alt+Enter and so on) or anything else repaints the window.

Fix 1, physical screen size check (`[physical_screen_check]`):

- The Mod hooks `0x5383d0`. While it runs, the thread uses per-monitor DPI awareness (`SetThreadDpiAwarenessContext`), so `GetSystemMetrics` gives the physical size of the primary screen, then the thread's previous DPI awareness is restored. The game's own code does the comparison and the mode change.
- At startup the Mod checks the bytes at `0x5383d0` (prologue), `0x5383e7` (the `GetSystemMetrics` import) and `0x5383fe` (the comparison). On Windows older than 10 version 1607, the hook is installed but changes nothing, and the log says so.
- `mod_loader.log` gets, on each activation, a line such as `display mode check: the game wants 3840x2160, the primary screen is 3840x2160 (physical pixels); no display mode change`.
- Verified without the game: on a mapped copy of the exe with `GetSystemMetrics` answering like a 125% screen and a recording `ChangeDisplaySettingsA`, the game's check changes the display mode to 3840x2160 without the Mod, does not with the Mod, still changes it for another size (2560x1440), and the thread is DPI unaware again afterwards.
- Confirmed in game (2026-10-12): returning to the window is much faster, the log line appears, and the picture and mouse are unchanged. The white flash and the picture spreading past the display remained.

Fix 3, full screen re-apply skip (`[skip_full_screen_reapply]`):

- The Mod hooks `0x538180`. When it is asked for full screen, the window is already in full screen (`window+0xec`), the settings' size equals the primary screen in physical pixels, the window is not minimized, has no frame, and covers exactly the screen, the call returns without doing anything. Otherwise, and always when switching to windowed mode, the game's code runs unchanged.
- At startup the Mod checks the bytes at `0x538180` (prologue), `0x53818e`/`0x538193` (the settings' size), `0x53819b` (the full screen flag) and `0x5381c9` (the window handle).
- `mod_loader.log` gets, on each activation, `full screen re-apply skipped: the display is already 3840x2160 and the window covers it`, or `full screen applied again: <reason>`.
- Verified without the game: on a mapped copy of the exe with fake window, display mode and renderer calls and a hidden test window, activation runs the renderer setup and window calls without the Mod and nothing with the Mod; a window larger than the screen, other settings, entering full screen from windowed mode, and switching to windowed mode still run the game's code.
- Confirmed in game (2026-10-12): the white flash is gone. Still to check: whether the picture spreading past the display and the remaining freeze are gone; that the game still recovers after the lock screen (Win+L) or a display change while it is in the background; that Alt+Enter still switches modes.

Fix 4, no white background (`[no_white_background]`):

- The Mod hooks the game's window procedure `0x5384c0` (registered at `0x538896`) and answers `WM_ERASEBKGND` with 1 without painting, so a repaint keeps the last frame until the game presents the next one, instead of the white class brush. Other messages go to the game's procedure unchanged. The hook runs for every message of the window and only adds one comparison.
- At startup the Mod checks the bytes at `0x5384c0` (prologue and the window object `[0x9ecf90]`), `0x538896` (the registration of `0x5384c0`) and `0x5388ca`/`0x5388cf` (`GetStockObject(WHITE_BRUSH)`).
- `mod_loader.log` gets `no white background installed at 0x5384c0`. Nothing is logged per message.
- Verified without the game: on a mapped copy of the exe with a recording `DefWindowProcA`, `WM_ERASEBKGND` reaches `DefWindowProcA` without the Mod, and returns 1 without reaching it with the Mod; other messages still reach it.
- Confirmed in game (2026-10-12): works as intended.

Each fix can be turned off in `config.ini` (`enabled=0`) to compare with and without it.

## Notes

- The window is still placed at the scaled size (3072x1728) and stretched by Windows, as before.
- The first full screen setup at startup runs before Mods are loaded, so it still changes the display mode once.
