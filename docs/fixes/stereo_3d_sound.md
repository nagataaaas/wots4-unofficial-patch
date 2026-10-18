# Sound effects that never play, and voices going silent after fishing

- Status: confirmed in game
- Code: `src/mod.cpp` (`playSound3dHook`, `stereo3dSoundCodeMatches`), hook at `0x6a3270`
- Tests: `stereo_3d_sound_native_probe` (15 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-18

## Problem

65 sound effects are never heard at all (the [list](#sound-effects-that-never-played) is below), for example the fishing splashes, doors and gates opening and closing, explosions, fireworks, the gambling dens, and the torture devices (the water wheel, hanging and stone embrace).

After a few casts or catches when fishing, the voices of shopkeepers and other characters are no longer heard either, and some other sound effects go missing. It lasts until the game is restarted.

For reference, fishing before and after the fix (852x480 with sound; turn the sound on). Before the fix the splashes are not heard. After the fix they are.

| Before | After |
| --- | --- |
| <video src="../assets/stereo_3d_sound/unfixed.mp4" controls width="400"></video><br>[unfixed.mp4](../assets/stereo_3d_sound/unfixed.mp4) | <video src="../assets/stereo_3d_sound/fixed.mp4" controls width="400"></video><br>[fixed.mp4](../assets/stereo_3d_sound/fixed.mp4) |

## Cause

Streamed sounds may use at most 5 of the game's 24 sound slots at once, music included (`0x5a05c0`). When 5 are in use, a new streamed voice or sound effect is dropped. A slot is freed when the slot update sees its sound stop playing (`0x6a2480` sends the end notification `0x5a1950` only after the channel played at the last update).

Some sound effects are both spatial (3D) and streamed from a stereo file, for example the fishing splashes (`fishing1`, `fishing2`, `up_water`). The stream's DirectSound buffer gets 3D control only when the file is mono, so for a stereo file the 3D play (`0x6a3270`) fails to get IDirectSound3DBuffer and returns `E_NOINTERFACE` before Play. The caller ignores the error and keeps the slot as playing. The sound never plays, so it never ends, and its slot is never freed.

Looping sounds of this kind are stopped by their code and freed by the [sound slot release](sound_slot_leak.md) fix. One-shot sounds such as the splash of a cast (`fishing1`) and of a catch (`up_water`) are never stopped, so each one takes a slot for good. With the music, 4 of them fill the 5 streams, and streamed voices go silent.

65 sound effects of the Japanese data (`SoundEffectInfo.l`) are stereo streams marked as spatial, so all of them stay silent, and places other than fishing can leak slots the same way.

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

## Sound effects that never played

The sound effects of the Japanese data that are streamed, spatial and stereo, by sound effect ID. What each one is was guessed from its file name and has not been checked by listening. "Loop" marks sounds that play until stopped; the length is one pass of the file.

Fishing and torture:

| ID | File | Likely use | Loop | Length |
| --- | --- | --- | --- | --- |
| 351 | `fishing1` | Fishing: splash of a cast | | 0.3 s |
| 352 | `fishing2` | Fishing: water around the float | Loop | 1.9 s |
| 303 | `up_water` | Splash of a catch | | 1.3 s |
| 359 | `water_mill` | Torture: water wheel | Loop | 3.7 s |
| 360 | `stake` | Torture: hanging | Loop | 7.4 s |
| 361 | `stone_embrace1` | Torture: stone embrace | | 0.3 s |
| 320 | `torture_remove` | Release from torture | | 1.4 s |

Doors, gates and devices:

| ID | File | Likely use | Loop | Length |
| --- | --- | --- | --- | --- |
| 374, 375 | `door_open`, `door_close` | Door opening and closing | | 0.8 s, 0.5 s |
| 348, 349 | `sliddoor_close`, `sliddoor_open` | Sliding door | | 0.4 s, 0.7 s |
| 382, 383, 414 | `gate_open2`, `gate_close2`, `gate_close3` | Gate opening and closing | | 3.2 s, 3.6 s, 2.7 s |
| 384 | `jail_open` | Jail door | | 5.1 s |
| 319 | `lock_open` | Unlocking | | 2.1 s |
| 294 | `open_floor` | Opening the floor (a hidden door) | | 0.5 s |
| 300 | `pulley` | Pulley | Loop | 2.8 s |
| 305, 306 | `wire_slip`, `wire_cut` | Rope slipping, rope cut | Loop (305) | 2.0 s, 0.4 s |
| 322, 323 | `fense_fall`, `stop_fall` | Fence falling, fall stopping | | 6.3 s, 1.0 s |
| 307, 308, 309 | `chimney_hit`, `chimney_fall`, `chimney_down` | Chimney hit and falling | | 3.1 s, 6.9 s, 6.3 s |
| 377, 354 | `bed_flip`, `nc_bed_flip` | Bed flipped over | | 0.9 s, 6.2 s |

Explosions, guns and fireworks:

| ID | File | Likely use | Loop | Length |
| --- | --- | --- | --- | --- |
| 311, 312 | `bs_explosion`, `bs_large_explosion` | Explosion, large explosion | | 3.3 s, 8.9 s |
| 313 | `bs_fireball` | Fireball | Loop | 9.5 s |
| 310 | `bs_simmers` | Smouldering | Loop | 2.6 s |
| 314 | `bs_sink` | Sinking | Loop | 21.3 s |
| 292 | `toguro_explosion` | Explosion | | 0.9 s |
| 316 | `draw_cannon` | Pulling a cannon | Loop | 2.7 s |
| 296 | `teppou_setup` | Readying a gun | | 0.5 s |
| 289, 290 | `fly_fireworks2`, `up_fireworks2` | Fireworks rising and bursting | | 1.4 s, 0.8 s |

Gambling and games:

| ID | File | Likely use | Loop | Length |
| --- | --- | --- | --- | --- |
| 346, 347 | `tobaku1`, `tobaku2` | Gambling | | 0.8 s, 0.3 s |
| 378, 379 | `roulette1`, `roulette2` | Roulette | Loop | 4.3 s, 2.1 s |
| 299, 324 | `cardputter`, `card_dance` | Placing cards, cards flying | Loop (299) | 0.9 s, 0.7 s |
| 355 | `nc_bingo` | Bingo | | 1.6 s |

Crowds, battles and surroundings:

| ID | File | Likely use | Loop | Length |
| --- | --- | --- | --- | --- |
| 195, 196 | `gy_battle1`, `gy_battle2` | Battle noise | | 60.1 s, 31.0 s |
| 405, 406 | `gy_battle3`, `gy_battle4` | Battle noise | Loop | 13.5 s, 7.5 s |
| 190, 276 | `gy_enkai` | Banquet chatter (one file, two IDs) | Loop (276) | 5.9 s |
| 403, 404 | `kenjutu_buzz`, `kenjutu_cheer` | Sword match crowd, cheers | Loop | 3.5 s, 4.5 s |
| 183 | `amb_bell2` | Bell | | 5.3 s |
| 388 | `japan_drum` | Drum | | 1.3 s |
| 204 | `horseshoe` | Horse hooves | | 19.9 s |
| 317 | `militaryshoes` | Marching boots | Loop | 2.2 s |
| 281 | `Row_boat` | Rowing a boat | Loop | 2.9 s |

Others:

| ID | File | Likely use | Loop | Length |
| --- | --- | --- | --- | --- |
| 288 | `telegraphic2` | Telegraph | Loop | 3.3 s |
| 291 | `teacup` | Teacup | | 0.4 s |
| 295 | `transform` | Transformation | | 2.6 s |
| 315 | `map_put` | Putting down a map | | 3.0 s |
| 318 | `whistle` | Whistle | | 0.5 s |
| 321 | `pour_your` | Pouring | | 6.1 s |
| 350 | `wepon_melt` | Melting a weapon (blacksmith) | | 2.8 s |
| 397 | `camera` | Camera (the photographer) | | 0.3 s |

## Notes

- Slots that leaked before the Mod was installed, or in a session without it, stay until the game is restarted.
- The game updates the 3D position of these sounds every frame (`0x6a2f50`); without IDirectSound3DBuffer that update does nothing, as before.
- Two pieces of music are also stereo streams marked as spatial (BGM 38 `S4_M37_Release` and 40 `S3_M19_Battle_Normal`). Whether music is ever played with a position, which is what makes it go through the 3D play, has not been checked; they are not counted above. No character voice matches.
