// prepareStoppedSlotRelease on synthetic sound slots and channels. No game files.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "sound_slots.h"

static unsigned tests, failures;
static void check(const char* label, bool passed) {
    ++tests; if(!passed) ++failures;
    printf("%s %s\n",passed?"PASS":"FAIL",label);
}

struct Channel { unsigned char bytes[0x20]; };
struct Slot { unsigned char bytes[0x74]; };

static void put(Slot& slot, uint32_t offset, uintptr_t value) { memcpy(slot.bytes+offset,&value,sizeof(value)); }

// A slot in use with a channel and a Sound, as 0x5a0f30 leaves it.
static void prepare(Slot& slot, Channel& channel, bool paused, bool playedLastUpdate) {
    static unsigned char sound[0x60];
    memset(&slot,0,sizeof(slot));
    memset(&channel,0,sizeof(channel));
    slot.bytes[unofficial_patch::slotInUseOffset]=1;
    put(slot,unofficial_patch::slotChannelOffset,(uintptr_t)&channel);
    put(slot,unofficial_patch::slotSoundOffset,(uintptr_t)sound);
    channel.bytes[unofficial_patch::channelPausedOffset]=paused;
    channel.bytes[unofficial_patch::channelPlayingOffset]=playedLastUpdate;
}

static bool notifiedNext(const Channel& channel) {
    return channel.bytes[unofficial_patch::channelPausedOffset]==0&&
        channel.bytes[unofficial_patch::channelPlayingOffset]==1;
}

int main() {
    using unofficial_patch::prepareStoppedSlotRelease;
    Slot slot; Channel channel;
    auto address=[&]{ return (uintptr_t)&slot; };

    prepare(slot,channel,true,false);
    check("an immediate stop of a paused channel is freed by the next update",
        prepareStoppedSlotRelease(address(),0.0f)&&notifiedNext(channel));

    prepare(slot,channel,false,false);
    check("an immediate stop before the first update saw it playing is freed by the next update",
        prepareStoppedSlotRelease(address(),0.0f)&&notifiedNext(channel));

    prepare(slot,channel,false,true);
    check("an immediate stop of a playing channel keeps the notification the game sends anyway",
        prepareStoppedSlotRelease(address(),0.0f)&&notifiedNext(channel));

    prepare(slot,channel,false,false);
    check("a negative fade is an immediate stop, as in 0x5a1243",
        prepareStoppedSlotRelease(address(),-1.0f)&&notifiedNext(channel));

    prepare(slot,channel,false,false);
    Channel before=channel;
    check("a fade out of a channel that is not paused is left to the update",
        !prepareStoppedSlotRelease(address(),0.5f)&&!memcmp(&before,&channel,sizeof(channel)));

    prepare(slot,channel,false,false);
    check("NaN fades, as in 0x5a1243, so it is left to the update",
        !prepareStoppedSlotRelease(address(),std::nanf(""))&&channel.bytes[unofficial_patch::channelPlayingOffset]==0);

    prepare(slot,channel,true,false);
    check("a fade out of a paused (silent) channel is freed like an immediate stop",
        prepareStoppedSlotRelease(address(),0.5f)&&notifiedNext(channel));

    prepare(slot,channel,true,false);
    put(slot,unofficial_patch::slotChannelOffset,0);
    check("without a channel nothing is changed (0x5a1210 marks it for release itself)",
        !prepareStoppedSlotRelease(address(),0.0f)&&channel.bytes[unofficial_patch::channelPausedOffset]==1);

    prepare(slot,channel,true,false);
    put(slot,unofficial_patch::slotSoundOffset,0);
    check("without a Sound nothing is changed",
        !prepareStoppedSlotRelease(address(),0.0f)&&channel.bytes[unofficial_patch::channelPausedOffset]==1);

    prepare(slot,channel,true,false);
    slot.bytes[unofficial_patch::slotInUseOffset]=0;
    check("a slot that is not in use is not changed",
        !prepareStoppedSlotRelease(address(),0.0f)&&channel.bytes[unofficial_patch::channelPausedOffset]==1);

    check("no slot (a handle that was not found) is ignored",!prepareStoppedSlotRelease(0,0.0f));

    prepare(slot,channel,true,false);
    Slot slotBefore=slot;
    prepareStoppedSlotRelease(address(),0.0f);
    check("the slot itself is not written",!memcmp(&slotBefore,&slot,sizeof(slot)));

    printf("RESULT tests=%u failures=%u\n",tests,failures);
    return failures?1:0;
}
