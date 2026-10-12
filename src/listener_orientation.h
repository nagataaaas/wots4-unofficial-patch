#pragma once
// The game sets DirectSound's 3D listener (0x6a2660) from the camera: the look
// axis (column 0 of the world rotation, as Gamebryo cameras look along their
// local +X; read at 0x59fe89) and the up axis (column 1, 0x59feb6). It negates
// the look axis before IDirectSound3DListener::SetOrientation (0x6a281e,
// 0x6a2837, 0x6a2848), so the listener faces away from the scene and the sounds
// in view are behind it. With a stereo mix, front and back pan the same, so this
// does not show. With a 7.1 mix, which DirectSound can use even when the
// speakers are stereo, those sounds go to the back and side speakers and are
// nearly silent unless a virtual surround such as Windows Sonic folds them in.
//
// Passing both axes negated makes DirectSound face the look axis. DirectSound
// takes its right axis from the cross product of up and front, which the two
// negations leave as it was, so left and right do not change. Its up axis comes
// out upside down, which the speaker panning does not use (it pans on the
// horizontal plane) and which distance and Doppler do not depend on.

namespace unofficial_patch {

// The vectors the game passes to 0x6a2660, in place of its own front (look) and up axes.
// Runs every frame, so it only negates.
inline void correctListenerAxes(const float* look, const float* up, float* passedFront, float* passedUp) {
    for(int axis=0;axis<3;++axis) {
        passedFront[axis]=-look[axis];
        passedUp[axis]=-up[axis];
    }
}

}  // namespace unofficial_patch
