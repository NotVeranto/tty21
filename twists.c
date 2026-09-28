#include "twists.h"

#include <stdlib.h>

static const TwistInfo TWIST_INFO[] = {
    {TWIST_NONE, "None", "No floor modifier.", TWIST_CAT_NEUTRAL},
    {TWIST_LUCKY_DRAW,
     "Lucky Draw",
     "Your first card is always 8 or higher.",
     TWIST_CAT_FAVORABLE},
    {TWIST_CHARITY,
     "Charity",
     "Losses cost only half your bet.",
     TWIST_CAT_FAVORABLE},
    {TWIST_SNIPER,
     "Sniper",
     "Dealer's hole card is visible.",
     TWIST_CAT_FAVORABLE},
    {TWIST_SIX_DECK,
     "Six Deck",
     "The deck reshuffles after every hand.",
     TWIST_CAT_NEUTRAL},
    {TWIST_NO_RESHUFFLE,
     "No Reshuffle",
     "The deck reshuffles only when empty.",
     TWIST_CAT_NEUTRAL},
    {TWIST_COLD_DECK,
     "Cold Deck",
     "Your first card is always 5 or lower.",
     TWIST_CAT_HOSTILE},
    {TWIST_HALF_PAYOUT,
     "Half Payout",
     "Regular wins pay only 1:2.",
     TWIST_CAT_HOSTILE},
    {TWIST_STICKY_HANDS,
     "Sticky Hands",
     "You cannot stand below 15.",
     TWIST_CAT_HOSTILE}
};

static const TwistId FAVORABLE_TWISTS[] = {
    TWIST_LUCKY_DRAW,
    TWIST_CHARITY,
    TWIST_SNIPER
};

static const TwistId NEUTRAL_TWISTS[] = {
    TWIST_SIX_DECK,
    TWIST_NO_RESHUFFLE
};

static const TwistId HOSTILE_TWISTS[] = {
    TWIST_COLD_DECK,
    TWIST_HALF_PAYOUT,
    TWIST_STICKY_HANDS
};

// Returns a twist from one category with uniform selection.
static TwistId selectTwistFromCategory(TwistCategory category) {
    const TwistId *twists;
    int twistCount;

    if (category == TWIST_CAT_FAVORABLE) {
        twists = FAVORABLE_TWISTS;
        twistCount = (int)(sizeof(FAVORABLE_TWISTS) /
                           sizeof(FAVORABLE_TWISTS[0]));
    } else if (category == TWIST_CAT_NEUTRAL) {
        twists = NEUTRAL_TWISTS;
        twistCount = (int)(sizeof(NEUTRAL_TWISTS) /
                           sizeof(NEUTRAL_TWISTS[0]));
    } else {
        twists = HOSTILE_TWISTS;
        twistCount = (int)(sizeof(HOSTILE_TWISTS) /
                           sizeof(HOSTILE_TWISTS[0]));
    }

    return twists[rand() % twistCount];
}

const TwistInfo *getTwistInfo(TwistId id) {
    if (id < TWIST_NONE || id >= TWIST_COUNT) {
        id = TWIST_NONE;
    }

    return &TWIST_INFO[id];
}

TwistId selectTwistForFloor(int floor) {
    int roll;
    TwistCategory category;

    if (floor <= 3) {
        return TWIST_NONE;
    }

    roll = rand() % 100;

    if (floor <= 7) {
        if (roll < 50) {
            category = TWIST_CAT_FAVORABLE;
        } else if (roll < 80) {
            category = TWIST_CAT_NEUTRAL;
        } else {
            category = TWIST_CAT_HOSTILE;
        }
    } else if (roll < 30) {
        category = TWIST_CAT_FAVORABLE;
    } else if (roll < 50) {
        category = TWIST_CAT_NEUTRAL;
    } else {
        category = TWIST_CAT_HOSTILE;
    }

    return selectTwistFromCategory(category);
}
