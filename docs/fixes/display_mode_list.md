# Game freezes for minutes when starting to play or opening Options

- Status: confirmed in game (the freeze is gone; the list of resolutions was not compared one by one)
- Code: `src/mod.cpp` (`testListedDisplayMode`, `displayModeListCodeMatches`), `src/display_mode_list.h` (`gameListsDisplayMode`), patch at `0x536af9`
- Tests: `display_mode_list_probe` (9 passed, 0 failed), `display_mode_list_native_probe` (12 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-16

## Problem

When starting to play, or the first time Options is opened, the game stops responding for a long time. Windows may show the window as "Not responding". On a 4K monitor with high refresh rates it took two to three minutes; then the game goes on normally.

Once the game has made its list of resolutions, it keeps it, so later openings of Options are not affected.

## Cause

- The display settings in Options need a list of resolutions. The game makes it once (`0x536a90`), the first time it needs the list; that was seen both when starting to play and when opening Options. It goes through every display mode `EnumDisplaySettingsA` lists, asks `ChangeDisplaySettingsA` with `CDS_TEST` whether the mode can be set (`0x536af9`), and only then drops the modes it never shows: not 32 bits per pixel, narrower than 640 pixels or below 60 Hz (`0x536b07`-`0x536b24`).
- To programs as old as this game, Windows lists every resolution and refresh rate again in 8 and 16 bits per pixel. Testing one of those took about 0.24 s on the monitor where this was seen, against about 0.007 s for a 32-bit mode.
- That monitor listed 184 modes in each depth (552 in all), so the 368 tests of modes the game drops anyway took about 90 s, and longer inside the game. The game does nothing else while it tests, so it freezes.

## Possible fixes

1. Answer "bad mode" without the test for the modes the game drops (chosen)
   - Pros: the list keeps exactly the same modes, because the game drops these modes whatever the test answers. Only one call site changes.
   - Cons / risks: none known. 32-bit modes are still tested.
2. Make Windows stop listing the 8 and 16-bit modes to the game
   - Cons / risks: how Windows decides this for a program was not examined, and it would change what the whole game sees, including the display mode changes of the full screen window.
3. Make the list in the background
   - Cons / risks: Options would need the list before it is done; much more code for the same result.

## Chosen fix

- The `call [ChangeDisplaySettingsA]` at `0x536af9` (in the resolution list only) is replaced with a call to the Mod. For a `CDS_TEST` of a mode the game would drop (not 32 bits, narrower than 640 pixels or below 60 Hz), it returns `DISP_CHANGE_BADMODE` at once. Every other call goes on to `ChangeDisplaySettingsA` through the game's import.
- The Mod checks the bytes of the test, of the drop conditions that follow it and of the `EnumDisplaySettingsA` call at startup. If they differ, this fix is skipped and the log says so.
- `mod_loader.log` gets `display mode list installed at 0x536af9` at startup.
- Verified with `display_mode_list_native_probe`: on a copy of the exe with 36 synthetic modes in 8, 16 and 32 bits, the list the game makes is the same with and without the Mod, and with the Mod no 8 or 16-bit mode is tested. In the game the freeze no longer happens; whether the list of resolutions is the same as before was not compared one by one.

## Notes

- With 184 32-bit modes the remaining tests take about 1 s.
- The full screen display mode changes (`0x5383d0`, `0x538180`) call `ChangeDisplaySettingsA` elsewhere and are not changed.
