# bug_fix

[日本語](README.md)

A Mod that fixes crashes and bugs in Way of the Samurai 4.

It is not in a Release yet.

## Fixes

Detailed explanations of each fix (what the problem is, why it happens, the fixes considered, the fix chosen, and notes) are in [`docs/fixes/`](docs/fixes/).

1. The game crashes at the true-route ending (right after defeating Kinugawa) when you resume from the photographer in the jail (confirmed in game. [Details](docs/fixes/jail_ending_crash.md))
   - Effects attached to a character's joints (auras, etc.) kept pointing at the joints after the character's model was freed, and the game crashed when it read a freed joint. When a character's effects are detached (`0x48d520`), this Mod also detaches every effect attached to that model's joints. The detached effects stay where they are.
   - When it detaches effects, it writes `detached N effect(s)` to `mod_loader.log`.

## Planned

1. Other crashes (the screen turns white and freezes when you return to the window, etc.)
2. Display and other bugs (black squares behind particle effects, etc.)

## Adding or changing a fix

1. Copy [`docs/fixes/TEMPLATE.md`](docs/fixes/TEMPLATE.md) to `docs/fixes/<fix name>.md` and fill in every section.
2. When you write or change the code of a fix, update its document in the same change (status, cause, chosen fix, test results, etc.).
3. Update the "Fixes" list above as well.

## Building

It is usually built inside [wots4-mods](https://github.com/nagataaaas/wots4-mods). To build only this Mod, run CMake with `-DWOTS4_SDK_DIR=<path to wots4-mods>`.
