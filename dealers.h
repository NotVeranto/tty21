#ifndef DEALERS_H
#define DEALERS_H

typedef enum {
    DEALER_CROUPIER = 0,
    DEALER_ACCOUNTANT,
    DEALER_CLOCKMAKER,
    DEALER_BUTCHER,
    DEALER_MIRROR,
    DEALER_REAPER,
    DEALER_VOID,
    DEALER_ORACLE,
    DEALER_DEVIL,
    DEALER_SAINT,
    DEALER_TWIN,
    DEALER_BOSS_PIT,
    DEALER_BOSS_PRIME,
    DEALER_BOSS_SUPREME,
    DEALER_BOSS_VOID,
    DEALER_COUNT
} DealerId;

typedef struct {
    DealerId id;
    const char *name;
    const char *rule1;
    const char *rule2;
} DealerInfo;

const DealerInfo *getDealerInfo(DealerId id);
int dealerIsBoss(DealerId id);
DealerId selectDealerForFloor(int floor, DealerId previousDealer);

int dealerTargetValue(DealerId id);
const char *dealerPowerRule2(DealerId id, int floor);
int dealerPowerLossMultiplier(DealerId id, int floor);
int dealerLossMultiplierPercent(DealerId id);
int dealerBlackjackNumerator(DealerId id);
int dealerBlackjackDenominator(DealerId id);
int dealerHidesTotal(DealerId id);
int dealerMirrorsOnHit(DealerId id);
int dealerShowsNextCard(DealerId id);
int dealerAcesCountAsOne(DealerId id);
int dealerForcesDouble(DealerId id);
int dealerPaysPureHandBonus(DealerId id);
int dealerHandsCount(DealerId id);

#endif
