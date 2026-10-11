# White or black boxes around effects

- Status: confirmed in game (the first version made characters see-through; the narrowed condition was confirmed 2026-10-12)
- Code: `src/mod.cpp` (`setRenderStateHook`, `installTranslucentDepthMask`), `src/translucent_depth.h` (`TranslucentDepthMask`); hooks `IDirect3DDevice9::SetRenderState` of the game's device, or `IDirect3D9::CreateDevice` until the device exists
- Tests: `translucent_depth_probe` (5 passed, 0 failed), `translucent_depth_native_probe` (19 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-12

## Problem

Some effects, such as the glow (bloom) and the effect when you kick a character, show a square box around them. The box is white against a bright sky and black against dark backgrounds.

With `EnableDOF=0` in `S4-SETTINGS.ini` the boxes are gone, but so is depth of field.

For reference, the same part of one scene before and after the fix (cropped from 3840x2160 screenshots, taken with the first version of the fix). Before the fix, a dark box floats above the wall, to the upper right of the "写真家" (photographer) label. After the fix it is gone.

| Before | After |
| --- | --- |
| ![Before the fix: a dark box above the wall](../assets/effect_box/unfixed.jpg) | ![After the fix: no box](../assets/effect_box/fixed.jpg) |

## Cause

- The scene is drawn to two render targets at once (set up at `0x541930`): the color, and a depth texture (render target 1) that depth of field reads.
- Every pixel shader of the scene material (`ColorDepthMaterial`) writes depth to `COLOR1` with alpha 1, whatever the texture's alpha is.
- In Direct3D 9, alpha blending and the color write mask apply to all render targets unless set separately, and the game never sets the mask of render target 1. So an effect quad changes the depth of its whole rectangle, transparent texels included: normal blending overwrites it, and additive blending adds to it.
- Depth of field (`DoF.fx`) uses that depth to choose how much to blur each pixel and, in one technique, how much to change its contrast. Only the rectangle of the quad changes, so a box appears that is brighter on bright backgrounds and darker on dark ones.

## Possible fixes

1. While alpha blending is on, do not write render target 1 (`D3DRS_COLORWRITEENABLE1 = 0`), by hooking `IDirect3DDevice9::SetRenderState`.
   - Pros: one extra render state call only when blending is switched, nothing per draw. No game file is changed.
   - Cons / risks: tried first and dropped. Characters (the player and NPCs) are also drawn with blending on, so their depth was lost and whatever stood behind them showed through them in game.
2. Mask render target 1 when Z writing is off (`D3DRS_ZWRITEENABLE = FALSE`).
   - Pros: matches the Z buffer exactly.
   - Cons / risks: the sky or other backgrounds drawn without Z writing would also lose their depth.
3. Mask render target 1 only while alpha blending is on and Z writing is off, by hooking `IDirect3DDevice9::SetRenderState`.
   - Pros: effects are blended and leave the Z buffer alone, so they are masked; characters write Z and keep writing depth; opaque backgrounds are untouched. One extra render state call only when either state changes. Needs `D3DPMISCCAPS_INDEPENDENTWRITEMASKS`, which current GPUs have.
   - Cons / risks: an effect drawn with Z writing on still writes its whole quad. Something blended without Z writing that should give depth of field its depth (water, for example) keeps the depth behind it.
4. Replace the shaders in memory so that `COLOR1` carries the texture's opacity.
   - Pros: transparent texels keep the depth behind them, opaque texels still write theirs.
   - Cons / risks: many generated shaders to replace, and additive blending would still add depth. Much larger change.
5. Tell players to set `EnableDOF=0`.
   - Pros: no Mod needed.
   - Cons / risks: depth of field is lost.

## Chosen fix

Approach 3 (approach 1 was the first version and made characters see-through).

- When the Mod loads, it reads the game's device from the renderer (`[0x9e0890]` → `+0x720` → `+0x2238`) and hooks its `SetRenderState`. If the renderer has not created the device yet, it hooks `IDirect3D9::CreateDevice` of `d3d9.dll` (loaded by the same name as the game does) and hooks `SetRenderState` right after the game's device is created.
- In the hook, `D3DRS_ALPHABLENDENABLE` and `D3DRS_ZWRITEENABLE` are passed on, then `D3DRS_COLORWRITEENABLE1` is set to 0 while blending is on and Z writing is off, and to the mask the game asked for (all channels by default) otherwise. A `D3DRS_COLORWRITEENABLE1` set by the game while masked is remembered and applied when the mask ends. Other states pass through unchanged.
- At startup the Mod checks the bytes at `0x6e6466`, `0x88e0f0` and `0x88e109` (where the renderer pointer, the render state and the device offset come from) and the GPU capability `D3DPMISCCAPS_INDEPENDENTWRITEMASKS`. If any differs, this fix is skipped and the reason is written to `mod_loader.log`; the other fixes still load.
- `mod_loader.log` gets `translucent depth mask installed on IDirect3DDevice9::SetRenderState`, or `translucent depth mask waits for the game to create its Direct3D device` followed by the installed line when the device is created.
- Verified without the game: on a mapped copy of the exe with a fake device where the renderer keeps it, the hook gives the expected render states (a blended draw with Z writing keeps render target 1, a blended draw without Z writing hides it, other states pass through), and the Mod skips the fix on a GPU without separate write masks.
- In the game, with the first version (approach 1): the boxes were gone with depth of field on, water looked the same as before, and `mod_loader.log` showed the installed line without the waiting line, so the device already existed when the Mod loaded (confirmed 2026-10-12). The player, NPCs and what stood behind NPCs showed through characters; with this fix off in `config.ini` they did not.
- In the game, with this version: the boxes are gone and the player, NPCs and what stands behind them are opaque again (confirmed 2026-10-12).
- The cost per call was not measured; the hook adds two comparisons to each `SetRenderState` and one call when blending or Z writing changes.

## Notes

- If a d3d9 wrapper (ReShade, DXVK and so on) is in the game folder, the hook is placed on the wrapper's device. Not tested.
- If the game's device is replaced by a new one of another class, the new device is not hooked. The game creates one device at startup.
