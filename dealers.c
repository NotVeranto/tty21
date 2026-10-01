#include "dealers.h"
#include "rng.h"

#include <stddef.h>

static const DealerInfo DEALER_INFO[] = {
    {DEALER_CROUPIER, "Croupier", NULL, NULL},
    {DEALER_ACCOUNTANT, "Accountant", "All dealer aces count as 1.", NULL},
    {DEALER_CLOCKMAKER, "Clockmaker", "Dealer draws to 18.", NULL},
    {DEALER_BUTCHER, "Butcher", "All losses cost 150% of your bet.", NULL},
    {DEALER_MIRROR, "Mirror", "Every player hit makes the dealer draw too.", NULL},
    {DEALER_REAPER, "Reaper", "Dealer total is always hidden.", NULL},
    {DEALER_VOID, "Void", "Your natural blackjack pays 1:1.", NULL},
    {DEALER_ORACLE, "Oracle", "You see the next deck card before acting.", NULL},
    {DEALER_DEVIL, "Devil", "Your first double down is forced.", NULL},
    {DEALER_SAINT, "Saint", "Pure wins pay 2:1.", NULL},
    {DEALER_TWIN, "Twin", "Dealer plays two hands.", NULL}
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
        DEALER_ORACLE,
        DEALER_DEVIL,
        DEALER_SAINT,
        DEALER_TWIN
    };
    DealerId candidates[8];
    int candidateCount;
    int dealerIndex;
    int randomIndex;

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

    for (dealerIndex = 0; dealerIndex < 8; dealerIndex++) {
        if (RANDOM_DEALERS[dealerIndex] != previousDealer) {
            candidates[candidateCount] = RANDOM_DEALERS[dealerIndex];
            candidateCount++;
        }
    }

    if (candidateCount == 0) {
        return DEALER_CROUPIER;
    }

    randomIndex = rngRange(0, candidateCount - 1);
    return candidates[randomIndex];
}

/* Returns the dealer's standing target value. */
int dealerTargetValue(DealerId id) {
    return id == DEALER_CLOCKMAKER ? 18 : 17;
}

/* Returns the dealer's loss multiplier in percent. */
int dealerLossMultiplierPercent(DealerId id) {
    return id == DEALER_BUTCHER ? 150 : 100;
}

/* Returns the numerator for a natural blackjack payout. */
int dealerBlackjackNumerator(DealerId id) {
    return id == DEALER_VOID ? 1 : 3;
}

/* Returns the denominator for a natural blackjack payout. */
int dealerBlackjackDenominator(DealerId id) {
    return id == DEALER_VOID ? 1 : 2;
}

/* Returns whether the dealer total is always hidden. */
int dealerHidesTotal(DealerId id) {
    return id == DEALER_REAPER;
}

/* Returns whether the dealer draws when the player hits. */
int dealerMirrorsOnHit(DealerId id) {
    return id == DEALER_MIRROR;
}

/* Returns whether the next deck card is shown during the player turn. */
int dealerShowsNextCard(DealerId id) {
    return id == DEALER_ORACLE;
}

/* Returns whether dealer aces always count as one. */
int dealerAcesCountAsOne(DealerId id) {
    return id == DEALER_ACCOUNTANT;
}

/* Returns whether the dealer forces the opening double down. */
int dealerForcesDouble(DealerId id) {
    return id == DEALER_DEVIL;
}

/* Returns whether the dealer pays the pure-hand bonus. */
int dealerPaysPureHandBonus(DealerId id) {
    return id == DEALER_SAINT;
}

/* Returns the number of active dealer hands. */
int dealerHandsCount(DealerId id) {
    return id == DEALER_TWIN ? 2 : 1;
}
