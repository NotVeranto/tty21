#include "run.h"
#include "round.h"

// Initializes the persistent state for a new run.
void initRun(Run *run) {
    run->floor = 1;
    run->marks = 0;
    run->dealerId = DEALER_CROUPIER;
    run->previousDealerId = DEALER_CROUPIER;
    run->twistId = TWIST_NONE;
    run->previousTwistId = TWIST_NONE;
    run->extraTwistCount = 0;
    run->roundsOnFloor = 0;
    run->twistBlocked = 0;
    run->twistRerolled = 0;
    run->bossRewarded = 0;
    run->shopPerkCount = 0;
    run->shopPerksRerolled = 0;
}

// Returns the minimum bet for a floor.
int getMinBet(const Game *game, int floor, GameMode mode) {
    int base;

    if (mode == MODE_CLASSIC) {
        base = 1;
    } else {
        int halfFloor = floor / 2;

        base = 2 * (1 + halfFloor * halfFloor);
    }

    if (twistActive(game, TWIST_GREED)) {
        base *= 2;
    }

    base *= game->minBetMultiplier;

    return base;
}
