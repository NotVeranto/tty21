#include "game.h"
#include "rng.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    Game game;
    char offers[7];
    int offerCount;
    int floorBefore;
    int marksBefore;

    rngInit(12345U, 1);
    initGame(&game, MODE_ROGUELIKE);

    assert(game.player.chips == 100);
    assert(game.state == STATE_BET);

    offerCount = getShopServiceOffers(&game, offers, 7);
    assert(offerCount > 0);
    assert(isShopServiceOffered(&game, 'v'));
    assert(canPurchaseShopService(&game, 'v', 10));
    marksBefore = game.run.marks;
    assert(purchaseShopService(&game, 'v', 10));
    assert(game.player.chips == 90);
    assert(game.run.marks == marksBefore + 2);

    game.player.bets[0] = 20;
    game.roundInProgress = 1;
    recoverInterruptedRound(&game);
    assert(game.player.chips == 70);
    assert(game.roundInProgress == 0);
    assert(game.state == STATE_BET);

    floorBefore = game.run.floor;
    advanceFloor(&game);
    assert(game.run.floor == floorBefore + 1);

    puts("renderer-free core tests passed");
    return 0;
}
