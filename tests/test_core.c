#include "game.h"
#include "hand.h"
#include "rng.h"
#include "round_settlement.h"
#include "round_economy.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void testInitialStateAndShopOffers(void) {
    Game game;
    char offers[7];
    int offerCount;

    rngInit(12345U, 1);
    initGame(&game, MODE_ROGUELIKE);

    assert(game.player.chips == 100);
    assert(game.state == STATE_BET);
    assert(game.run.floor == 1);

    offerCount = getShopServiceOffers(&game, offers, 7);
    assert(offerCount == 5);
    assert(offers[0] == 'v');
    assert(offers[1] == 'n');
    assert(offers[2] == 't');
    assert(offers[3] == 'z');
    assert(offers[4] == 'l');
    assert(!isShopServiceOffered(&game, 'r'));
    assert(!isShopServiceOffered(&game, 'b'));
    assert(!isShopServiceOffered(&game, '?'));
    assert(getShopServiceOffers(&game, NULL, 7) == 0);
    assert(getShopServiceOffers(&game, offers, 0) == 0);
}

static void testShopPurchaseTransactions(void) {
    Game game;
    int marksBefore;

    rngInit(12345U, 1);
    initGame(&game, MODE_ROGUELIKE);

    assert(!canPurchaseShopService(&game, 'v', 91));
    assert(!purchaseShopService(&game, 'v', 91));
    assert(game.player.chips == 100);
    assert(game.run.marks == 0);

    marksBefore = game.run.marks;
    assert(canPurchaseShopService(&game, 'v', 10));
    assert(purchaseShopService(&game, 'v', 10));
    assert(game.player.chips == 90);
    assert(game.run.marks == marksBefore + 2);
    assert(game.totalMarksEarned == 2);

    assert(canPurchaseShopService(&game, 't', 10));
    assert(purchaseShopService(&game, 't', 10));
    assert(game.player.chips == 80);
    assert(game.tippedThisRound == 1);

    rngInit(12345U, 1);
    initGame(&game, MODE_ROGUELIKE);
    assert(canPurchaseShopService(&game, 'n', 10));
    assert(purchaseShopService(&game, 'n', 10));
    assert(game.player.chips == 50);
    assert(game.safetyNetArmed == 1);
    assert(!isShopServiceOffered(&game, 'n'));
    assert(!canPurchaseShopService(&game, 'n', 10));
    assert(!purchaseShopService(&game, 'n', 10));
    assert(game.player.chips == 50);

    rngInit(12345U, 1);
    initGame(&game, MODE_ROGUELIKE);
    assert(canPurchaseShopService(&game, 'l', 10));
    assert(purchaseShopService(&game, 'l', 10));
    assert(game.player.chips == 150);
    assert(game.loanAmount == 75);
    assert(game.loanDueFloor == game.run.floor + 3);
    assert(!isShopServiceOffered(&game, 'l'));
    assert(!canPurchaseShopService(&game, 'l', 10));

    rngInit(12345U, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.run.marks = 10;
    assert(canPurchaseShopService(&game, 'z', 10));
    assert(purchaseShopService(&game, 'z', 10));
    assert(game.run.marks == 0);
    assert(game.run.shopPerksRerolled == 1);
    assert(!canPurchaseShopService(&game, 'z', 10));
}

static void testBetLimits(void) {
    Game game;
    int minBet;

    rngInit(11223U, 1);
    initGame(&game, MODE_ROGUELIKE);

    game.player.chips = 100;
    game.run.floor = 8;
    minBet = getMinBet(&game, game.run.floor, game.mode);
    assert(minBet == 34);
    assert(getMaxBet(&game, minBet) == 50);

    game.run.floor = 12;
    minBet = getMinBet(&game, game.run.floor, game.mode);
    assert(minBet == 74);
    assert(getMaxBet(&game, minBet) == 100);

    game.player.chips = 300;
    assert(getMaxBet(&game, minBet) == 100);

    game.mode = MODE_CLASSIC;
    assert(getMaxBet(&game, 1) == 300);
}

static void testFloorMenuAndAdvancement(void) {
    Game game;
    int floorBefore;
    int chipsBefore;
    int nextMinBet;
    int fee;

    rngInit(67890U, 1);
    initGame(&game, MODE_CLASSIC);
    assert(handleFloorMenuAction(&game, 's', 1) == FLOOR_MENU_IGNORED);
    assert(handleFloorMenuAction(&game, 'w', 1) == FLOOR_MENU_IGNORED);
    assert(handleFloorMenuAction(&game, 'c', 1) == FLOOR_MENU_HANDLED);
    assert(game.state == STATE_BET);

    rngInit(67890U, 1);
    initGame(&game, MODE_ROGUELIKE);
    assert(handleFloorMenuAction(&game, 's', 1) == FLOOR_MENU_HANDLED);
    assert(game.state == STATE_SHOP);
    assert(handleShopMenuAction(&game, 'q') == 1);
    assert(game.state == STATE_MENU);

    nextMinBet = getMinBet(&game, game.run.floor + 1, game.mode);
    fee = 2 * nextMinBet;
    game.player.chips = fee + nextMinBet - 1;
    floorBefore = game.run.floor;
    chipsBefore = game.player.chips;
    assert(!advanceToNextFloor(&game));
    assert(game.run.floor == floorBefore);
    assert(game.player.chips == chipsBefore);

    game.player.chips = fee + nextMinBet;
    game.safetyNetArmed = 1;
    assert(advanceToNextFloor(&game));
    assert(game.run.floor == floorBefore + 1);
    assert(game.player.chips == nextMinBet);
    assert(game.safetyNetArmed == 0);
    assert(game.run.roundsOnFloor == 0);
}


static void testAdvanceForecastIgnoresCurrentFloorModifiers(void) {
    Game game;
    AdvanceForecast forecast;
    int baseNextBet;

    rngInit(4411U, 1);
    initGame(&game, MODE_ROGUELIKE);
    baseNextBet = getMinBet(&game, game.run.floor + 1, game.mode);

    game.run.twistId = TWIST_GREED;
    game.minBetMultiplier = 3;
    game.run.extraTwists[0] = TWIST_GREED;
    game.run.extraTwistCount = 1;
    computeAdvanceForecast(&game, &forecast);
    assert(forecast.nextMinBet == baseNextBet);
    assert(forecast.fee == 2 * baseNextBet);

    game.run.twistId = TWIST_NONE;
    game.minBetMultiplier = 1;
    game.run.extraTwistCount = 0;
    computeAdvanceForecast(&game, &forecast);
    assert(forecast.nextMinBet == baseNextBet);
}

static void testGreedCannotBlockAffordableFloorAdvance(void) {
    Game game;
    unsigned int seed;
    int skippedGreed = 0;

    for (seed = 1; seed <= 500 && !skippedGreed; seed++) {
        rngInit(seed, 1);
        initGame(&game, MODE_ROGUELIKE);
        game.run.floor = 3;
        game.run.previousTwistId = TWIST_NONE;
        game.player.chips = 30;

        assert(advanceToNextFloor(&game));
        assert(game.run.floor == 4);
        assert(game.player.chips == 10);
        assert(game.player.chips >=
               getMinBet(&game, game.run.floor, game.mode));

        if (game.run.twistId == TWIST_NONE) {
            skippedGreed = 1;
        }
    }

    assert(skippedGreed);
}

static void testClassicMenuReturnTransition(void) {
    Game game;

    rngInit(24680U, 1);
    initGame(&game, MODE_CLASSIC);
    game.state = STATE_MENU;
    assert(resumeClassicBetting(&game) == 1);
    assert(game.state == STATE_BET);

    game.mode = MODE_ROGUELIKE;
    game.state = STATE_MENU;
    assert(resumeClassicBetting(&game) == 0);
    assert(game.state == STATE_MENU);

    game.mode = MODE_CLASSIC;
    game.state = STATE_PLAYER_TURN;
    assert(resumeClassicBetting(&game) == 0);
    assert(game.state == STATE_PLAYER_TURN);
}

static Card coreCard(Rank rank) {
    Card card;

    card.suit = HEARTS;
    card.rank = rank;
    card.cursed = CURSED_NONE;
    return card;
}

static void addCoreCard(Hand *hand, Rank rank) {
    Card card = coreCard(rank);

    addCardToHand(hand, &card);
}

static void setupCoreSettlement(Game *game) {
    memset(game, 0, sizeof(*game));
    game->mode = MODE_CLASSIC;
    game->state = STATE_RESULT;
    game->run.floor = 1;
    game->run.dealerId = DEALER_CROUPIER;
    game->run.twistId = TWIST_NONE;
    game->minBetMultiplier = 1;
    game->player.chips = 100;
    game->player.handCount = 1;
    game->player.bets[0] = 10;
    game->dealer.handCount = 1;
}

static void testSettlementCore(void) {
    Game game;

    setupCoreSettlement(&game);
    addCoreCard(&game.player.hands[0], ACE);
    addCoreCard(&game.player.hands[0], KING);
    addCoreCard(&game.dealer.hands[0], TEN);
    addCoreCard(&game.dealer.hands[0], EIGHT);
    game.roundHadNaturalBj = 1;
    assert(resolveRound(&game) == MSG_WIN);
    assert(game.player.chips == 115);
    assert(game.state == STATE_BET);
    assert(game.roundInProgress == 0);

    setupCoreSettlement(&game);
    addCoreCard(&game.player.hands[0], TEN);
    addCoreCard(&game.player.hands[0], KING);
    addCoreCard(&game.dealer.hands[0], TEN);
    addCoreCard(&game.dealer.hands[0], EIGHT);
    assert(resolveRound(&game) == MSG_WIN);
    assert(game.player.chips == 110);
}

static void testGreedMarksUpdateRunEarnings(void) {
    Game game;

    setupCoreSettlement(&game);
    game.mode = MODE_ROGUELIKE;
    game.run.twistId = TWIST_GREED;
    game.run.marks = 4;
    game.totalMarksEarned = 4;
    addCoreCard(&game.player.hands[0], TEN);
    addCoreCard(&game.player.hands[0], KING);
    addCoreCard(&game.dealer.hands[0], TEN);
    addCoreCard(&game.dealer.hands[0], EIGHT);

    assert(resolveRound(&game) == MSG_WIN);
    assert(game.run.marks == 5);
    assert(game.totalMarksEarned == 5);
}

static void testSettlementPushAndSurrender(void) {
    Game game;

    setupCoreSettlement(&game);
    addCoreCard(&game.player.hands[0], TEN);
    addCoreCard(&game.player.hands[0], EIGHT);
    addCoreCard(&game.dealer.hands[0], TEN);
    addCoreCard(&game.dealer.hands[0], EIGHT);
    assert(resolveRound(&game) == MSG_PUSH);
    assert(game.player.chips == 100);
    assert(game.state == STATE_BET);

    setupCoreSettlement(&game);
    game.player.chips = 95;
    game.player.surrendered = 1;
    game.roundInProgress = 1;
    assert(resolveRound(&game) == MSG_DEFAULT);
    assert(game.player.chips == 95);
    assert(game.player.surrendered == 0);
    assert(game.roundInProgress == 0);
    assert(game.state == STATE_BET);
}

static void testInsuranceSettlement(void) {
    Game game;

    setupCoreSettlement(&game);
    addCoreCard(&game.player.hands[0], TEN);
    addCoreCard(&game.player.hands[0], EIGHT);
    addCoreCard(&game.dealer.hands[0], ACE);
    addCoreCard(&game.dealer.hands[0], KING);
    game.player.chips = 95;
    game.player.insuranceBet = 5;

    roundSettleInsurance(&game);

    assert(game.player.chips == 110);
    assert(game.player.insuranceBet == 0);
}

static void testTwinSplitAndPayoutCapCore(void) {
    Game game;

    setupCoreSettlement(&game);
    addCoreCard(&game.player.hands[0], TEN);
    addCoreCard(&game.player.hands[0], KING);
    addCoreCard(&game.dealer.hands[0], TEN);
    addCoreCard(&game.dealer.hands[0], EIGHT);
    game.dealer.handCount = 2;
    addCoreCard(&game.dealer.hands[1], TEN);
    addCoreCard(&game.dealer.hands[1], NINE);
    assert(resolveRound(&game) == MSG_WIN);
    assert(game.player.chips == 110);

    setupCoreSettlement(&game);
    addCoreCard(&game.player.hands[0], TEN);
    addCoreCard(&game.player.hands[0], KING);
    game.player.handCount = 2;
    game.player.bets[1] = 10;
    addCoreCard(&game.player.hands[1], TEN);
    addCoreCard(&game.player.hands[1], NINE);
    addCoreCard(&game.dealer.hands[0], TEN);
    addCoreCard(&game.dealer.hands[0], EIGHT);
    game.dealer.handCount = 2;
    addCoreCard(&game.dealer.hands[1], TEN);
    addCoreCard(&game.dealer.hands[1], TEN);
    assert(resolveRound(&game) == MSG_WIN);
    assert(game.player.chips == 105);

    setupCoreSettlement(&game);
    game.mode = MODE_ROGUELIKE;
    game.run.twistId = TWIST_DESPERATION;
    game.run.extraTwists[0] = TWIST_HIGH_STAKES;
    game.run.extraTwists[1] = TWIST_BLOOD_PACT;
    game.run.extraTwistCount = 2;
    game.run.roundsOnFloor = 1;
    game.player.bets[0] = 20;
    game.player.usedDouble[0] = 1;
    game.winStreak = 3;
    addCoreCard(&game.player.hands[0], TEN);
    addCoreCard(&game.player.hands[0], KING);
    addCoreCard(&game.dealer.hands[0], TEN);
    addCoreCard(&game.dealer.hands[0], EIGHT);
    assert(resolveRound(&game) == MSG_WIN);
    assert(game.player.chips == 500);
    assert(strstr(game.lastPayoutBreakdown, "Won 400") != NULL);
}

static void testUnaffordableRunTermination(void) {
    Game game;

    rngInit(13579U, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.player.chips = 9;
    game.state = STATE_MENU;
    assert(endRunIfUnaffordable(&game, 10) == 1);
    assert(game.state == STATE_QUIT);

    game.player.chips = 10;
    game.state = STATE_MENU;
    assert(endRunIfUnaffordable(&game, 10) == 0);
    assert(game.state == STATE_MENU);

    game.player.chips = 0;
    game.state = STATE_BET;
    assert(endRunIfUnaffordable(&game, 10) == 0);
    assert(game.state == STATE_BET);
}

static void testBossPrepTransaction(void) {
    Game game;

    rngInit(24680U, 1);
    initGame(&game, MODE_ROGUELIKE);
    game.state = STATE_BOSS_PREP;
    game.run.marks = 12;
    game.bossRulePaid = 5;
    completeBossPrep(&game, 1);
    assert(game.run.marks == 7);
    assert(game.bossRuleDisabled == 1);
    assert(game.state == STATE_BET);

    game.state = STATE_BOSS_PREP;
    game.run.marks = 3;
    game.bossRuleDisabled = 0;
    completeBossPrep(&game, 1);
    assert(game.run.marks == 3);
    assert(game.bossRuleDisabled == 0);
    assert(game.state == STATE_BET);

    game.state = STATE_BOSS_PREP;
    game.run.marks = 12;
    completeBossPrep(&game, 0);
    assert(game.run.marks == 12);
    assert(game.bossRuleDisabled == 0);
    assert(game.state == STATE_BET);

    game.state = STATE_MENU;
    completeBossPrep(&game, 1);
    assert(game.state == STATE_MENU);
    assert(game.run.marks == 12);
}

static void testInterruptedRoundRecovery(void) {
    Game game;

    rngInit(12345U, 1);
    initGame(&game, MODE_ROGUELIKE);

    game.player.bets[0] = 20;
    game.roundInProgress = 1;
    recoverInterruptedRound(&game);
    assert(game.player.chips == 80);
    assert(game.roundInProgress == 0);
    assert(game.state == STATE_BET);

    game.state = STATE_MENU;
    game.player.bets[0] = 5;
    recoverInterruptedRound(&game);
    assert(game.player.chips == 80);
    assert(game.state == STATE_MENU);
}


static void testSixthSenseProtectsTwistRerolls(void) {
    Game game;
    int seed;

    for (seed = 1; seed <= 100; seed++) {
        rngInit((unsigned int)seed, 1);
        initGame(&game, MODE_ROGUELIKE);
        game.run.floor = 4;
        game.run.marks = 30;
        game.run.twistId = TWIST_COLD_DECK;
        game.run.previousTwistId = TWIST_NONE;
        game.player.perkOwned[PERK_SIXTH_SENSE] = 1;

        assert(purchaseShopService(&game, 'r', getMinBet(
            &game, game.run.floor, game.mode)));
        assert(getTwistInfo(game.run.twistId)->category !=
               TWIST_CAT_HOSTILE);
        assert(game.run.marks == 0);
    }
}

int main(void) {
    testInitialStateAndShopOffers();
    testShopPurchaseTransactions();
    testSixthSenseProtectsTwistRerolls();
    testBetLimits();
    testFloorMenuAndAdvancement();
    testAdvanceForecastIgnoresCurrentFloorModifiers();
    testGreedCannotBlockAffordableFloorAdvance();
    testClassicMenuReturnTransition();
    testSettlementCore();
    testGreedMarksUpdateRunEarnings();
    testSettlementPushAndSurrender();
    testInsuranceSettlement();
    testTwinSplitAndPayoutCapCore();
    testUnaffordableRunTermination();
    testBossPrepTransaction();
    testInterruptedRoundRecovery();

    puts("renderer-free core tests passed");
    return 0;
}
