# Weapons and items are not given back after Madara's rescue

- Status: implemented, needs in-game confirmation
- Code: `src/mod.cpp` (`setNextScriptHook`, `propertyReturnCodeMatches`), `src/property_return.h` (`shouldReturnProperty`), hook at `0x55e140`
- Tests: `property_return_probe` (11 passed, 0 failed), `property_return_native_probe` (15 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-12

## Problem

On the true route, in "The Fighting Spirit Never Dies", the three sisters capture the player in the torture room of the magistrate's office and take the player's weapons and items. The player is also dressed in the torture outfit.

Usually the player is tortured three times, fights the sisters unarmed, and after the fight Kogure hands the gear back.

When the save flag at `0x3760` is 1, Madara rescues the player instead and the torture and the fight are skipped. On this branch nothing hands the gear back: the player goes on to "Rescuing Laura" without weapons or items and in the torture outfit.

The taken weapons are kept in memory that is not saved. If you save and load the game in this state, those weapons are lost for good.

## Cause

- `BR_14_00` calls the script command `PlayerTakeProperty 0` (`0x554dd0` → `0x553d80`). It moves the 10 weapon slots and the items to a backup, saves the current outfit and switches to the torture outfit, and sets game state `+0x48b9e` to 1 (`0x553f50`). The weapon backup (game state `+0x493d8`) is outside the saved area.
- Only `PlayerGiveProperty` (`0x554e40` → `0x553f60`) restores them and sets `+0x48b9e` back to 0 (`0x55408e`). In this event, `BR_14_00_S140` (after the fight with the sisters) calls it.
- At the end of `BR_14_00_S050` (the execution stand scene), the script reads `akemi_proof` (`0x3760`). If it is 1 it goes to `BR_14_00_S210` (meeting Moro and Kogure), otherwise to `S060` and the fight. The `S210` branch never reaches `S140`, and `S210` itself does not call `PlayerGiveProperty`.

## Possible fixes

1. When the script moves to `BR_14_00_S210` with the property still taken, call the game's own `PlayerGiveProperty` (`0x553f60`), by hooking the `SetNextScript` command (`0x55e140`).
   - Pros: uses the game's own restore, so weapons, items and outfit come back exactly as on the other branch. The timing matches `S140`: `S210` starts with `LoadEventChara` and `LoadWait`, as `S140` does right after `PlayerGiveProperty`. `SetNextScript` runs only when a script changes scene, not every frame.
   - Cons / risks: depends on the script name. The restore runs at the end of `S050`, one script earlier than a call inside `S210` would.
2. Hook the parameter setter and restore when `S210` sets event 86 to 4.
   - Pros: no script name.
   - Cons / risks: the parameter setter is called much more often, and `S210` sets 86 = 4 after its scene, with no `LoadWait` before `BR_15_00`.
3. Restore later, e.g. when `BR_15_00` starts.
   - Cons / risks: `BR_15_00` is shared by both branches and has no clear point for it. The player would still look wrong in `S210`.
4. Edit the script files so that `S210` calls `PlayerGiveProperty`.
   - Cons / risks: changes game files, which this Mod does not do.

## Chosen fix

Approach 1.

- The Mod hooks `0x55e140` (`SetNextScript`). After the game's code has copied the name of the next script to `[0x9eb5f8]+0x2c`, the Mod checks that the name is exactly `scenario/p420/BR_14_00_S210` and that game state (`[0x9e967c]`) `+0x48b9e` is not 0. Only then it calls `0x553f60`.
- On the fight branch the script never goes to `S210`, and after `S140` the flag is 0, so nothing is given back twice. The extra work in `SetNextScript` is one string comparison, with no allocation or file I/O.
- Viewer mode is not checked, as `S140` gives the property back without checking it either. The flag limits the restore to a property that is actually taken.
- Safety checks at startup: the Mod compares the bytes at `0x55e140`, `0x55e19e`–`0x55e1ab` (with `0x55e19f` pointing at `0x9eb5f8`), `0x553f60`, `0x553f50` and `0x55408e`, and checks that `0x553f7c` points at `0x9e967c`. If anything differs, it skips this fix (other fixes are still installed) and writes `property return after Madara's rescue skipped: the game code at 0x55e140 is not the known version` to `mod_loader.log`. It claims 5 bytes at `0x55e140` through `HostApi::claim`.
- What the player sees: when the rescue scene ends, the weapons, items and outfit are back, as after the fight on the other branch. The Mod writes `returned the player's weapons, items and outfit after Madara's rescue (BR_14_00_S210)` to `mod_loader.log`. If the game finds no player character, the property stays taken and the Mod writes a line that says so.
- Verified without starting the game: `property_return_probe` checks the decision on synthetic names and flags. `property_return_native_probe` installs `mod.dll` on a copy of the exe and runs the hooked `SetNextScript`, with the rest of `SetNextScript` and the body of `PlayerGiveProperty` replaced on the copy. It checks that only `S210` with the flag set calls `PlayerGiveProperty`, once, and that the Mod still installs this fix when the other fix's code differs.
- Needs in-game confirmation: with `0x3760` = 1, that after the rescue the weapons, items and outfit are back, and that the weapons can be used in "Rescuing Laura".

## Notes

- A save made after the bug already happened cannot be repaired by this fix. The weapon backup was not saved. The item backup is at save `0x5b40` (game state `+0x5b58`) and may still be there; not checked.
- After a game update, check the byte patterns in `propertyReturnCodeMatches` first. The Mod skips this fix if they no longer match.
