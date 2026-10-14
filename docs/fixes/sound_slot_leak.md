# Sound effects and voices go silent until a restart

- Status: implemented, needs in-game confirmation
- Code: `src/mod.cpp` (`stopSoundSlotHook`, `acquireSoundSlotHook`, `releaseTakenSlot`, `soundSlotCodeMatches`), `src/sound_slots.h` (`prepareStoppedSlotRelease`), hooks at `0x5a1210` and `0x5a19e0`
- Tests: `sound_slots_probe` (12 passed, 0 failed), `sound_slots_native_probe` (22 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-14

## Problem

After some scenes, part of the game's sound stops playing until the game is restarted:

- The sound of a successful catch when fishing, and the voices of shopkeepers and other characters, are no longer heard.
- Opening or closing a menu plays the torture sound of the current town's security level.
- Players also report sword clashes that keep sounding for a while, for example in group battles.

It is most often reported after torture or an arrest (almost every time after a successful catch followed by an arrest or torture), and after the torture near the end of the Light of Hope route. Torture is only one way to cause it: any sound stopped at the wrong moment can.

## Cause

The game plays each sound in one of 24 slots. Streamed sounds (most voices, event voices, music and about half of the sound effects, including the fishing success sounds and the torture loops) may use at most 5 slots at once, music included (`0x5a05c0`, checked by `0x5a2d30` for voices and `0x5a2f10` for sound effects). When 5 are in use, a new streamed voice or sound effect is dropped.

A slot is freed when the end notification of its channel says the sound ended (`0x5a1950`, sent by `0x6a2480` from the slot update `0x5a0640`). That notification is sent only on a change: the channel was playing at the last update, is not playing now, and is not paused. Stopping a sound without a fade (`0x5a1210`, used by the torture and fishing code, map changes and many others) only stops the channel, so two kinds of stopped slots are never freed:

1. A slot stopped while it was paused. Menus pause looping sound effects (`0x5a39a0`), so a torture loop stopped while a menu is open stays paused.
2. A slot stopped before the slot update ever saw it playing, for example a loop started and stopped within one frame.

Such a slot stays in use until the game is restarted. It keeps counting toward the 5 streams, so after a few of them streamed voices and sound effects are dropped. A paused looping sound effect is played again whenever a menu closes (`0x5a39a0` resumes every paused loop without checking whether it was stopped), which is why the torture sound plays when a menu is opened or closed.

Separately, when all 24 slots are in use, `0x5a19e0` hands out a slot that is still playing (`0x5a1a90`) and `0x5a2900` fills it with the new sound without stopping the old one. The old sound keeps playing with no slot or handle left to stop it. This is the likely cause of sounds that keep playing, such as the sword clashes; it has not been confirmed in game.

## Possible fixes

1. After every stop, hand the slot to the game's own release
   - Pros: one hook on the function every stop goes through (by handle, by group and the stream limit), so every scene that stops sounds is covered, not only torture. The game's own end notification frees the slot, as it does for a sound that ends normally.
   - Cons / risks: runs on every stop of a sound; it only reads and writes a few bytes.
2. Send the end notification from the channel's state instead of its change (`0x6a2480`)
   - Pros: no change to the stop.
   - Cons / risks: runs for every slot every frame, and still cannot tell a stopped paused slot from a slot that a menu paused.
3. Free slots that look stopped from time to time
   - Pros: would also clear slots that already leaked.
   - Cons / risks: a stream that is still loading also looks stopped, so it needs extra checks; it fixes the result and not the cause.
4. Fix only the torture code
   - Pros: smallest change.
   - Cons / risks: fishing, map changes and other code stop sounds the same way.

## Chosen fix

Approach 1, plus a fix of the slot handout.

- `0x5a1210` (stop): after the game's stop, if the sound was stopped without a fade, or was paused (a paused sound is silent and cannot fade out), the Mod clears the channel's pause and marks it as playing at the last update. At the next slot update the game sees a channel that played and stopped, sends its end notification and frees the slot: a stream through `+0x10 = 2`, a sample at once. A fade out of a playing sound is left to the game, which frees it when the fade ends. A closing menu no longer plays the stopped sound, because it is not paused any more.
- `0x5a19e0` (slot handout): when it hands out a slot that is still in use, the Mod stops and frees that slot's sound the way the end notification does (`0x5a0290`, the Sound's destructor, `0x5a03a0`) and keeps the new handle. This only happens when all 24 slots are in use and is logged as `all 24 sound slots were in use; stopped the sound in slot N for a new one`.

At startup the Mod checks the bytes of `0x5a1210` (including its fade test at `0x5a1243` and the channel stop at `0x5a1257`), the end notification check in `0x6a2480` and its callback set at `0x5a10c0`, `0x5a19e0` and its handle store at `0x5a1b29`, the caller at `0x5a2a9d`, and `0x5a0290` and `0x5a03a0`. If any differs, the fix is skipped and `mod_loader.log` says so. When it is installed, the log has `sound slot release installed at 0x5a1210 and 0x5a19e0`. It can be turned off with `[sound_slot_release] enabled=0`.

Tests (the game is not started):

- `sound_slots_probe` checks, on synthetic slots, which stops are handed to the game's release and that a fade out of a playing sound is not.
- `sound_slots_native_probe` runs the game's own stop, end notification check and slot handout on a copy of the exe with fake DirectSound buffers. Without the Mod, a stream stopped while paused and a stream stopped before the first update stay in use, the paused one stays paused, and a full table hands out a playing slot without stopping it. With the Mod, the streams are marked for release, a stopped sample is freed, a fade out is left to the game, a stop by handle goes through the fix, and a slot taken from a playing sound has that sound stopped and freed. Registers and the stack are checked after each hook.

Needs in-game confirmation: that the symptoms no longer appear after torture, arrests and the end of the Light of Hope route; that menus no longer play the torture sound; and whether the sword clashes stop.

## Notes

- Slots that leaked before the Mod was installed, or in a session without it, stay until the game is restarted.
- Whether the shopkeepers' voices are streamed has not been checked; most character voices are.
- The game's slot update (`0x5a0640`) has a fallback that frees a stopped sample without deleting its Sound; the end notification used by this fix deletes it.
