# White screen and freeze when returning to the window

- Status: partly implemented (physical screen size check); confirmed in game that returning to the window is much faster
- Code: `src/mod.cpp` (`displayModeCheckHook`, `usePhysicalPixels`, `restoreDpiAwareness`), hook at `0x5383d0`
- Tests: `display_mode_check_native_probe` (18 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-12

## Problem

In full screen (`FullScreen=1` in `S4-SETTINGS.ini`), every time the game window gets the focus back (for example after Alt+Tab), the screen turns white and the game freezes for a moment.

Seen on a 3840x2160 monitor with Windows scaling at 125%. Whether it also happens in windowed mode, or at 100% scaling, needs in-game confirmation.

## Cause

- The game's "full screen" is a borderless window that covers the screen, plus `ChangeDisplaySettingsA(CDS_FULLSCREEN)`. The Direct3D 9 device is always created in windowed mode.
- On `WM_ACTIVATEAPP` (the window procedure at `0x538510`), the game applies full screen again (`0x538180`): it changes the display mode (`0x5383d0`), sets the window style and position again, and resets the Direct3D device (`IDirect3DDevice9::Reset`, with all render resources released and created again). All of it runs inside the message handler, so the game stops until it is done. On losing the focus it always calls `ChangeDisplaySettingsA(NULL, 0)`.
- `0x5383d0` skips the display mode change when the screen already has the wanted size, using `GetSystemMetrics(SM_CXSCREEN/SM_CYSCREEN)`. The exe is not DPI aware, so on a scaled monitor Windows gives it the scaled size (3072x1728 for 3840x2160 at 125%). The sizes never match, so the display mode is changed every time the window is activated.
- The window class uses a white background brush (`WHITE_BRUSH`, `0x538870`), and the game leaves `WM_ERASEBKGND` and `WM_PAINT` to `DefWindowProcA`. When the window is repainted before the game presents a new frame, it is white.
- Still unknown: whether changing to the same physical mode really switches the mode on the monitor, and how long each part (mode change, device reset, resource reload) takes.

## Possible fixes

1. Make the size check in `0x5383d0` compare physical pixels.
   - Pros: no display mode change when the screen already has the wanted size; other sizes still change the mode as before. Small change in code that runs only on activation.
   - Cons / risks: the device reset stays, so part of the freeze may remain.
2. Make the whole process DPI aware (`SetProcessDPIAware`).
   - Pros: would also fix the size check, and place the window in physical pixels instead of being scaled by Windows.
   - Cons / risks: Mods are loaded when the game first calls `DirectInput8Create`, after its window exists. The window keeps the DPI awareness it was created with, and its messages run with it, so this has no effect from a Mod.
3. Skip applying full screen again on activation when nothing changed.
   - Pros: removes the device reset and the freeze.
   - Cons / risks: a device that was really lost (lock screen and so on) must still be recovered. Needs a reliable check that the display mode was not changed, which needs fix 1 first.
4. Do not paint the window background white (handle `WM_ERASEBKGND`, or a black brush).
   - Pros: no white flash, whatever repaints the window.
   - Cons / risks: the freeze stays.
5. Play in windowed mode, or set "Override high DPI scaling behavior: Application" in the exe's compatibility settings.
   - Pros: no Mod needed.
   - Cons / risks: not tested.

## Chosen fix

Fix 1 for now. Fixes 3 and 4 are planned after fix 1 is checked in the game, one at a time, so the effect of each can be seen.

- The Mod hooks `0x5383d0`. While it runs, the thread uses per-monitor DPI awareness (`SetThreadDpiAwarenessContext`), so `GetSystemMetrics` gives the physical size of the primary screen, then the thread's previous DPI awareness is restored. The game's own code does the comparison and the mode change.
- At startup the Mod checks the bytes at `0x5383d0` (prologue), `0x5383e7` (the `GetSystemMetrics` import) and `0x5383fe` (the comparison). If they differ, this fix is skipped and the reason is written to `mod_loader.log`. On Windows older than 10 version 1607, the hook is installed but changes nothing, and the log says so.
- `mod_loader.log` gets `physical screen size check installed at 0x5383d0`, and on each activation a line such as `display mode check: the game wants 3840x2160, the primary screen is 3840x2160 (physical pixels); no display mode change`. This runs only when the window is activated or the resolution is applied, not every frame.
- Verified without the game: on a mapped copy of the exe with `GetSystemMetrics` answering like a 125% screen and a recording `ChangeDisplaySettingsA`, the game's check changes the display mode to 3840x2160 without the Mod, does not with the Mod, still changes it for another size (2560x1440), and the thread is DPI unaware again afterwards.
- `[physical_screen_check] enabled=0` in `config.ini` turns this fix off, to compare with and without it.
- Confirmed in game (2026-10-12): returning to the window is much faster with this fix. Still to check: whether the white flash remains, the log lines, and that the picture and mouse are unchanged.

## Notes

- The window is still placed at the scaled size (3072x1728) and stretched by Windows, as before.
- The first full screen setup at startup runs before Mods are loaded, so it still changes the display mode once.
