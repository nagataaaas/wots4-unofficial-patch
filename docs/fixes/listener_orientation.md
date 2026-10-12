# Voices and sound effects near the player nearly silent on stereo output

- Status: confirmed in game
- Code: `src/mod.cpp` (`setListenerHook`, `listenerOrientationCodeMatches`), `src/listener_orientation.h` (`correctListenerAxes`), hook at `0x6a2660`
- Tests: `listener_orientation_probe` (9 passed, 0 failed), `listener_orientation_native_probe` (16 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-12

## Problem

In the field, voices and sound effects close to the player are very quiet or silent. Sounds behind the camera (behind the player's back as seen on screen) are heard normally, so it feels as if the listener stands 10 to 15 steps behind the player. Music and cutscene audio are normal.

It happens on stereo output (speakers or a stereo device). Turning on a virtual surround in Windows, such as Windows Sonic for Headphones (Sound settings, the output device's Properties, Spatial sound), makes it go away. Players also report that headphones or a 5.1 speaker setup help.

## Cause

- The game places DirectSound's 3D listener at the camera each frame (`0x59fc60`, which calls `0x6a2660`). It passes the camera's look axis (column 0 of the camera's world rotation, read at `0x59fe89`) as the front and the camera's up axis (column 1, `0x59feb6`) as the top.
- `0x6a2660` negates the three components of the front axis (`0x6a281e`, `0x6a2837`, `0x6a2848`) before `IDirectSound3DListener::SetOrientation`. The listener therefore faces away from the scene, and everything in view, including the player, is behind it.
- With a stereo panner, front and back sound the same, so this does not show. On the PC where this was examined, DirectSound mixes the game's 3D sounds for 7.1 channels even though the output device is stereo. Sounds in view then go to the back and side channels. Those come out nearly silent on the stereo output, unless a virtual surround such as Windows Sonic folds them into the two speakers.
- Still unknown: why DirectSound uses a 7.1 mix on a stereo device, and at which stage of Windows' or the driver's processing the back and side channels are lost.

## Possible fixes

1. Make the listener face the camera's look axis
   - Pros: corrects the actual error with one small hook on the listener update, called once per frame. Sounds in view go to the front channels, which stereo output plays. Players with real surround speakers or a virtual surround also get front and back the right way round.
   - Cons / risks: sounds behind the camera now go to the back channels, so on affected stereo output they may be quieter than before. They are fewer and less important than the sounds in view.
2. Do the 3D panning in the Mod and give DirectSound plain stereo buffers
   - Pros: independent of the channel layout DirectSound picks, so nothing can be lost on stereo output.
   - Cons / risks: replaces the game's buffer creation and every 3D parameter update, and has to reproduce DirectSound's distance attenuation and Doppler. Removes surround for players who have it. Much larger change.
3. Move the listener to the player
   - Pros: sounds near the player get louder.
   - Cons / risks: does not touch the front and back error, so sounds in view still go to the back channels. Changes the balance of distances for everyone.

## Chosen fix

Approach 1. The Mod hooks `0x6a2660` and passes it the negated look axis and the negated up axis. After the game's own negation of the front axis, DirectSound gets the look axis as the front and the up axis upside down. DirectSound takes its right axis from the cross product of the top and front axes, which the two negations leave unchanged, so left and right stay as before. The upside-down top axis does not matter: the speaker panning works on the horizontal plane, and distance and Doppler do not depend on it. The hook only negates six numbers and calls the original; it allocates nothing.

At startup the Mod checks the bytes of `0x6a2660` (prologue, the three negations and the `SetOrientation` call), the caller at `0x59ff1c`, and the camera axes read at `0x59fe89` and `0x59feb6`. If any differs, the fix is skipped and `mod_loader.log` says so. When it is installed, the log has `listener orientation installed at 0x6a2660`.

Tests (the game is not started):

- `listener_orientation_probe` models what DirectSound gets, for 960 combinations of camera and sound directions: with the Mod, a sound in view is in front of the listener, left and right are unchanged, and the distance to the listener is unchanged.
- `listener_orientation_native_probe` runs the game's own `0x6a2660` on a copy of the exe with a fake DirectSound: without the Mod the listener gets the look axis negated; with the Mod it gets the look axis, the position and velocity are passed on unchanged, and the caller's registers and stack are kept.

Confirmed in game: on stereo output without Windows Sonic, voices and sound effects near the player are heard normally.

## Notes

- If sounds behind the camera turn out too quiet on stereo output, approach 2 is the way to fix that part.
- With Windows Sonic or real surround speakers, the game's sounds were front-back reversed before this fix. With the fix, sounds in view are heard in front.
