# Crash right after the final boss when resuming at the jail photographer

- Status: confirmed in game (reported 2026-10-11)
- Code: `src/mod.cpp` (`releaseEffectsHook`, `followDetachCodeMatches`), `src/follow_detach.h` (`detachJointFollowers`), hook at `0x48d520`
- Tests: `follow_detach_probe` (11 passed, 0 failed), `follow_detach_native_probe` (17 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-12

## Problem

On the true route, if you save at the photographer in the magistrate's jail and later load that save, the game crashes to the desktop when you beat the final boss (Kinugawa). The crash happens a few seconds into the scene that follows the fight. It does not happen on every run.

Workarounds reported without the Mod are to avoid resuming from the jail photographer, or to change the starting point before the ending.

## Cause

Some effects follow a joint of a character model and copy the joint's position every frame. The effect keeps a raw pointer to the joint at effect `+8` and reads its world position at `0x4a4cf8`.

- Effects attached by scripts (`SetEffect`) and by time-table type 41 are created at `0x5658c0`. It stores the joint `[[character+0x18]+0x48+joint*4]` in `+8` and does not record the owning character.
- When a character is released or its model is replaced, the game first calls `0x48d520` (character virtual `+0x68`) and then frees the model at `+0x18`. `0x48d520` detaches effects through `0x4a8ec0`, but that only handles two effect types (`0x62` and `0x84`), which it finds by the owner ID.
- So any other effect on the model's joints keeps pointing at the freed joint. If the memory has already been returned to the OS, the next update raises an access violation at `0x4a4cf8`. If the memory is still there, the effect only reads garbage. This explains why the crash depends on the run.

In the scene right after the boss fight, time-table type 41 attaches the aura effect `v_aura_b` (14) to six joints of Kinugawa. It has no lifetime limit. In the reported crash, those six auras pointed at a freed model.

Still unknown: why Kinugawa's model is freed during that scene only after resuming at the jail photographer. The fix does not depend on the answer.

## Possible fixes

1. Detach every effect that follows the joints of the model about to be freed (after `0x48d520`).
   - Pros: fixes the stale pointer itself, whatever frees the model. Runs only when a character is released or its model is replaced, not every frame. Uses the same detach as the game (`+8 = 0`).
   - Cons / risks: the detached effect stays where it was until it ends (the aura has no lifetime limit). Only the main effect manager is scanned.
2. Check whether the joint is readable before `0x4a4cf8`, and detach the effect if it is not.
   - Pros: small change at the crash site.
   - Cons / risks: it runs every frame for every following effect. It cannot detect freed memory that is still committed, so the effect may follow garbage or memory that is reused for something else.
3. Find out why the model is freed after resuming at the jail photographer, and avoid that.
   - Pros: would keep the scene exactly as designed.
   - Cons / risks: the cause is not known (U-CF-2), and any other path that frees a model with effects attached would still crash.
4. Stop the aura effects (14) before the scene changes characters.
   - Pros: narrow.
   - Cons / risks: covers only this scene and this effect.

## Chosen fix

Approach 1. It removes the dangling pointer at the point where the game already detaches effects, it costs nothing per frame, and it also covers other scripts that attach effects to joints.

- The Mod hooks `0x48d520` with MinHook. After the original function, it reads the effect manager `[0x9e966c]` and the character's model `[character+0x18]`. That model is still the one about to be freed or replaced. The Mod then clears `+8` of every effect among the 350 slots (`+0x1c`) whose `+8` is one of the model's 23 joints (`+0x48`, `JOINT_ID_END` = 0x17). Effects that do not follow a joint, effects that follow other models, and effects without a target are left alone.
- Safety checks at startup: the Mod compares the bytes at `0x48d520`, `0x4a8ec0`, `0x56590e` and `0x48d54e`, and checks that `0x48d550` points at `0x9e966c`. If anything differs, it skips this fix (the other fixes are still installed) and writes `follow-effect detach skipped: the game code at 0x48d520 is not the known version` to `mod_loader.log`. It claims 5 bytes at `0x48d520` through `HostApi::claim`.
- What the player may notice: a detached aura stays where it was instead of following the character. Each time effects are detached, `mod_loader.log` gets a line `detached N effect(s) from the joints of a released model`.
- Verification:
  - `follow_detach_probe` checks the detach on synthetic data.
  - `follow_detach_native_probe` loads `mod.dll` into a copy of the exe mapped into the test process and runs the game's own `0x4a4c80`, `0x48d520` and `0x4a8ec0`:
    - Without the Mod, a freed joint faults at `0x4a4cf8`.
    - With the Mod, the six auras are detached, the blood effect is still detached by the game, effects on another model are kept, and updates after the model is freed do not fault.
  - In game, the user resumed from the jail photographer save and reached the ending without the crash (2026-10-11).

## Notes

- Effect groups that keep their own 350 slots (`0x4a64c0`) are not scanned. No crash has been traced to them.
- The reason the model is freed mid-scene after resuming at the jail photographer (U-CF-2) is still open. If it also leaves other references to the old model, they would show up as a different crash.
- After a game update, check the byte patterns in `followDetachCodeMatches` first. The Mod skips this fix if they no longer match.
