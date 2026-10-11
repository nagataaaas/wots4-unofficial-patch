# <Fix title>

<!--
Copy this file to docs/fixes/<fix-name>.md (lower_snake_case, e.g. jail_ending_crash.md)
and fill in every section. Remove these comments when done.

This file is public. Keep the technical part short: the addresses the code needs and a
brief reason. Detailed analysis (how it was found, dumps, disassembly) stays in
research/notes/ and is not linked from here.

Write "needs in-game confirmation" for anything not yet checked in the game.
-->

- Status: <investigating | implemented, needs in-game confirmation | confirmed in game | dropped>
- Code: `src/<file>` (`<function>`), hook at `0x<address>`
- Tests: `<test name>` (N passed, 0 failed)
- Last updated: YYYY-MM-DD

## Problem

<!--
What the player sees (UX view). No internals here.
- When and where it happens (game progress, map, save state, settings)
- What happens (crash, freeze, wrong display, ...), and how often
- Steps to reproduce, if known
- Known workarounds without the Mod
-->

## Cause

<!--
Why it happens (technical view).
- What goes wrong in the game code, with the addresses involved
- Why it only happens under the conditions above
- What is still unknown
-->

## Possible fixes

<!--
Every approach considered, including rejected ones.
For each: what it changes, pros, cons and risks (side effects, other code paths,
performance in per-frame code).
-->

1. <Approach A>
   - Pros:
   - Cons / risks:
2. <Approach B>
   - Pros:
   - Cons / risks:

## Chosen fix

<!--
Which approach was taken and why it beats the others.
- What the Mod changes in memory, and when (hooked function, timing)
- Safety checks (byte patterns checked at startup, what happens if they do not match)
- What the player may notice (visual side effects, log lines in mod_loader.log)
- How it was verified, and what still needs in-game confirmation
-->

## Notes

<!--
Anything else: related bugs, remaining unknowns, things to watch for after a game update,
ideas for later.
-->
