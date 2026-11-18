#include "twists.h"
#include "rng.h"
#include "game.h"

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
     TWIST_CAT_HOSTILE},
    {TWIST_JACKPOT,
     "Jackpot",
     "5% chance a regular win pays 10:1.",
     TWIST_CAT_FAVORABLE},
    {TWIST_DESPERATION,
     "Desperation",
     "Doubles pay 3:1. A busted double loses the full bet.",
     TWIST_CAT_HOSTILE},
    {TWIST_BUST_INSURANCE,
     "Bust Insurance",
     "Busting loses only half your bet.",
     TWIST_CAT_FAVORABLE},
    {TWIST_ACES_LOW,
     "Aces Low",
     "All aces count as 1, for player and dealer.",
     TWIST_CAT_NEUTRAL},
    {TWIST_DEALER_TO_18,
     "Dealer to 18",
     "Dealer draws to 18.",
     TWIST_CAT_HOSTILE},
    {TWIST_BLOOD_PACT,
     "Blood Pact",
     "Wins pay 2:1. Losses cost 2x.",
     TWIST_CAT_HOSTILE},
    {TWIST_GREED,
     "Greed",
     "Each win grants +1 mark. Minimum bet is doubled.",
     TWIST_CAT_FAVORABLE},
    {TWIST_HIGH_STAKES,
     "High Stakes",
     "All payouts and losses are doubled.",
     TWIST_CAT_NEUTRAL}
};

static const TwistId FAVORABLE_TWISTS[] = {
    TWIST_LUCKY_DRAW,
    TWIST_CHARITY,
    TWIST_SNIPER,
    TWIST_JACKPOT,
    TWIST_BUST_INSURANCE,
    TWIST_GREED
};

static const TwistId NEUTRAL_TWISTS[] = {
    TWIST_SIX_DECK,
    TWIST_NO_RESHUFFLE,
    TWIST_ACES_LOW,
    TWIST_HIGH_STAKES
};

static const TwistId HOSTILE_TWISTS[] = {
    TWIST_COLD_DECK,
    TWIST_HALF_PAYOUT,
    TWIST_STICKY_HANDS,
    TWIST_DESPERATION,
    TWIST_DEALER_TO_18,
    TWIST_BLOOD_PACT
};

// Returns a twist from one category with uniform selection.
static TwistId selectTwistFromCategory(TwistCategory category,
                                         TwistId previousTwist) {
    const TwistId *twists;
    int twistCount;
    TwistId candidates[16];
    int candidateCount;
    int index;

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

    candidateCount = 0;
    for (index = 0; index < twistCount; index++) {
        if (twists[index] != previousTwist)
            candidates[candidateCount++] = twists[index];
    }
    if (candidateCount == 0)
        return twists[rngRange(0, twistCount - 1)];
    return candidates[rngRange(0, candidateCount - 1)];
}

const TwistInfo *getTwistInfo(TwistId id) {
    if (id < TWIST_NONE || id >= TWIST_COUNT) {
        id = TWIST_NONE;
    }

    return &TWIST_INFO[id];
}

int twistIsForcedHostile(const Game *game) {
    return game->run.floor >= 10 &&
           (game->run.floor - 10) % 3 == 0 &&
           !dealerIsBoss(game->run.dealerId);
}

TwistId selectTwistForFloor(const Game *game) {
    int floor = game->run.floor;
    TwistId previousTwist = game->run.previousTwistId;
    int roll;
    TwistCategory category;
    int sixthSense = game->player.perkOwned[PERK_SIXTH_SENSE];

    if (floor <= 3) return TWIST_NONE;
    if (twistIsForcedHostile(game)) {
        return selectTwistFromCategory(TWIST_CAT_HOSTILE, previousTwist);
    }

    roll = rngRange(0, 99);
    if (sixthSense) {
        if (floor <= 7) {
            category = roll < 50 ? TWIST_CAT_FAVORABLE
                                 : TWIST_CAT_NEUTRAL;
        } else {
            category = roll < 60 ? TWIST_CAT_FAVORABLE
                                 : TWIST_CAT_NEUTRAL;
        }
    } else if (floor <= 7) {
        if (roll < 50) category = TWIST_CAT_FAVORABLE;
        else if (roll < 80) category = TWIST_CAT_NEUTRAL;
        else category = TWIST_CAT_HOSTILE;
    } else if (roll < 30) {
        category = TWIST_CAT_FAVORABLE;
    } else if (roll < 50) {
        category = TWIST_CAT_NEUTRAL;
    } else {
        category = TWIST_CAT_HOSTILE;
    }
    return selectTwistFromCategory(category, previousTwist);
}
