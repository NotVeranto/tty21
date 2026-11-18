#include "game.h"
#include "rng.h"

#include <assert.h>
#include <stdio.h>

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

int main(void) {
    testInitialStateAndShopOffers();
    testShopPurchaseTransactions();
    testFloorMenuAndAdvancement();
    testInterruptedRoundRecovery();

    puts("renderer-free core tests passed");
    return 0;
}
