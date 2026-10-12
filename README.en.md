# unofficial_patch

[日本語](README.md)

> [!NOTE]
> This Mod is for the [Steam version of Way of the Samurai 4](https://store.steampowered.com/app/312780/4/?l=english) only.

An unofficial patch for Way of the Samurai 4. It fixes crashes and bugs to improve the play experience.

## Features

It fixes the following problems. Detailed explanations of each fix (what the problem is, why it happens, the fixes considered, the fix chosen, and notes) are in [`docs/fixes/`](docs/fixes/).

1. The game crashes at the true-route ending (right after defeating Kinugawa) when you resume from the photographer in the jail ([Details](docs/fixes/jail_ending_crash.md))
2. In "The Fighting Spirit Never Dies", when Madara rescues you, the confiscated weapons, items and outfit are not given back ([Details](docs/fixes/madara_rescue_property.md))
   - Normal play never takes the rescue branch; it happens only when the flag is written with a save editor or similar.
   - Weapons in a save already made in this state cannot be restored.
3. When the depth of field (DoF) setting is on, effects such as the glow (bloom) and the effect when you kick someone get a white or black box around them ([Details](docs/fixes/effect_box.md))
4. In full screen, the screen turns white and freezes for a moment every time you return to the window, and on some displays it appears to spread past the screen ([Details](docs/fixes/focus_white_screen.md))
   - It happened when Windows scaling is not 100%.
5. After a night visit (yobai), returning to the title without saving and loading a save with a night visit promise immediately runs the end-of-night-visit process and loses the promise ([Details](docs/fixes/yobai_result_after_title.md))

No further fixes are planned at the moment.
If there is a bug you would like fixed, please open an Issue in this repository and it may be addressed.

## Installation

> [!WARNING]
> Install [wots4-mods (ModLoader)](https://github.com/nagataaaas/wots4-mods) first.

1. Close the game.
2. Download `wots4-mod-unofficial-patch-<version>.zip` from [Releases](../../releases) and extract it.
3. Copy the extracted `mods` folder into the game's installation folder (the folder containing `WayOfTheSamurai4.exe`). If a `mods` folder already exists, merge the contents into it.
4. Confirm that `{game installation folder}/mods/unofficial_patch/mod.dll` exists, then start the game.

To keep your current settings when updating, replace only `mod.dll` and do not overwrite `mods/unofficial_patch/config.ini`.

## Settings

Close the game and edit `{game installation folder}/mods/unofficial_patch/config.ini`. Save the file and start the game to apply the settings.

Each fix has its own section. `enabled` set to `1` installs the fix, and `0` leaves the game's original behavior. A value other than `1` or `0` is treated as `1`. Without the file or a key, every fix is installed.

| Section | Key | Default | Description |
| --- | --- | --- | --- |
| `follow_detach` | `enabled` | `1` | 1. Fixes the crash at the ending after resuming from the photographer in the jail. |
| `property_return` | `enabled` | `1` | 2. Fixes property not being given back after Madara's rescue. |
| `translucent_depth` | `enabled` | `1` | 3. Fixes white or black boxes around effects. |
| `physical_screen_check` | `enabled` | `1` | 4. Fixes the white screen and freeze on returning to the window (resolution check). |
| `skip_full_screen_reapply` | `enabled` | `1` | 4. Fixes the white screen and freeze on returning to the window (skip applying full screen again). |
| `no_white_background` | `enabled` | `1` | 4. Fixes the white screen and freeze on returning to the window (no white background). |
| `yobai_result_reset` | `enabled` | `1` | 5. Fixes the night visit promise being lost when loading after returning to the title. |

## Building

> [!NOTE]
> You do not need this just to use the Mod. Refer to it only if you want to fix or develop it.

It is usually built inside [wots4-mods](https://github.com/nagataaaas/wots4-mods). To build only this Mod, run CMake with `-DWOTS4_SDK_DIR=<path to wots4-mods>`.
