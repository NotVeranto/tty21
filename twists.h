#ifndef TWISTS_H
#define TWISTS_H

typedef enum {
    TWIST_NONE = 0,
    TWIST_LUCKY_DRAW,
    TWIST_CHARITY,
    TWIST_SNIPER,
    TWIST_SIX_DECK,
    TWIST_NO_RESHUFFLE,
    TWIST_COLD_DECK,
    TWIST_HALF_PAYOUT,
    TWIST_STICKY_HANDS,
    TWIST_COUNT
} TwistId;

typedef enum {
    TWIST_CAT_FAVORABLE,
    TWIST_CAT_NEUTRAL,
    TWIST_CAT_HOSTILE
} TwistCategory;

typedef struct {
    TwistId id;
    const char *name;
    const char *description;
    TwistCategory category;
} TwistInfo;

const TwistInfo *getTwistInfo(TwistId id);
TwistId selectTwistForFloor(int floor, TwistId previousTwist);

#endif
