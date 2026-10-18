# Sound effects that never play, and voices going silent after fishing

- Status: confirmed in game
- Code: `src/mod.cpp` (`playSound3dHook`, `stereo3dSoundCodeMatches`), hook at `0x6a3270`
- Tests: `stereo_3d_sound_native_probe` (15 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-18

## Problem

More than 65 sound effects are never heard at all, for example the fishing splashes, doors and gates opening and closing, explosions, fireworks, the gambling dens, and the torture devices (the water wheel, hanging and stone embrace).

After a few casts or catches when fishing, the voices of shopkeepers and other characters are no longer heard either, and some other sound effects go missing. It lasts until the game is restarted.

## Cause

Streamed sounds may use at most 5 of the game's 24 sound slots at once, music included (`0x5a05c0`). When 5 are in use, a new streamed voice or sound effect is dropped. A slot is freed when the slot update sees its sound stop playing (`0x6a2480` sends the end notification `0x5a1950` only after the channel played at the last update).

Some sound effects are both spatial (3D) and streamed from a stereo file, for example the fishing splashes (`fishing1`, `fishing2`, `up_water`). The stream's DirectSound buffer gets 3D control only when the file is mono, so for a stereo file the 3D play (`0x6a3270`) fails to get IDirectSound3DBuffer and returns `E_NOINTERFACE` before Play. The caller ignores the error and keeps the slot as playing. The sound never plays, so it never ends, and its slot is never freed.

Looping sounds of this kind are stopped by their code and freed by the [sound slot release](sound_slot_leak.md) fix. One-shot sounds such as the splash of a cast (`fishing1`) and of a catch (`up_water`) are never stopped, so each one takes a slot for good. With the music, 4 of them fill the 5 streams, and streamed voices go silent.

65 sound effects whose files could be checked are stereo streams marked as spatial (a few more have files that could not be resolved), so all of them stay silent, and places other than fishing can leak slots the same way.

## Possible fixes

1. Play such a sound in 2D when the 3D play fails with `E_NOINTERFACE`
   - Pros: the sound is heard and ends normally, so the game frees its slot itself. One hook on the only 3D play of a stream or sample, run once per sound start.
   - Cons / risks: the sound has no 3D position. The game gives spatial sounds full volume and leaves the distance to DirectSound, so it does not get quieter with distance. It plays as recorded in stereo.
2. Free the slot when the 3D play fails
   - Pros: no audible change from the original game.
   - Cons / risks: the sounds stay silent.
3. Create stereo streams with 3D control, or mix them down to mono
   - Pros: keeps the 3D position.
   - Cons / risks: DirectSound does not allow 3D control on stereo buffers; mixing down needs changes in the stream decoder and its buffer refills.

## Chosen fix

Approach 1. After the game's 3D play (`0x6a3270`), if it returned `E_NOINTERFACE`, the Mod calls the game's own 2D play (`0x6a31e0`) with the same Sound and play flags, as `0x6a2b00` does for non-spatial sounds, and returns its result. Any other result, success or failure, is passed on unchanged. The first fallback is logged as `stereo 3D sound fallback: a sound without 3D control was played in 2D`; later ones are not.

At startup the Mod checks the bytes of `0x6a3270` (its prologue and the IDirectSound3DBuffer query at `0x6a32f6`, with the IID at `0x962f50`), `0x6a31e0`, and the call from `0x6a2c6d`. If any differs, the fix is skipped and `mod_loader.log` says so. When it is installed, the log has `stereo 3D sound fallback installed at 0x6a3270`. It can be turned off with `[stereo_3d_sound] enabled=0`.

Tests (the game is not started): `stereo_3d_sound_native_probe` runs the game's 3D play on a copy of the exe with fake DirectSound buffers. Without the Mod, a buffer without 3D control is not played and `E_NOINTERFACE` is returned. With the Mod, it is played once in 2D with the play flags, the Sound is marked as playing and `S_OK` is returned. A buffer with 3D control is still played in 3D, another failure is passed on without a 2D play, and registers and the stack are checked after each call.

Confirmed in game: voices keep playing after fishing. Not checked one by one: the volume of each of the other sounds that now play.

## Notes

- Slots that leaked before the Mod was installed, or in a session without it, stay until the game is restarted.
- The game updates the 3D position of these sounds every frame (`0x6a2f50`); without IDirectSound3DBuffer that update does nothing, as before.
