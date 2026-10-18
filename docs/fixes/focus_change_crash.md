# Crash when switching to another window

- Status: confirmed in game
- Code: `src/mod.cpp` (`beginRenderTargetGroupHook`, `restoreLostDeviceHook`, `renderTargetRecoveryCodeMatches`), `src/render_target_recovery.h` (`RenderTargetRecovery`), hooks at `0x88f300` and `0x891f80`
- Tests: `render_target_recovery_probe` (9 passed, 0 failed), `render_target_recovery_native_probe` (20 passed, 0 failed, label `needs_exe`)
- Last updated: 2026-10-18

## Problem

Switching from the game to another window (for example with Alt+Tab or a click on another monitor) sometimes closes the game without a message. It does not happen every time. It was seen twice in full screen on a 4K monitor.

## Cause

- Leaving the window makes the game change the display mode back (`ChangeDisplaySettingsA(NULL, 0)` on `WM_ACTIVATEAPP`), so the Direct3D device has to be reset. Gamebryo's renderer resets it at the start of a frame (`0x891f80` → `0x890e20`).
- In the crashes, the frame was drawn although the back buffer had not been taken back after the reset (its surface was NULL). Which step left it NULL (a failed `Reset`, or `GetRenderTarget` failing after a successful one) is not known yet.
- `NiDX9Renderer::BeginUsingRenderTargetGroup` (`0x88f300`) handles a render target it cannot set by calling itself with the default group, the back buffer (`0x88f454`). When the back buffer is the one that fails, that call fails the same way and calls itself again, until the stack overflows (`0xc00000fd`).

## Possible fixes

1. Stop the fallback when it asks again for the group that is failing, and reset the device before the next frame (chosen)
   - Pros: removes the crash whatever left the back buffer missing. The game already handles a `false` result from this function (it is what the fallback returns after logging). The device reset is the same one the game does when the device reports `D3DERR_DEVICENOTRESET`.
   - Cons / risks: the frames until the reset succeeds are not drawn. A reset that keeps failing keeps the screen frozen instead of crashing.
2. Only check the back buffer at the start of each frame and skip frames without it
   - Pros: no hook in the per-draw render target code.
   - Cons / risks: misses a back buffer lost in the middle of a frame (for example by a display change handled during the frame), which still recurses.
3. Find and fix why the back buffer stays missing
   - Pros: would remove the cause.
   - Cons / risks: the cause is not known yet and needs logs from the game; a different cause would still crash.

## Chosen fix

- `0x88f300` is hooked. The hook remembers the group whose call is running; a nested call for the same group (only the fallback makes one) returns `false` without running, and a device reset is requested. A fallback from another group to the back buffer still runs once, as before. Each call adds one comparison.
- `0x891f80` (called first by BeginFrame, `0x893d03`) is hooked. Without a requested reset it only tests one flag. With one, it runs the game's check; if the game did not reset the device itself, it calls the game's reset (`0x890e20`). If the reset fails, the frame is skipped and the reset is tried again at the next frame.
- The Mod checks the bytes of both functions, the fallback, the vtable entry (`0x947808`) and the calls at `0x891f9b` and `0x893d03` at startup. If they differ, this fix is skipped and the log says so.
- `mod_loader.log` gets one line when the back buffer stops working (`render target recovery: the back buffer could not be set as the render target; ...`), one line for the first failed reset, and one line when the device is reset (`... the device was reset by this fix after N failed reset(s); drawing continues`). These lines also tell which step failed in the game.
- Verified with `render_target_recovery_native_probe`, which runs the game's own `0x88f300` and `0x891f80` on a copy of the exe with a synthetic renderer whose back buffer cannot be set and a fake device. In the game, switching to another window and back no longer crashes.
- As a side effect, leaving and returning to the window is faster, and the game no longer freezes or crashes at those moments (seen in the game).

## Notes

- `post_fx` hooks the reset (`0x890e20`) to release its own resources. The reset called by this fix goes through that hook too.
