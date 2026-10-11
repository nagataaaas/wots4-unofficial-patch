#pragma once
// The result of the last night visit (yobai) stays in the night visit progress
// ([0x9ecbb8]) after it was handled: 0x6a0560 hands out the reward, removes the
// promise and only marks the result handled (+0xa = 1). Saving the game clears
// the result (0x53e43a), but ending the play session (0x4e18e0) only clears the
// mark. Loading a save file that still has a promise then hands out the old
// result again at the next map change. Retry after death also ends the session,
// but restores the snapshot taken at the last map exit, which still has the
// promise of that visit and not its reward, so the result must be kept there.
#include <cstdint>

namespace bug_fix {

// Night visit progress ([0x9ecbb8], 0x44 bytes)
constexpr uint32_t yobaiResultOffset = 0x9;    // int8: -1 none, 0 success, 1 consolation, 2 Laura, 3 failure
constexpr uint32_t yobaiHandledOffset = 0xa;   // uint8: 1 after 0x6a0560 handed the result out
constexpr int8_t noYobaiResult = -1;

// Sets the progress to "no result", as saving does (0x53e43a). Returns the result that was
// discarded, or noYobaiResult when there was none (or no progress object).
inline int8_t discardYobaiResult(uint8_t* progress) {
    if(!progress)
        return noYobaiResult;
    auto result=static_cast<int8_t>(progress[yobaiResultOffset]);
    progress[yobaiResultOffset]=static_cast<uint8_t>(noYobaiResult);
    progress[yobaiHandledOffset]=0;
    return result;
}

}  // namespace bug_fix
