# Name input: E closes it and Japanese characters are dropped

- Status: confirmed in game
- Code: `src/mod.cpp` (`pollTextInputHook`, `resumeKeyboardButtonsAfterTextInput`; `acceptsTypedCharacter`; `makePlayerNameCharacterPatch`), `src/text_input.h` (`KeyboardMappingPause`, `acceptsTextInputCharacter`); hook at `0x4f4b10`, code changes at `0x4f4be7` and `0x5f7607`
- Tests: `text_input_probe` (52 passed, 0 failed), `text_input_native_probe` (21 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-14

## Problem

- When naming your own sword style (俺流) or a sword, typing romaji with the Japanese IME closes the input as soon as you type "e" (え, け, せ, ...). E is the keyboard's Back key. Space, the IME's conversion key, is also the OK key.
- Japanese characters typed into those names are taken or dropped seemingly at random: あ and 武 go in, but ー, 侍 and 刀 do not, nor do fullwidth letters and digits.
- The player's name at character creation works with the IME, but some characters never go in: kanji from U+8000 up (鬼, 郎, 長, 黒, 龍, ...) and fullwidth letters and digits such as Ａ and １.
- The key bindings can be changed in the options, but in their list E also goes back.

## Cause

- The input system turns keys into pad buttons (key `0x9db630 + 4n` to button `0x9db500 + 4n`; by default Space is OK and E is Back) while its byte `+0x20` (`[0x9e968c]`) is nonzero (`0x4f285a`).
- The player name input turns that byte off when it starts (`0x5f6e1d`) and on when it ends (`0x5f78a7`). The shared text input used for the sect name (`0x5d417f`) and weapon names (`0x6171a6`), polled by `0x4f4b10`, does not, and closes on the Back button (`0x4f4e09`). While the IME composes, typed letters send no character to the game, so only the Back button of E arrives.
- Typed characters come from `WM_CHAR` and are converted to UTF-16 (`0x5366c0`). The shared text input passes each UTF-16 code unit to the C runtime's `isalnum` (`0x4f4be7`). The game sets the user's locale (`setlocale(LC_ALL, "")`), and under code page 932 `isalnum` treats a value above 0xff as Shift_JIS bytes, so the answer depends on the low byte of the character.
- The player name input skips a character with `test ax, ax; jle` (`0x5f7604`), a signed 16-bit test that also skips U+8000 to U+FFFF.

## Possible fixes

1. Turn the keyboard buttons off while the shared text input is polled, as the player name input does.
   - Pros: the game's own mechanism; Enter and Esc still accept and cancel through `WM_CHAR`, and the pad and mouse are unchanged.
   - Cons / risks: the byte must be turned on again even if a screen stops polling without the input finishing.
2. Change the key bindings so that no letter is a button.
   - Pros: no Mod needed.
   - Cons / risks: the player loses those keys elsewhere; in the binding list E goes back.
3. Replace the `isalnum` call with a check made for UTF-16.
   - Pros: one call changes; ASCII can keep the original rule (letters and digits; the game adds the space itself).
   - Cons / risks: characters the font lacks may show blank.
4. Rewrite the whole text input poll in the Mod.
   - Pros: full control.
   - Cons / risks: much larger, and it would copy the sound and mouse handling.
5. Make the player name test unsigned or test only for 0.
   - Pros: a 1-byte change of the jump; 0 never reaches the test (it ends the loop at `0x5f75e9`).
   - Cons / risks: none found; the name input already takes any other character.

## Chosen fix

Fixes 1, 3 and 5, each with its own section in `config.ini`.

Fix 1, keyboard buttons (`[text_input_keyboard]`):

- The Mod hooks `0x4f4b10`. Before each poll it turns the input system's byte `+0x20` off when it is on, and remembers that it did. When the poll returns true (the text was accepted or cancelled) it turns it on again. Once per frame (ModLoader `onFrame`), if it is off because of the Mod and the input was not polled since the previous frame, the Mod turns it on again and logs `text input was closed without finishing; keyboard keys act as buttons again`.
- When something else turned it off (the player name input), the Mod leaves it alone.
- The per-frame work is one byte read and compare; the hook only runs while a name is typed. Not measured.
- At startup the Mod checks the bytes at `0x4f4b10` (prologue), `0x4f285a` (the test of `+0x20`) and `0x5f6e18` (the player name input turning it off, which also checks `[0x9e968c]`). Without `onFrame` in the ModLoader the fix is not installed.

Fix 3, characters (`[text_input_characters]`):

- The Mod replaces `call [isalnum]` at `0x4f4be7` (6 bytes) with a call to its own check and a `nop`. ASCII stays as before: letters and digits (the game itself also takes a space). Other characters are taken unless they are C1 controls (U+0080 to U+009F), surrogates, private use (U+E000 to U+F8FF, where the fullwidth digit fix keeps key images) or U+FFF0 and up.
- At startup the Mod checks the bytes around the call (`0x4f4bd0`, `0x4f4bdc`, `0x4f4bed`) and that it calls the `isalnum` import.

Fix 5, player name (`[player_name_characters]`):

- The Mod changes the `jle` at `0x5f7607` to `je` with the same target, after checking the bytes at `0x5f75e6` and `0x5f75ff`.

`mod_loader.log` gets `text input keyboard buttons installed at 0x4f4b10`, `text input characters installed at 0x4f4be7` and `player name characters installed at 0x5f7607`.

Verified without the game:

- `text_input_probe`: when the keyboard buttons are turned off and on (poll, finished poll, frames with and without a poll, buttons turned off by someone else), and which characters are taken.
- `text_input_native_probe`: runs the game's own poll `0x4f4b10` on a mapped copy of the exe, with characters in the game's character queue and fake input and UI states. Without the Mod (game's C runtime under code page 932) a侍刀ー武 becomes a武, and the keyboard buttons stay on. With the Mod, Japanese, fullwidth and halfwidth kana go in, ASCII symbols and private use do not, the keyboard buttons are off during the poll, on again after Enter or Esc, and on again one frame after the polls stop. The player name change is checked by its bytes only.

Confirmed in game (2026-10-14):

- With the IME, え・け and kana to kanji conversion with Space work in the sect name and sword names.
- Typed Japanese shows in the names and stays after saving and loading.
- Every place that names a sword (blacksmith, dojo) uses this input.
- The player's name takes 鬼, Ａ and similar characters.

## Notes

- The key binding list in the options still goes back with E; this fix does not change it.
- The sect name allows 10 characters and weapon names 18, counted in UTF-16 code units, as before.
