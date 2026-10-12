# Night visit reward handed out again after returning to the title

- Status: confirmed in game (reported 2026-10-12)
- Code: `src/mod.cpp` (`loadSaveDataHook`, `yobaiResultCodeMatches`), `src/yobai_result.h` (`discardYobaiResult`), hook at `0x4d9f50`
- Tests: `yobai_result_probe` (9 passed, 0 failed), `yobai_result_native_probe` (16 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-12

## Problem

After a night visit (yobai), success or failure, the next map change hands out the result: the "token of love" or the "consolation gift" drops on the ground and the night visit promise is removed. This part is normal.

If you then choose "Return to title" from the menu without saving and load a save that still has a night visit promise, the same thing happens again right after loading, at the first map change. The old reward drops again, and the promise in the loaded save is lost.

Saving after the night visit and before returning to the title avoids it.

## Cause

- The night visit progress (`[0x9ecbb8]`) keeps the result of the last visit at `+9` (-1 none, 0 success, 1 consolation, 2 Laura, 3 failure) and a "handled" mark at `+0xa`.
- At a map change, `0x6a0560` hands the result out when a promise exists, the mark is 0 and the result is 0 to 3: it shows the message, gives the reward (`0x6a08b0`), removes the promise (`0x69f650`) and sets the mark to 1. It leaves the result as it is.
- Saving clears both (`0x53e43a`: result -1, mark 0). Returning to the title ends the play session in `0x4e18e0`, which clears only the mark (`0x4e2076`, `0x4e208b`). The progress object is not created again for the title, so the result survives.
- After loading, a save with a promise meets an old result and a cleared mark, so the next map change hands the old result out again.

### Retry after death needs the result

- Dying also ends the play session through `0x4e18e0` (the dialog callback `0x4e0830`). "Retry" then copies the in-memory snapshot `[0x9ecfa8]` back into the game state (`0x4e91c2` → `0x4d9f50` with its third argument 0). Loading a save file uses the same function with 1 (`0x53eaed`, data read into `[0x9ecf9c]`).
- The snapshot is taken at map exits (`0x4e72b6` → `0x53e3e0`). When you leave after a night visit, the result is not handed out yet, so the snapshot still has the promise and not the reward, and `0x53e3e0` keeps the result (it clears it only when it was handled).
- So after a retry, the game hands the result out again at the next map change, and you get the reward that the restored state does not have. Clearing the result there is wrong.

## Possible fixes

1. When the play session ends (`0x4e18e0`), also set the result back to -1, as saving does.
   - Pros: one place every "Return to title" goes through.
   - Cons / risks: retry after death goes through it too, and loses the reward of a successful visit.
2. Set the result back to -1 where `0x6a0560` marks it handled (`0x6a06c4`).
   - Cons / risks: the same problem as 1 for a retry, and other code reads the result after the visit (for example `0x4c4551` at a map change).
3. Clear the result when a save file is loaded (`0x4d9f50` with the third argument set).
   - Pros: the reported case is a save file loaded after the session ended. Retry restores the snapshot with the argument 0 and keeps the game's behavior. Runs once per load, not every frame.
   - Cons / risks: starting a new game from the title loads no file, but a new game has no promise, so `0x6a0560` does nothing.

## Chosen fix

Approach 3.

- The Mod hooks `0x4d9f50`. After the game's own code has run, if the low byte of the third argument is not 0 (as the game tests it at `0x4d9f8b`) and the night visit progress (`[0x9ecbb8]`) exists, it sets `+9` to -1 and `+0xa` to 0, the same values the game writes after saving.
- Safety checks at startup: the Mod compares the bytes at `0x4d9f50` and `0x4d9f8b`, the save file load call (`0x53eadd`–`0x53eaf2`, which must read `0x9ecf9c` and `0x9e967c`, push 1 and call `0x4d9f50`), the retry call (`0x4e91b6` pushes 0, after `0x4e918e` reads `0x9ecfa8`), `0x6a05f3` and `0x6a06c4` (reading the result and marking it handled), and `0x53e43a` (clearing after saving, with `0x53e42c` pointing at `0x9ecbb8`). If anything differs, it skips this fix (other fixes are still installed) and writes `night visit result reset skipped: the game code at 0x4d9f50 is not the known version` to `mod_loader.log`. It claims 5 bytes at `0x4d9f50` through `HostApi::claim`.
- What the player sees: after returning to the title and loading, the promise in the save stays and no reward drops. After dying and retrying, the reward of the visit is handed out as without the Mod. When a result was discarded, the Mod writes `discarded the night visit result N left in memory while loading a save file` to `mod_loader.log`.
- Verified without starting the game: `yobai_result_probe` checks the reset on synthetic progress objects. `yobai_result_native_probe` installs `mod.dll` on a copy of the exe and runs `0x4d9f50` through the game's own last lines (its body is skipped on the copy). Without the Mod the result stays after a file load; with it a file load sets the result to -1 and the mark to 0, a snapshot load keeps the result, and the caller's registers are kept.
- Confirmed in game (reported 2026-10-12): loading a save with a promise after a night visit keeps the promise, and a success followed by death and retry hands out the reward again.

## Notes

- Possibly the same cause, not handled by this fix: after a night visit, without saving, a new promise in the same session might be used up by the old result when the next event ends (`0x56a810` clears the mark and checks again). Needs in-game confirmation.
- Retry restores the snapshot from the last map exit even when that exit was before the visit (the doors of a night visit do not take a snapshot, `0x4e72a0`). The game then hands out the result to a state before the visit. This is the game's own behavior and the Mod keeps it.
- After a game update, check the byte patterns in `yobaiResultCodeMatches` first. The Mod skips this fix if they no longer match.
