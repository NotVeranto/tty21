#include "run.h"

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
    run->coldSnapUsed = 0;
    run->coldSnapActive = 0;
    run->perkLockedUsed = 0;
    run->perkLockedId = -1;
}

