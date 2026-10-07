#include "game.h"
#include "round.h"
#include "render.h"
#include "menu.h"

#include <stdio.h>
#include <string.h>

void initGame(Game *game, GameMode mode) {
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
    game->mode = mode;
    initRun(&game->run);

    if (mode == MODE_CLASSIC) {
        game->run.dealerId = DEALER_CROUPIER;
        game->run.previousDealerId = DEALER_CROUPIER;
        game->run.twistId = TWIST_NONE;
        game->run.previousTwistId = TWIST_NONE;
    }

    game->peekActive = 0;
    game->fastMode = 0;
    game->fatigueTier = 0;
    game->winStreak = 0;
    game->lossStreak = 0;
    game->loanAmount = 0;
    game->loanDueFloor = 0;
    game->extraPerkSlots = 0;
    memset(game->player.perkOwned, 0, sizeof(game->player.perkOwned));
    memset(game->player.perkCharges, 0, sizeof(game->player.perkCharges));
    game->player.usedSplit = 0;
    game->player.usedDouble = 0;
    game->log.count = 0;

    {
        int lineIndex;

        for (lineIndex = 0; lineIndex < LOG_SIZE; lineIndex++) {
            game->log.lines[lineIndex][0] = '\0';
        }
    }
}
void logPush(Game *game, const char *msg, MessageStyle style) {
    int i;

    if (game->log.count < LOG_SIZE) {
        snprintf(game->log.lines[game->log.count],
                 LOG_LINE_LEN,
                 "%s",
                 msg);
        game->log.styles[game->log.count] = style;
        game->log.count++;
    } else {
        for (i = 0; i < LOG_SIZE - 1; i++) {
            memcpy(game->log.lines[i],
                   game->log.lines[i + 1],
                   LOG_LINE_LEN);
            game->log.styles[i] = game->log.styles[i + 1];
        }

        snprintf(game->log.lines[LOG_SIZE - 1],
                 LOG_LINE_LEN,
                 "%s",
                 msg);
        game->log.styles[LOG_SIZE - 1] = style;
    }
}


void advanceFloor(Game *game) {
    int bonus;
    int earned;
    char message[128];

    if (game->mode == MODE_CLASSIC) {
        return;
    }

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

    if (game->loanAmount > 0 &&
        game->run.floor >= game->loanDueFloor) {
        if (game->player.chips >= game->loanAmount) {
            game->player.chips -= game->loanAmount;
            logPush(game, "Loan repaid: -75 chips.", MSG_INFO);
            game->loanAmount = 0;
            game->loanDueFloor = 0;
        } else {
            logPush(game, "Loan defaulted. Run over.", MSG_LOSS);
            game->state = STATE_QUIT;
            return;
        }
    }

    game->run.twistRerolled = 0;
    game->run.twistBlocked = 0;
    game->run.roundsOnFloor = 0;
    game->run.extraTwistCount = 0;
    game->fatigueTier = 0;
    game->run.dealerId = selectDealerForFloor(game->run.floor,
                                          game->run.previousDealerId);

    if (game->player.perkOwned[PERK_PEEK]) {
        game->player.perkCharges[PERK_PEEK] = 1;
    }

    if (game->player.perkOwned[PERK_INSURANCE_PERK]) {
        game->player.perkCharges[PERK_INSURANCE_PERK] = 1;
    }

    if (game->player.perkOwned[PERK_DEALERS_FAVOR]) {
        game->player.perkCharges[PERK_DEALERS_FAVOR] = 1;
    }

    if (game->player.perkOwned[PERK_MARKED_CARD]) {
        game->player.perkCharges[PERK_MARKED_CARD] = 1;
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

    if (game->player.perkOwned[PERK_BANKROLL]) {
        game->player.chips += 10;
        logPush(game, "Bankroll: +10 chips.", MSG_WIN);
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
             getMinBet(game, game->run.floor, game->mode));
    logPush(game, message, MSG_DEFAULT);
}

int runGame(Game *game) {
    char finalMsg[64];

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
            if (game->mode == MODE_CLASSIC) {
                game->state = STATE_BET;
            } else {
                floorMenu(game);
            }
        }
    }

    snprintf(finalMsg,
             sizeof(finalMsg),
             "Game over. Final chips: %d.",
             game->player.chips);
    logPush(game, finalMsg, MSG_DEFAULT);
    renderTable(game, 0);
    renderShowCursor();

    return 0;
}
