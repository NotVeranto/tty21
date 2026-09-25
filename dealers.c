#include "dealers.h"

#include <stdlib.h>

static const DealerInfo DEALER_INFO[] = {
    {DEALER_CROUPIER, "Croupier", NULL, NULL},
    {DEALER_ACCOUNTANT, "Accountant", "All dealer aces count as 1.", NULL},
    {DEALER_CLOCKMAKER, "Clockmaker", "Dealer draws to 18.", NULL},
    {DEALER_BUTCHER, "Butcher", "All losses cost 150% of your bet.", NULL},
    {DEALER_MIRROR, "Mirror", "Every player hit makes the dealer draw too.", NULL},
    {DEALER_REAPER, "Reaper", "Dealer total is always hidden.", NULL},
    {DEALER_VOID, "Void", "Your natural blackjack pays 1:1.", NULL},
    {DEALER_ORACLE, "Oracle", "You see the next deck card before acting.", NULL}
};

const DealerInfo *getDealerInfo(DealerId id) {
    if (id < 0 || id >= DEALER_COUNT) {
        id = DEALER_CROUPIER;
    }

    return &DEALER_INFO[id];
}

DealerId selectDealerForFloor(int floor, DealerId previousDealer) {
    static const DealerId RANDOM_DEALERS[] = {
        DEALER_BUTCHER,
        DEALER_MIRROR,
        DEALER_REAPER,
        DEALER_VOID,
        DEALER_ORACLE
    };
    DealerId candidates[5];
    int candidateCount;
    int dealerIndex;
    int randomIndex;
    int randomValue;
    int randomLimit;

    if (floor == 1) {
        return DEALER_CROUPIER;
    }

    if (floor == 2) {
        return DEALER_ACCOUNTANT;
    }

    if (floor == 3) {
        return DEALER_CLOCKMAKER;
    }

    candidateCount = 0;

    for (dealerIndex = 0; dealerIndex < 5; dealerIndex++) {
        if (RANDOM_DEALERS[dealerIndex] != previousDealer) {
            candidates[candidateCount] = RANDOM_DEALERS[dealerIndex];
            candidateCount++;
        }
    }

    if (candidateCount == 0) {
        return DEALER_CROUPIER;
    }

    randomLimit = RAND_MAX - RAND_MAX % candidateCount;

    do {
        randomValue = rand();
    } while (randomValue >= randomLimit);

    randomIndex = randomValue % candidateCount;
    return candidates[randomIndex];
}

/* TODO(Stage 9.5): DEALER_DEVIL  — double down variant */
/* TODO(Stage 9.5): DEALER_TWIN   — two dealer hands (needs multi-hand) */
/* TODO(Stage 9.5): DEALER_SAINT  — pure-hand bonus (needs split) */
