#include "game.h"
#include "round.h"
#include "render.h"
#include "menu.h"

#include <stdio.h>
#include <string.h>





void initGame(Game *game) {
    initDeck(&game->deck);
    shuffleDeck(&game->deck);
    memset(game->player.hands, 0, sizeof(game->player.hands));
    memset(game->player.bets, 0, sizeof(game->player.bets));
    memset(game->player.isSplitAce, 0, sizeof(game->player.isSplitAce));
    game->player.handCount = 1;
    game->player.activeHand = 0;
    resetHand(&game->dealer.hands[0]);
    resetHand(&game->dealer.hands[1]);
    game->dealer.handCount = 1;
    game->player.chips = 100;
    game->state = STATE_BET;
    game->roundNumber = 0;
    initRun(&game->run);
    game->peekActive = 0;
    memset(game->player.perkOwned, 0, sizeof(game->player.perkOwned));
    memset(game->player.perkCharges, 0, sizeof(game->player.perkCharges));
    game->player.usedSplit = 0;
    game->player.usedDouble = 0;
}

int calculatePlayerHandValue(const Game *game) {
    int totalValue;
    int cardIndex;

    if (!game->player.perkOwned[PERK_ACE_HIGH]) {
        return game->player.hands[game->player.activeHand].value;
    }

    totalValue = 0;

    for (cardIndex = 0; cardIndex < game->player.hands[game->player.activeHand].count; cardIndex++) {
        totalValue += getCardValue(&game->player.hands[game->player.activeHand].cards[cardIndex]);
    }

    return totalValue;
}

void advanceFloor(Game *game) {
    int bonus;
    int earned;
    char message[128];

    bonus = 0;

    if (game->player.chips > 100) {
        bonus = (game->player.chips - 100) / 20;

        if (bonus > 20) {
            bonus = 20;
        }
    }

    earned = 10 + game->run.floor * 3 + bonus;
    game->run.marks += earned;
    game->run.previousDealerId = game->run.dealerId;
    game->run.previousTwistId = game->run.twistId;
    game->run.floor++;
    game->run.dealerId = selectDealerForFloor(game->run.floor,
                                          game->run.previousDealerId);

    if (game->player.perkOwned[PERK_PEEK]) {
        game->player.perkCharges[PERK_PEEK] = 1;
    }

    {
        int attempts;

        attempts = 0;

        do {
            game->run.twistId = selectTwistForFloor(game->run.floor,
                                                game->run.previousTwistId);
            attempts++;
        } while (game->player.perkOwned[PERK_SIXTH_SENSE] &&
                 game->run.floor >= 4 &&
                 getTwistInfo(game->run.twistId)->category == TWIST_CAT_HOSTILE &&
                 attempts < 10);
    }

    {
        int handIndex;

        for (handIndex = 0; handIndex < MAX_PLAYER_HANDS; handIndex++) {
            resetHand(&game->player.hands[handIndex]);
        }
    }

    memset(game->player.isSplitAce, 0, sizeof(game->player.isSplitAce));
    game->player.handCount = 1;
    game->player.activeHand = 0;
    resetHand(&game->dealer.hands[0]);
    resetHand(&game->dealer.hands[1]);
    game->dealer.handCount = 1;

    snprintf(message,
             sizeof(message),
             "Floor %d. Marks: %d. Minimum bet: %d.",
             game->run.floor,
             game->run.marks,
             getMinBet(game->run.floor));
    renderMessage(message, ROW_RESULT, MSG_DEFAULT);
}







int runGame(Game *game) {
    char finalChipsMessage[64];

    while (game->state != STATE_QUIT) {
        if (game->state == STATE_BET) {
            startRound(game);
        } else if (game->state == STATE_PLAYER_TURN) {
            playerTurn(game);
        } else if (game->state == STATE_DEALER_TURN) {
            dealerTurn(game);
        } else if (game->state == STATE_RESULT) {
            resolveRound(game);
        } else if (game->state == STATE_SHOP) {
            runShop(game);
        } else if (game->state == STATE_MENU) {
            floorMenu(game);
        }
    }

    renderMessage("Game over.", ROW_OVER, MSG_DEFAULT);
    snprintf(finalChipsMessage,
             sizeof(finalChipsMessage),
             "Final chips: %d",
             game->player.chips);
    renderMessage(finalChipsMessage, ROW_FINAL, MSG_DEFAULT);

    return 0;
}
