#ifndef PERKS_H
#define PERKS_H

typedef enum {
    PERK_SECOND_WIND = 0,
    PERK_PEEK,
    PERK_SURVIVAL,
    PERK_ACE_HIGH,
    PERK_SIXTH_SENSE,
    PERK_COUNT
} PerkId;

typedef struct {
    PerkId id;
    const char *name;
    const char *description;
    int cost;
} PerkInfo;

const PerkInfo *getPerkInfo(PerkId id);
int perkCost(PerkId id);
const char *perkName(PerkId id);
const char *perkDescription(PerkId id);

#endif
