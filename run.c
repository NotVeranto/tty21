#include "run.h"

// Initializes the persistent state for a new run.
void initRun(Run *run) {
    run->floor = 1;
    run->souls = 0;
    run->dealerId = DEALER_CROUPIER;
    run->previousDealerId = DEALER_CROUPIER;
    run->twistId = TWIST_NONE;
    run->previousTwistId = TWIST_NONE;
}

// Returns the minimum bet for a floor.
int getMinBet(int floor) {
    if (floor <= 1) {
        return 1;
    }

    if (floor <= 3) {
        return 5;
    }

    if (floor <= 6) {
        return 10;
    }

    if (floor <= 9) {
        return 25;
    }

    return 50;
}
