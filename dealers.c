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
    {DEALER_TWIN, "Twin", "Dealer plays two hands.", NULL},
    {DEALER_BOSS_PIT, "The Pit Boss",
     "All losses cost 150% of your bet.",
     "Dealer total is always hidden."},
    {DEALER_BOSS_PRIME, "Croupier Prime",
     "Every player hit makes the dealer draw too.",
     "Dealer draws to 18."},
    {DEALER_BOSS_SUPREME, "Accountant Supreme",
     "All dealer aces count as 1.",
     "Your first double down is forced."},
    {DEALER_BOSS_VOID, "Void Saint",
     "Your natural blackjack pays 1:1.",
     "Pure wins pay 2:1."}
};

const DealerInfo *getDealerInfo(DealerId id) {
    if (id < 0 || id >= DEALER_COUNT) {
        id = DEALER_CROUPIER;
    }

    return &DEALER_INFO[id];
}

int dealerIsBoss(DealerId id) {
    return id == DEALER_BOSS_PIT ||
           id == DEALER_BOSS_PRIME ||
           id == DEALER_BOSS_SUPREME ||
           id == DEALER_BOSS_VOID;
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

    if (floor >= 5 && floor % 5 == 0) {
        static const DealerId BOSSES[] = {
            DEALER_BOSS_PIT,
            DEALER_BOSS_PRIME,
            DEALER_BOSS_SUPREME,
            DEALER_BOSS_VOID
        };
        int bossIndex = (floor / 5 - 1) % 4;

        return BOSSES[bossIndex];
    }

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
    return id == DEALER_CLOCKMAKER || id == DEALER_BOSS_PRIME ? 18 : 17;
}

const char *dealerPowerRule2(DealerId id, int floor) {
    if (floor < 10) return NULL;
    switch (id) {
        case DEALER_CROUPIER:   return NULL;
        case DEALER_ACCOUNTANT: return "Losses cost 125%.";
        case DEALER_CLOCKMAKER: return "Dealer draws to 19.";
        case DEALER_BUTCHER:    return "You cannot stand below 15.";
        case DEALER_MIRROR:     return "Every loss costs 2x.";
        case DEALER_REAPER:     return "Your blackjack pays 1:1.";
        case DEALER_VOID:       return "You cannot double.";
        case DEALER_ORACLE:     return "Dealer draws to 18.";
        case DEALER_DEVIL:      return "Every hit costs 3 chips.";
        case DEALER_SAINT:      return "Losses cost 150%.";
        case DEALER_TWIN:       return "You cannot split.";
        default:                return NULL;
    }
}

int dealerPowerLossMultiplier(DealerId id, int floor) {
    if (floor < 10) return 100;
    if (id == DEALER_ACCOUNTANT) return 125;
    if (id == DEALER_SAINT) return 150;
    if (id == DEALER_MIRROR) return 200;
    return 100;
}

/* Returns the dealer's loss multiplier in percent. */
int dealerLossMultiplierPercent(DealerId id) {
    return id == DEALER_BUTCHER || id == DEALER_BOSS_PIT ? 150 : 100;
}

/* Returns the numerator for a natural blackjack payout. */
int dealerBlackjackNumerator(DealerId id) {
    return id == DEALER_VOID || id == DEALER_BOSS_VOID ? 1 : 3;
}

/* Returns the denominator for a natural blackjack payout. */
int dealerBlackjackDenominator(DealerId id) {
    return id == DEALER_VOID || id == DEALER_BOSS_VOID ? 1 : 2;
}

/* Returns whether the dealer total is always hidden. */
int dealerHidesTotal(DealerId id) {
    return id == DEALER_REAPER || id == DEALER_BOSS_PIT;
}

/* Returns whether the dealer draws when the player hits. */
int dealerMirrorsOnHit(DealerId id) {
    return id == DEALER_MIRROR || id == DEALER_BOSS_PRIME;
}

/* Returns whether the next deck card is shown during the player turn. */
int dealerShowsNextCard(DealerId id) {
    return id == DEALER_ORACLE;
}

/* Returns whether dealer aces always count as one. */
int dealerAcesCountAsOne(DealerId id) {
    return id == DEALER_ACCOUNTANT || id == DEALER_BOSS_SUPREME;
}

/* Returns whether the dealer forces the opening double down. */
int dealerForcesDouble(DealerId id) {
    return id == DEALER_DEVIL || id == DEALER_BOSS_SUPREME;
}

/* Returns whether the dealer pays the pure-hand bonus. */
int dealerPaysPureHandBonus(DealerId id) {
    return id == DEALER_SAINT || id == DEALER_BOSS_VOID;
}

/* Returns the number of active dealer hands. */
int dealerHandsCount(DealerId id) {
    return id == DEALER_TWIN ? 2 : 1;
}
