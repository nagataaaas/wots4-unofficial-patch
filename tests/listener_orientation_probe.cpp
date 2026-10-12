// correctListenerAxes with synthetic camera axes and sound positions. Models what
// DirectSound gets after the game negates the front axis (0x6a2660) and where a
// sound lands relative to the listener. No game files, no audio device.
#include <cmath>
#include <cstdio>
#include "listener_orientation.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

struct Vector { float x, y, z; };

static Vector cross(Vector a, Vector b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
static float dot(Vector a, Vector b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
static Vector negated(Vector v) { return {-v.x,-v.y,-v.z}; }

// A sound's position in the listener's axes (right, up, front) as DirectSound pans it.
// DirectSound's right axis is cross(up, front): front (0,0,1) and up (0,1,0) give right (1,0,0).
struct Relative { float right, up, front; };
static Relative relativeTo(Vector listenerFront, Vector listenerUp, Vector offset) {
    return {dot(offset,cross(listenerUp,listenerFront)),dot(offset,listenerUp),dot(offset,listenerFront)};
}

// What DirectSound gets from 0x6a2660 for the front and up the game passes in: front negated.
static Relative asGameSetsIt(Vector passedFront, Vector passedUp, Vector offset) {
    return relativeTo(negated(passedFront),passedUp,offset);
}

static Relative withoutMod(Vector look, Vector up, Vector offset) { return asGameSetsIt(look,up,offset); }

static Relative withMod(Vector look, Vector up, Vector offset) {
    Vector front{}, passedUp{};
    unofficial_patch::correctListenerAxes(&look.x,&up.x,&front.x,&passedUp.x);
    return asGameSetsIt(front,passedUp,offset);
}

static bool near(float a, float b) { return std::fabs(a-b)<1e-4f; }

// A camera turned by yaw around the world up axis (+Y) and pitched down by pitch, in radians.
struct Camera { Vector look, up; };
static Camera camera(float yaw, float pitch) {
    Vector look{std::cos(pitch)*std::sin(yaw),-std::sin(pitch),std::cos(pitch)*std::cos(yaw)};
    Vector up{std::sin(pitch)*std::sin(yaw),std::cos(pitch),std::sin(pitch)*std::cos(yaw)};
    return {look,up};
}

int main() {
    // A third-person camera looking along +Z, slightly down, as in the field.
    auto view=camera(0.0f,0.25f);
    Vector ahead{view.look.x*10,view.look.y*10,view.look.z*10};
    Vector behind=negated(ahead);

    check("without the Mod a sound in view is behind the listener (the bug)",withoutMod(view.look,view.up,ahead).front<0);
    check("without the Mod a sound behind the camera is in front of the listener",
        withoutMod(view.look,view.up,behind).front>0);
    check("with the Mod a sound in view is in front of the listener",withMod(view.look,view.up,ahead).front>0);
    check("with the Mod a sound behind the camera is behind the listener",withMod(view.look,view.up,behind).front<0);

    // Left and right as the game had them, for every direction of the camera and the sound.
    bool sameSide=true, frontFlipped=true, sameDistance=true;
    for(int yawStep=0;yawStep<12;++yawStep) {
        for(int pitchStep=-2;pitchStep<=2;++pitchStep) {
            auto turned=camera(yawStep*0.5236f,pitchStep*0.3f);
            for(int soundStep=0;soundStep<16;++soundStep) {
                float angle=soundStep*0.3927f;
                Vector offset{7*std::sin(angle),1.5f-soundStep*0.2f,7*std::cos(angle)};
                auto before=withoutMod(turned.look,turned.up,offset);
                auto after=withMod(turned.look,turned.up,offset);
                sameSide&=near(before.right,after.right);
                frontFlipped&=near(before.front,-after.front);
                sameDistance&=near(before.right*before.right+before.up*before.up+before.front*before.front,
                    after.right*after.right+after.up*after.up+after.front*after.front);
            }
        }
    }
    check("left and right stay as the game had them (960 camera and sound directions)",sameSide);
    check("front and back are swapped back (960 directions)",frontFlipped);
    check("the distance to the listener does not change (960 directions)",sameDistance);

    // The game's default when there is no camera (0x59fcab): front (0,0,1), up (0,1,0).
    Vector defaultFront{0,0,1}, defaultUp{0,1,0}, front{}, up{};
    unofficial_patch::correctListenerAxes(&defaultFront.x,&defaultUp.x,&front.x,&up.x);
    check("the default axes are negated, so DirectSound gets front (0,0,1) and up (0,-1,0)",
        front.x==0&&front.y==0&&front.z==-1&&up.x==0&&up.y==-1&&up.z==0);
    check("the inputs are not changed",defaultFront.z==1&&defaultUp.y==1);

    printf("RESULT tests=%u failures=%u\n",tests,failures);
    return failures?1:0;
}
