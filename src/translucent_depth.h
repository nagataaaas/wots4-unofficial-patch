#pragma once
// The scene is drawn to two render targets at once (0x541930): color, and a
// depth texture (render target 1) that depth of field reads. Every pixel shader
// of ColorDepthMaterial writes depth to COLOR1 with alpha 1, so a translucent
// effect quad overwrites (or, with additive blending, adds to) the depth of its
// whole rectangle, transparent texels included. Depth of field then blurs and
// changes the contrast of that rectangle: a white or black box behind the effect.
//
// Translucent draws do not write the Z buffer either, so keep them out of the
// depth texture: while alpha blending is on, render target 1 gets no channels.
#include <cstdint>

namespace bug_fix {

constexpr uint32_t allColorChannels = 0xf;  // D3DCOLORWRITEENABLE_RED | GREEN | BLUE | ALPHA

// Follows the two render states the game sets and gives the write mask of render target 1.
struct TranslucentDepthMask {
    bool blending = false;                    // D3DRS_ALPHABLENDENABLE (default FALSE)
    uint32_t requested = allColorChannels;    // D3DRS_COLORWRITEENABLE1 as the game asked (default 0xf)

    uint32_t effective() const { return blending ? 0 : requested; }
};

}  // namespace bug_fix
