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
     "Random twists on floors 4+ are favorable or neutral; forced hostile floors remain hostile.",
     80},
    {PERK_DEALERS_FAVOR, "Dealer's Favor",
     "Once per floor, a bust is treated as a push.", 70},
    {PERK_SPLIT_MASTERY, "Split Mastery",
     "Split hands may be re-split once more.", 60},
    {PERK_DOUBLE_DIP, "Double Dip",
     "A doubled win pays an extra half bet.", 55},
    {PERK_COMPOUNDING, "Compounding",
     "Every consecutive win adds +1 to the next payout.", 75},
    {PERK_INSURANCE_PERK, "Insurance",
     "Once per floor, a losing hand loses half the bet.", 50},
    {PERK_MARKED_CARD, "Marked Card",
     "The top three deck cards are visible each round.", 45},
    {PERK_BANKROLL, "Bankroll",
     "Start each floor with +5 chips.", 40},
    {PERK_EXTRA_LIFE, "Extra Life",
     "Once per run, a loss that would end the run is forgiven.", 100}
};

// All perks in the current catalogue are sold. The function is retained so future unsold perks can be gated here without changing callers.
int perkIsSold(PerkId id) {
    (void)id;
    return 1;
}

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
