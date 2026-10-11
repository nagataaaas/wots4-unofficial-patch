# Night visit reward handed out again after returning to the title

- Status: implemented, needs in-game confirmation
- Code: `src/mod.cpp` (`endPlaySessionHook`, `yobaiResultCodeMatches`), `src/yobai_result.h` (`discardYobaiResult`), hook at `0x4e18e0`
- Tests: `yobai_result_probe` (9 passed, 0 failed), `yobai_result_native_probe` (13 passed, 0 failed, label `needs_exe`)
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

## Possible fixes

1. When the play session ends (`0x4e18e0`), also set the result back to -1, as saving does.
   - Pros: the exact case reported. Runs once per session, not every frame. Matches what saving already does.
   - Cons / risks: does not cover the case where a new promise is made later in the same session without saving (see Notes).
2. Set the result back to -1 where `0x6a0560` marks it handled (`0x6a06c4`).
   - Pros: the result is never handed out twice, in any session.
   - Cons / risks: other code reads the result after the visit (for example `0x4c4551` at a map change); each reader has to be checked first.
3. Clear the result when a save is loaded.
   - Cons / risks: the load path is longer and shared with other features; returning to the title and starting a new game would still keep it.

## Chosen fix

Approach 1.

- The Mod hooks `0x4e18e0`. After the game's own code has run, if the night visit progress (`[0x9ecbb8]`) exists, it sets `+9` to -1 and `+0xa` to 0, the same values the game writes after saving. Every play session ends here, so a result still kept belongs to the session being thrown away.
- Safety checks at startup: the Mod compares the bytes at `0x4e18e0`, at both ends of `0x4e18e0` (`0x4e2070`, `0x4e2083`, which must load `0x9ecbb8`), at `0x6a05f3` and `0x6a06c4` (reading the result and marking it handled), and at `0x53e43a` (clearing after saving, with `0x53e42c` pointing at `0x9ecbb8`). If anything differs, it skips this fix (other fixes are still installed) and writes `night visit result reset skipped: the game code at 0x4e18e0 is not the known version` to `mod_loader.log`. It claims 6 bytes at `0x4e18e0` through `HostApi::claim`.
- What the player sees: after returning to the title and loading, the promise in the save stays and no reward drops. When a result was discarded, the Mod writes `discarded the night visit result N left from the play session that ended` to `mod_loader.log`.
- Verified without starting the game: `yobai_result_probe` checks the reset on synthetic progress objects. `yobai_result_native_probe` installs `mod.dll` on a copy of the exe and ends a session through the game's own last lines of `0x4e18e0` (its body is skipped on the copy). Without the Mod the result stays; with it the result is -1 and the mark 0, and the caller's registers are kept.
- Needs in-game confirmation: that "Return to title" goes through `0x4e18e0`, and that loading a save with a promise after a night visit keeps the promise.

## Notes

- Possibly the same cause, not handled by this fix: after a night visit, without saving, a new promise in the same session might be used up by the old result when the next event ends (`0x56a810` clears the mark and checks again). Needs in-game confirmation. Approach 2 would cover it.
- After a game update, check the byte patterns in `yobaiResultCodeMatches` first. The Mod skips this fix if they no longer match.
