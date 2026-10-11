# bug_fix

[日本語](README.md)

A Mod that fixes crashes and bugs in Way of the Samurai 4.

It is not in a Release yet.

## Fixes

Detailed explanations of each fix (what the problem is, why it happens, the fixes considered, the fix chosen, and notes) are in [`docs/fixes/`](docs/fixes/).

1. The game crashes at the true-route ending (right after defeating Kinugawa) when you resume from the photographer in the jail (confirmed in game. [Details](docs/fixes/jail_ending_crash.md))
   - The ending plays through without crashing.
2. In "The Fighting Spirit Never Dies", when Madara rescues you, the confiscated weapons, items and outfit are not given back (confirmed in game. [Details](docs/fixes/madara_rescue_property.md))
   - Normal play never takes the rescue branch; it happens only when the flag is written with a save editor or similar.
   - After the rescue, your property is given back.
   - Weapons in a save already made in this state cannot be restored.
3. Effects such as the glow (bloom) and the effect when you kick someone get a white or black box around them (confirmed in game. [Details](docs/fixes/effect_box.md))
   - Effects no longer get a box around them. You can keep depth of field (DoF) on.
4. In full screen, the screen turns white, freezes for a moment and spreads past the display every time you return to the window (confirmed in game. [Details](docs/fixes/focus_white_screen.md))
   - It happened when Windows scaling is not 100%.
   - Returning to the window is quick and does not turn white.
5. After a night visit (yobai), returning to the title without saving and loading a save with a night visit promise drops the old reward ("token of love" or "consolation gift") again and loses the promise (needs in-game confirmation. [Details](docs/fixes/yobai_result_after_title.md))
   - The promise in the loaded save stays, and the old reward does not drop.
   - When you die and choose "Retry", the night visit reward is handed out as in the game.

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

A value other than `1` or `0` is treated as `1`. The settings are read when the game starts.

## Adding or changing a fix

1. Copy [`docs/fixes/TEMPLATE.md`](docs/fixes/TEMPLATE.md) to `docs/fixes/<fix name>.md` and fill in every section.
2. When you write or change the code of a fix, update its document in the same change (status, cause, chosen fix, test results, etc.).
3. Update the "Fixes" list above as well.
4. Add a section for the fix to `config.ini`, and update the "Settings" table above.

## Building

It is usually built inside [wots4-mods](https://github.com/nagataaaas/wots4-mods). To build only this Mod, run CMake with `-DWOTS4_SDK_DIR=<path to wots4-mods>`.
