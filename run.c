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
}

// Returns the minimum bet for a floor.
int getMinBet(const Game *game, int floor, GameMode mode) {
    int base;

    if (mode == MODE_CLASSIC) {
        base = 1;
    } else if (floor <= 1) {
        base = 1;
    } else if (floor <= 3) {
        base = 5;
    } else if (floor <= 6) {
        base = 10;
    } else if (floor <= 9) {
        base = 25;
    } else {
        base = 50;
    }

    if (twistActive(game, TWIST_GREED)) {
        base *= 2;
    }

    return base;
}
