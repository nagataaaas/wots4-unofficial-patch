# Empty corpses remain searchable

- Status: implemented, needs in-game confirmation
- Code: `src/empty_corpse_loot.h`, `src/mod.cpp` (`findLootableCorpse`, `buildCorpseLootHook`); hooks at `0x4a0560` and `0x4f57f0`
- Tests: `empty_corpse_loot_native_probe` (64 passed, 0 failed); full Mod suite: 26 tests, 729 checks passed, 0 failed
- Last updated: 2026-10-16

## Problem

A corpse can remain searchable even though searching or kicking it never gives an item.
One reported case is Madara on Hard or Harakiri after his automatic part drop when the player
already owns his style. The fix applies to all characters and difficulties with no obtainable loot.

## Cause

The corpse search checks whether the body has been looted, without checking whether any drop
candidates remain. Automatic drops can consume the available parts without setting that looted
flag. Owned style books and absent items are excluded from later draws. A draw with no candidates
returns the same failure as an ordinary missed roll, leaving the empty body searchable.

## Possible fixes

1. Mark every body looted after its automatic drop.
   - Cheap, but would also prevent obtaining books and other items that remain.
2. Fix only the reported character and difficulty.
   - Covers that example, but leaves the same bug on other bodies.
3. Check the game's remaining candidates during corpse selection and mark empty candidate lists.
   - Covers all characters, difficulties and drop tables, including bodies that were already empty.
   - Adds a candidate check when selecting a corpse. It must not run the random draw or consume loot.

## Chosen fix

Approach 3. Corpse selection uses the character's own drop table and the game's candidate builder
to check the remaining items without drawing random numbers or removing items. An empty corpse
is marked looted and the search continues, so it can select another nearby body with loot.
The shared candidate builder also marks an empty corpse when called by a loot, kick or death draw.
Living characters are not marked looted. Bodies with remaining candidates retain the original odds.

Both hook ranges are reserved through the ModLoader and installed together. Startup checks verify
the relevant code and field accesses. Unsupported code or a conflicting reservation skips this fix.
Use `[empty_corpse_loot] enabled=0` to disable it; the default is `1`.

The native test runs the original death drop, draw, ownership check and corpse search on a private
copy of the executable, with synthetic characters and item tables. It checks empty and nonempty
bodies across all difficulties, different characters and tables, retained items and books, nearby
bodies, ordinary missed rolls, configuration and startup guards. No game files or saves are changed.

## Notes

- The fix excludes an empty corpse; it does not create additional loot or change drop rates.
- The shared random-draw entry and its two roll sites remain unchanged for compatibility with
  tweaks' Guaranteed Loot. An empty list still has no item to give.
- The check allocates no memory, performs no file I/O and logs nothing during corpse selection.
  Gameplay performance has not been measured.
- In-game behavior, including the looting animation and interaction with Guaranteed Loot, needs
  in-game confirmation.
