// detachJointFollowers on synthetic effect manager, effects and model. No game files.
#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "follow_detach.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

struct Effect { uintptr_t table, node, follow; float life, unused, time; unsigned char type, rest[7]; };
static_assert(sizeof(Effect)==0x20,"base effect size (0x4a49c0 frees 0x20)");
static_assert(offsetof(Effect,follow)==unofficial_patch::effectFollowOffset,"follow offset");

struct Model { unsigned char head[unofficial_patch::modelJointsOffset]; uintptr_t joints[unofficial_patch::modelJointCount]; };
struct Manager { unsigned char head[unofficial_patch::effectSlotsOffset]; uintptr_t slots[unofficial_patch::effectSlotCount]; };

int main() {
    static unsigned char nodes[2][unofficial_patch::modelJointCount][0x100], objectNode[0x100];
    static Model model, other;
    for(uint32_t j=0;j<unofficial_patch::modelJointCount;++j) {
        model.joints[j]=(uintptr_t)nodes[0][j];
        other.joints[j]=(uintptr_t)nodes[1][j];
    }
    model.joints[3]=0;  // A joint the model does not have

    // Same shape as the crash dump of 2026-10-11: v_aura_b (14) on 6 joints of one model.
    static Effect aura[6], foreign, object, unattached, last;
    const uint32_t auraJoints[6]={1,20,21,7,6,5};
    static Manager manager;
    for(int i=0;i<6;++i) {
        aura[i].type=14; aura[i].follow=model.joints[auraJoints[i]];
        manager.slots[275+i]=(uintptr_t)&aura[i];
    }
    foreign.type=14; foreign.follow=other.joints[1]; manager.slots[0]=(uintptr_t)&foreign;
    object.type=14; object.follow=(uintptr_t)objectNode; manager.slots[10]=(uintptr_t)&object;
    unattached.type=14; manager.slots[11]=(uintptr_t)&unattached;
    last.type=0x84; last.follow=model.joints[0]; last.node=0x1234; last.time=4.07f;
    manager.slots[unofficial_patch::effectSlotCount-1]=(uintptr_t)&last;

    auto detached=unofficial_patch::detachJointFollowers((uintptr_t)&manager,(uintptr_t)&model);
    bool auraCleared=true;
    for(auto& e:aura) auraCleared=auraCleared&&e.follow==0;
    check("effects on the released model's joints are detached",auraCleared);
    check("the last slot (349) is scanned",last.follow==0);
    check("detach changes only +8",last.node==0x1234&&last.time==4.07f&&last.type==0x84);
    check("returns the number of detached effects",detached==7);
    check("an effect on another model's joint is kept",foreign.follow==other.joints[1]);
    check("an effect on a node that is not a joint is kept",object.follow==(uintptr_t)objectNode);
    check("an effect without a follow target is kept",unattached.follow==0);

    check("second pass detaches nothing",unofficial_patch::detachJointFollowers((uintptr_t)&manager,(uintptr_t)&model)==0);
    foreign.follow=other.joints[1];
    check("no manager: nothing changes",unofficial_patch::detachJointFollowers(0,(uintptr_t)&other)==0&&foreign.follow!=0);
    check("no model: nothing changes",unofficial_patch::detachJointFollowers((uintptr_t)&manager,0)==0&&foreign.follow!=0);
    // A missing joint (0) must not match an effect whose +8 is 0.
    check("a missing joint does not match an unattached effect",
        unofficial_patch::detachJointFollowers((uintptr_t)&manager,(uintptr_t)&model)==0);

    printf("RESULT tests=%u failures=%u; synthetic data only\n",tests,failures);
    return failures?1:0;
}
