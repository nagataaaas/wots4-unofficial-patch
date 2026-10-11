# bug_fix

[日本語](README.md)

A Mod that fixes crashes and bugs in Way of the Samurai 4.

It is not in a Release yet.

## Fixes

Detailed explanations of each fix (what the problem is, why it happens, the fixes considered, the fix chosen, and notes) are in [`docs/fixes/`](docs/fixes/).

1. The game crashes at the true-route ending (right after defeating Kinugawa) when you resume from the photographer in the jail (confirmed in game. [Details](docs/fixes/jail_ending_crash.md))
   - Effects attached to a character's joints (auras, etc.) kept pointing at the joints after the character's model was freed, and the game crashed when it read a freed joint. When a character's effects are detached (`0x48d520`), this Mod also detaches every effect attached to that model's joints. The detached effects stay where they are.
   - When it detaches effects, it writes `detached N effect(s)` to `mod_loader.log`.
2. In "The Fighting Spirit Never Dies", when Madara rescues you (save `0x3760` is 1), the confiscated weapons, items and outfit are not given back (confirmed in game. [Details](docs/fixes/madara_rescue_property.md))
   - Normal play never sets this flag to 1; it happens only when the flag is written with a save editor or similar. Strictly speaking, this is less a bug fix than making an unfinished branch consistent with the normal one.
   - The restore (`PlayerGiveProperty`) exists only in the scene after defeating the three sisters, and the rescue branch never called it. The weapons are kept in memory that is not saved, so saving and loading in this state loses them. When the script moves to the scene after the rescue (`BR_14_00_S210`, at `0x55e140`) while the property is still taken, this Mod calls the game's own restore.
   - When it gives them back, it writes `returned the player's weapons, items and outfit after Madara's rescue` to `mod_loader.log`.
   - Weapons in a save already made in this state cannot be restored.
3. Effects such as the glow (bloom) and the effect when you kick someone get a white or black box around them (confirmed in game. [Details](docs/fixes/effect_box.md))
   - The scene is drawn to two textures at once: the color, and the depth that depth of field (DoF) reads. An effect's square quad changed the depth of its whole rectangle, transparent parts included, so the DoF blur and contrast changed in the shape of the quad. While something is drawn with alpha blending and without writing the depth buffer (an effect), this Mod keeps the depth texture unwritten (by hooking `IDirect3DDevice9::SetRenderState`).
   - The first version used blending alone as the condition, so characters, which are also drawn with blending, became see-through. With this version's narrower condition, the boxes are gone and characters are opaque, confirmed in the game.
   - When it is installed, it writes `translucent depth mask installed` to `mod_loader.log`.
4. In full screen, the screen turns white, freezes for a moment and spreads past the display every time you return to the window (confirmed in game: returning is much faster and the white flash is gone. [Details](docs/fixes/focus_white_screen.md))
   - The game is not DPI aware, so when Windows scaling is not 100%, it gets the screen size before scaling (3072x1728 for 3840x2160 at 125%). Its "the screen already has the wanted resolution, do not change it" check never matched, so it changed the resolution every time you returned. While that check runs (`0x5383d0`), this Mod makes it see the screen size in physical pixels.
   - The game also applied full screen again every time you returned (`0x538180`): it made the window larger than the screen before fixing it, and reset the render device. When neither the resolution nor the window changed, this Mod skips that. A device that was really lost is still recovered by the game's renderer at the next frame.
   - Each time you return, it writes `display mode check: ...` and `full screen re-apply skipped: ...` (or `full screen applied again: <reason>`) to `mod_loader.log`.
   - As a safety net, this Mod also stops painting the window background white (confirmed in game). The game's window has a white background, and when it was repainted while full screen had to be applied again (a resolution in the settings that differs from the screen, Alt+Enter and so on) it showed white. This Mod answers the request to paint the background (`WM_ERASEBKGND`) without painting, so the last frame stays (`0x5384c0`).
5. After a night visit (yobai), returning to the title without saving and loading a save with a night visit promise drops the old reward ("token of love" or "consolation gift") again and loses the promise (needs in-game confirmation. [Details](docs/fixes/yobai_result_after_title.md))
   - The game keeps the result of the last night visit after handing it out, and clears it only when you save. Returning to the title cleared only the "handed out" mark, so the first map change after loading handed the old result out again. When the game ends the play session (`0x4e18e0`), this Mod clears the result as well, as saving does.
   - When it clears a result, it writes `discarded the night visit result N` to `mod_loader.log`.

## Planned

1. Other crashes
2. Other display bugs

## Settings (config.ini)

`mods/bug_fix/config.ini` has a section for each fix (`[<fix name>]`). `enabled=1` installs the fix, `0` does not (the game stays as it is). Without the file or a key, every fix is installed.

| Section | Fix |
| --- | --- |
| `[follow_detach]` | 1. Crash at the ending after resuming from the photographer in the jail |
| `[property_return]` | 2. Property not given back after Madara's rescue |
| `[translucent_depth]` | 3. White or black boxes around effects |
| `[physical_screen_check]` | 4. White screen and freeze on returning to the window (resolution check) |
| `[skip_full_screen_reapply]` | 4. White screen and freeze on returning to the window (skip applying full screen again) |
| `[no_white_background]` | 4. White screen and freeze on returning to the window (no white background) |
| `[yobai_result_reset]` | 5. Night visit reward handed out again after returning to the title |

A fix that is not installed is written to `mod_loader.log` as `[<fix name>] disabled in config.ini`. A value other than `1` or `0` is treated as `1`, and the log says so. The settings are read when the game starts.

## Adding or changing a fix

1. Copy [`docs/fixes/TEMPLATE.md`](docs/fixes/TEMPLATE.md) to `docs/fixes/<fix name>.md` and fill in every section.
2. When you write or change the code of a fix, update its document in the same change (status, cause, chosen fix, test results, etc.).
3. Update the "Fixes" list above as well.
4. Add a section for the fix to `config.ini`, and update the "Settings" table above.

## Building

It is usually built inside [wots4-mods](https://github.com/nagataaaas/wots4-mods). To build only this Mod, run CMake with `-DWOTS4_SDK_DIR=<path to wots4-mods>`.
