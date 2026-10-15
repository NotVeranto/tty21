#ifndef PERKS_H
#define PERKS_H

typedef enum {
    PERK_SECOND_WIND = 0,
    PERK_PEEK,
    PERK_SURVIVAL,
    PERK_ACE_HIGH,
    PERK_SIXTH_SENSE,
    PERK_DEALERS_FAVOR,
    PERK_SPLIT_MASTERY,
    PERK_DOUBLE_DIP,
    PERK_COMPOUNDING,
    PERK_INSURANCE_PERK,
    PERK_SLEIGHT,
    PERK_MULLIGAN,
    PERK_MARKED_CARD,
    PERK_FREEZE,
    PERK_BANKROLL,
    PERK_EXTRA_LIFE,
    PERK_COUNT
} PerkId;

typedef struct {
    PerkId id;
    const char *name;
    const char *description;
    int cost;
} PerkInfo;

int perkIsSold(PerkId id);
const PerkInfo *getPerkInfo(PerkId id);
int perkCost(PerkId id);
const char *perkName(PerkId id);
const char *perkDescription(PerkId id);

#endif
