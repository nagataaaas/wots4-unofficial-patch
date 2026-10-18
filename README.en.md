# unofficial_patch

[日本語](README.md) | [Changelog](CHANGELOG.en.md)

> [!NOTE]
> This Mod is for the [Steam version of Way of the Samurai 4](https://store.steampowered.com/app/312780/4/?l=english) only.

An unofficial patch for Way of the Samurai 4. It fixes crashes and bugs to improve the play experience.

## Features

It fixes the following problems. Detailed explanations of each fix (what the problem is, why it happens, the fixes considered, the fix chosen, and notes) are in [`docs/fixes/`](docs/fixes/).

1. The game crashes at the true-route ending (right after defeating Kinugawa) when you resume from the photographer in the jail ([Details](docs/fixes/jail_ending_crash.md))
2. After opening Options on the title screen, choosing New Game, or loading a save made after the end of a playthrough with Load, can crash the game ([Details](docs/fixes/title_select_window.md))
3. Switching to another window can crash the game ([Details](docs/fixes/focus_change_crash.md))
   - Instead of crashing, the screen stops updating for a moment and then recovers.
   - Leaving and returning to the window also becomes smoother, and the game no longer freezes or crashes at those moments.
4. The game freezes for minutes when starting to play, or the first time Options is opened ([Details](docs/fixes/display_mode_list.md))
   - It happened on monitors with many display modes, such as 4K or high refresh rate monitors.
5. On stereo output, voices and sound effects near the player in the field are nearly silent and only sounds behind the camera are heard ([Details](docs/fixes/listener_orientation.md))
   - They can be heard without turning on a spatial sound such as Windows Sonic.
6. When naming a sect or a sword, typing "e" (as in え or け) with the IME closes the input because E is the Back key, and Japanese characters are taken or dropped at random ([Details](docs/fixes/text_input.md))
   - While typing, keyboard keys no longer act as buttons (E = Back, Space = OK). On the keyboard, Enter accepts and Esc goes back, as in the player name input.
   - Also lets the player's name take some kanji such as 鬼, 郎 and 龍, and fullwidth letters and digits, which it dropped.
7. More than 65 sound effects never play ([Details](docs/fixes/stereo_3d_sound.md))
   - Fishing splashes, doors and gates opening and closing, explosions, fireworks, gambling dens, and torture (the water wheel, hanging and stone embrace) can now be heard.
   - These sounds do not get quieter with distance or move between left and right, and sound as if they were close by.
8. After fishing, the voices of shopkeepers and others stop playing ([Details](docs/fixes/stereo_3d_sound.md))
9. After torture or an arrest, the fishing success sound and the voices of shopkeepers and others stop playing, and opening or closing a menu plays the torture sound ([Details](docs/fixes/sound_slot_leak.md))
10. In full screen, the screen turns white and freezes for a moment every time you return to the window, and on some displays it appears to spread past the screen ([Details](docs/fixes/focus_white_screen.md))
    - It happened when Windows scaling is not 100%.
11. When the depth of field (DoF) setting is on, effects such as the glow (bloom) and the effect when you kick someone get a white or black box around them ([Details](docs/fixes/effect_box.md))
12. Fullwidth digits in dialogue and item descriptions appear as solid black boxes ([Details](docs/fixes/fullwidth_digits.md))
    - Numeric key images in input reassignment and tutorials are preserved. The numeric key images still need in-game confirmation.
13. After a night visit (yobai), returning to the title without saving and loading a save with a night visit promise immediately runs the end-of-night-visit process and loses the promise ([Details](docs/fixes/yobai_result_after_title.md))
14. A corpse with no obtainable items can still be searched over and over without yielding anything ([Details](docs/fixes/empty_corpse_loot.md))
    - Excludes empty corpses regardless of character or difficulty. Bodies with an obtainable book or item remain searchable.
15. In "The Fighting Spirit Never Dies", when Madara rescues you, the confiscated weapons, items and outfit are not given back ([Details](docs/fixes/madara_rescue_property.md))
    - Normal play never takes the rescue branch; it happens only when the flag is written with a save editor or similar.
    - Weapons in a save already made in this state cannot be restored.

No further fixes are planned at the moment.
If there is a bug you would like fixed, please open an Issue in this repository and it may be addressed.

## Installation

### Automatic installation (recommended)

1. Close the game.
2. Download and run [WayOfTheSamurai4ModInstaller.exe](https://github.com/nagataaaas/wots4-mods/releases/latest/download/WayOfTheSamurai4ModInstaller.exe).
   - If "Windows protected your PC" appears, click "More info" → "Run anyway".
   - When asked whether to allow it to make changes as administrator, click "Yes" (it writes to the game folder).
3. Follow the screen and choose `unofficial_patch` in the list of Mods. Move with Up and Down, choose with Space and confirm with Enter.

The ModLoader is installed along with it. The same steps update the Mod, and your settings (`config.ini`) are kept.

<details>
<summary>Manual installation</summary>

> [!WARNING]
> Install [wots4-mods (ModLoader)](https://github.com/nagataaaas/wots4-mods) first.

1. Close the game.
2. Download `wots4-mod-unofficial-patch-<version>.zip` from [Releases](../../releases) and extract it.
3. Copy the extracted `mods` folder into the game's installation folder (the folder containing `WayOfTheSamurai4.exe`). If a `mods` folder already exists, merge the contents into it.
4. Confirm that `{game installation folder}/mods/unofficial_patch/mod.dll` exists, then start the game.

To keep your current settings when updating, replace only `mod.dll` and do not overwrite `mods/unofficial_patch/config.ini`.

</details>

## Uninstallation

1. Close the game.
2. Delete the `{game installation folder}/mods/unofficial_patch/` folder. The settings in it (`config.ini`) are deleted too.

## Settings

Close the game and edit `{game installation folder}/mods/unofficial_patch/config.ini`. Save the file and start the game to apply the settings.

Each fix has its own section. `enabled` set to `1` installs the fix, and `0` leaves the game's original behavior. A value other than `1` or `0` is treated as `1`. Without the file or a key, every fix is installed.

| Section | Key | Default | Description |
| --- | --- | --- | --- |
| `follow_detach` | `enabled` | `1` | 1. Fixes the crash at the ending after resuming from the photographer in the jail. |
| `title_select_window` | `enabled` | `1` | 2. Fixes the crash after opening Options on the title screen. |
| `render_target_recovery` | `enabled` | `1` | 3. Fixes the crash when switching to another window. |
| `display_mode_list` | `enabled` | `1` | 4. Fixes the freeze of several minutes when starting to play or opening Options. |
| `listener_orientation` | `enabled` | `1` | 5. Fixes sounds near the player being inaudible on stereo output. |
| `text_input_keyboard` | `enabled` | `1` | 6. Keeps E and Space from closing the sect name and sword name input while typing. |
| `text_input_characters` | `enabled` | `1` | 6. Lets the sect name and sword names take Japanese characters. |
| `player_name_characters` | `enabled` | `1` | 6. Lets the player's name take some kanji and fullwidth letters and digits. |
| `stereo_3d_sound` | `enabled` | `1` | 7, 8. Plays the sound effects that never played, and keeps voices from going silent after fishing. |
| `sound_slot_release` | `enabled` | `1` | 9. Keeps sound effects and voices from going silent, for example after torture. |
| `physical_screen_check` | `enabled` | `1` | 10. Fixes the white screen and freeze on returning to the window (resolution check). |
| `skip_full_screen_reapply` | `enabled` | `1` | 10. Fixes the white screen and freeze on returning to the window (skip applying full screen again). |
| `no_white_background` | `enabled` | `1` | 10. Fixes the white screen and freeze on returning to the window (no white background). |
| `translucent_depth` | `enabled` | `1` | 11. Fixes white or black boxes around effects. |
| `fullwidth_digits` | `enabled` | `1` | 12. Fixes fullwidth digits while preserving numeric key images in control prompts. |
| `yobai_result_reset` | `enabled` | `1` | 13. Fixes the night visit promise being lost when loading after returning to the title. |
| `empty_corpse_loot` | `enabled` | `1` | 14. Keeps corpses with no obtainable items from remaining searchable. |
| `property_return` | `enabled` | `1` | 15. Fixes property not being given back after Madara's rescue. |

## Building

> [!NOTE]
> You do not need this just to use the Mod. Refer to it only if you want to fix or develop it.

It is usually built inside [wots4-mods](https://github.com/nagataaaas/wots4-mods). To build only this Mod, run CMake with `-DWOTS4_SDK_DIR=<path to wots4-mods>`.
