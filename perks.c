#include "perks.h"

static const PerkInfo PERK_INFO[] = {
    {PERK_SECOND_WIND,
     "Second Wind",
     "Bust resets your hand to 11 once per run.",
     30},
    {PERK_PEEK,
     "Peek",
     "Press P to reveal the dealer's hole card this round.",
     40},
    {PERK_SURVIVAL,
     "Survival",
     "A loss that would end the run leaves you with 50% of chips.",
     50},
    {PERK_ACE_HIGH,
     "Ace High",
     "Your aces always count as 11, never 1.",
     60},
    {PERK_SIXTH_SENSE,
     "Sixth Sense",
     "Twists on floors 4+ are always favorable or neutral.",
     80}
};

const PerkInfo *getPerkInfo(PerkId id) {
    if (id < PERK_SECOND_WIND || id >= PERK_COUNT) {
        return 0;
    }

    return &PERK_INFO[id];
}

int perkCost(PerkId id) {
    const PerkInfo *info;

    info = getPerkInfo(id);

    if (info == 0) {
        return 0;
    }

    return info->cost;
}

const char *perkName(PerkId id) {
    const PerkInfo *info;

    info = getPerkInfo(id);

    if (info == 0) {
        return "";
    }

    return info->name;
}

const char *perkDescription(PerkId id) {
    const PerkInfo *info;

    info = getPerkInfo(id);

    if (info == 0) {
        return "";
    }

    return info->description;
}
