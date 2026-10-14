# Crash on the title screen after opening Options

- Status: implemented, needs in-game confirmation (the same refresh in the difficulty selection was confirmed in game)
- Code: `src/mod.cpp` (`titleOptionsStateHook`, `titleSelectWindowCodeMatches`), `src/title_select_window.h` (`useCurrentSelectWindow`), hook at `0x4ee380`
- Tests: `title_select_window_native_probe` (10 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-14

## Problem

After opening Options on the title screen and going back, the title screen can crash when it shows a selection window:

- Load on a save made after the end of a playthrough, when the game opens the difficulty selection. This was seen in game.
- New Game, when the game asks for confirmation. This goes through the same code, but whether it crashes depends on where the memory of the old window ends up (needs in-game confirmation).

Restarting the game and not opening Options on the title before choosing avoids it.

## Cause

- When the title (GamePartTitle) opens, it creates a select window (UI type `0x1b`) and keeps a pointer to it at `+0x4c` (`0x4ed24b`). The New Game confirmation (`0x4ee0d6`) and the difficulty selection (`0x4ed530`) use that pointer.
- Opening Options on the title (its state `0x4ee380`, `OptionsMenuManager`) creates a select window of its own (`0x6763df`). The UI manager's create function (`0x67fcf0`) deletes the existing window of the same type first, so the title's window is deleted.
- The title never updates `+0x4c`, so after Options it points at freed memory. Opening the window from there reads its layout parts, which are gone (`0x5bde80` reads `[+0xc]` = NULL).
- If a new object happens to be allocated at the same address, the stale pointer still works, which is likely why it does not always crash.

## Possible fixes

1. When Options is open on the title, point the title's `+0x4c` at the select window the UI manager holds (chosen)
   - Pros: one hook covers every later use (New Game, difficulty selection). The window is the one Options made, of the same type, already loaded.
   - Cons / risks: runs every frame while Options is open; it is only a few reads.
2. Refresh the pointer where it is used (difficulty selection, New Game confirmation)
   - Pros: touches only the moment of use.
   - Cons / risks: several places to hook, and any missed use stays broken.
3. Keep Options from deleting the title's window
   - Cons / risks: changes how the UI manager owns its windows; Options would then share or leak a window.

## Chosen fix

- `0x4ee380` (the title's Options state) is hooked. After the game's code runs, if the UI manager's select window (`[[title+0x58]+4+0x1b*4]`, as `0x67fcdc` reads it) exists and differs from the title's `+0x4c`, `+0x4c` is set to it.
- The Mod checks the bytes of the state, of the title's window creation (`0x4ed242`), of Options' creation (`0x6763de`) and of the UI manager's lookup at startup. If they differ, this fix is skipped and the log says so.
- `mod_loader.log` gets one line each time Options replaces the title's window: `title select window refresh: Options replaced the title's select window; the title uses the new one`.
- Verified with `title_select_window_native_probe` on a copy of the exe with a synthetic title and UI manager. In the game, the same refresh made in the difficulty selection stopped the crash after Options; this hook itself still needs in-game confirmation.

## Notes

- Options in the pause menu during play is a different code path and is not changed.
