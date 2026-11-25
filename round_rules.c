#include "round_rules.h"

#include "rng.h"

#include <stdio.h>

static int bossRule1Disabled(const Game *game) {
    return game->bossRuleDisabled && dealerIsBoss(game->run.dealerId);
}

int effectiveDealerLossMultiplier(const Game *game) {
    if (bossRule1Disabled(game) && game->run.dealerId == DEALER_BOSS_PIT) return 100;
    return dealerLossMultiplierPercent(game->run.dealerId);
}

int effectiveDealerBlackjackNumerator(const Game *game) {
    if (bossRule1Disabled(game) && game->run.dealerId == DEALER_BOSS_VOID) return 3;
    return dealerBlackjackNumerator(game->run.dealerId);
}

int effectiveDealerBlackjackDenominator(const Game *game) {
    if (bossRule1Disabled(game) && game->run.dealerId == DEALER_BOSS_VOID) return 2;
    return dealerBlackjackDenominator(game->run.dealerId);
}

int effectiveDealerMirrorsOnHit(const Game *game) {
    if (bossRule1Disabled(game) && game->run.dealerId == DEALER_BOSS_PRIME) return 0;
    return dealerMirrorsOnHit(game->run.dealerId);
}

int effectiveDealerAcesCountAsOne(const Game *game, DealerId id) {
    return dealerAcesCountAsOne(id);
}

int effectiveDealerAcesCountAsEleven(const Game *game, DealerId id) {
    return id == DEALER_BOSS_SUPREME && !bossRule1Disabled(game);
}

int twistActive(const Game *game, TwistId id) {
    int i;

    if (game->run.twistBlocked &&
        id == game->run.twistId) {
        return 0;
    }

    if (game->run.twistId == id) {
        return 1;
    }

    for (i = 0; i < game->run.extraTwistCount; i++) {
        if (game->run.extraTwists[i] == id) {
            return 1;
        }
    }

    return 0;
}

static TwistId pickHostileTwistExcluding(const Game *game) {
    static const TwistId POOL[] = {
        TWIST_COLD_DECK,
        TWIST_HALF_PAYOUT,
        TWIST_STICKY_HANDS,
        TWIST_DESPERATION,
        TWIST_DEALER_TO_18,
        TWIST_BLOOD_PACT
    };
    TwistId candidates[6];
    int count;
    int i;

    count = 0;

    for (i = 0; i < 6; i++) {
        if (!twistActive(game, POOL[i])) {
            candidates[count] = POOL[i];
            count++;
        }
    }

    if (count == 0) {
        return TWIST_NONE;
    }

    return candidates[rngRange(0, count - 1)];
}

void applyAntiFarmPressure(Game *game) {
    int r;
    int threshold;

    r = game->run.roundsOnFloor;
    threshold = 0;

    if (r >= 16) {
        threshold = 3 + (r - 16) / 3;

        if (threshold > MAX_EXTRA_TWISTS) {
            threshold = MAX_EXTRA_TWISTS;
        }
    } else if (r >= 11) {
        threshold = 2;
    } else if (r >= 6) {
        threshold = 1;
    }

    while (game->run.extraTwistCount < threshold) {
        TwistId pick;
        char msg[96];

        pick = pickHostileTwistExcluding(game);

        if (pick == TWIST_NONE) {
            break;
        }

        game->run.extraTwists[game->run.extraTwistCount] = pick;
        game->run.extraTwistCount++;

        snprintf(msg,
                 sizeof(msg),
                 "Floor tension rises: %s activates.",
                 getTwistInfo(pick)->name);
        logPush(game, msg, MSG_LOSS);
    }
}

int fatiguePercent(int roundsOnFloor) {
    if (roundsOnFloor <= 5) {
        return 100;
    }

    if (roundsOnFloor <= 8) {
        return 80;
    }

    if (roundsOnFloor <= 11) {
        return 60;
    }

    if (roundsOnFloor <= 14) {
        return 50;
    }

    return 30;
}
