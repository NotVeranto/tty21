#include "controller.h"

#include "input.h"
#include "menu.h"
#include "render.h"
#include "rng.h"
#include "round.h"
#include "round_actions.h"
#include "round_dealer.h"
#include "round_settlement.h"
#include "round_cards.h"
#include "save.h"

#include <ctype.h>
#include <stdio.h>

static int controllerAllHandsBust(const Game *game) {
    int handIndex;
    int foundHand = 0;

    for (handIndex = 0;
         handIndex < game->player.handCount &&
         handIndex < MAX_PLAYER_HANDS;
         handIndex++) {
        if (game->player.hands[handIndex].count < 1) {
            continue;
        }
        foundHand = 1;
        if (calculatePlayerHandValueForIndex(game, handIndex) <= 21) {
            return 0;
        }
    }

    return foundHand;
}

static void presentRoundResult(Game *game) {
    MessageStyle resultStyle = resolveRound(game);

    renderTable(game, 0);
    if (resultStyle == MSG_WIN) {
        renderResultFlash(game, "YOU WIN!", MSG_WIN);
        renderPause(game, 600);
    } else if (resultStyle == MSG_PUSH) {
        renderResultFlash(game, "PUSH", MSG_PUSH);
        renderPause(game, 600);
    } else if (resultStyle == MSG_LOSS && controllerAllHandsBust(game)) {
        renderResultFlash(game, "BUST!", MSG_LOSS);
        renderPause(game, 600);
    }
}

void bossPrepTurn(Game *game) {
    KeyEvent event;
    int answer;
    renderBossPrep(game);
    answer = EOF;
    if (inputIsRaw()) {
        event = readKey();
        if (event.type == KEY_CHAR) answer = tolower((unsigned char)event.ch);
    } else {
        answer = getchar();
        if (answer != EOF) {
            answer = tolower((unsigned char)answer);
            while (answer != '\n' && getchar() != '\n' && !feof(stdin)) {
            }
        }
    }
    if (answer == 'y' && game->run.marks >= game->bossRulePaid) {
        game->run.marks -= game->bossRulePaid;
        game->bossRuleDisabled = 1;
    }
    game->state = STATE_BET;
}

int runGame(Game *game) {
    char finalMsg[64];

    while (game->state != STATE_QUIT) {
        if (game->mode == MODE_ROGUELIKE &&
            game->saveEnabled &&
            (game->state == STATE_BET ||
             game->state == STATE_MENU)) {
            game->rngCalls = rngGetCallCount();
            if (saveGame(game) == 0) {
                game->lastAutosavedRound = game->roundNumber;
            }
        }
        if (game->state == STATE_BET) {
            startRound(game);
        } else if (game->state == STATE_PLAYER_TURN) {
            playerTurn(game);
        } else if (game->state == STATE_DEALER_TURN) {
            dealerTurn(game);
        } else if (game->state == STATE_RESULT) {
            presentRoundResult(game);
        } else if (game->state == STATE_SHOP) {
            runShop(game);
        } else if (game->state == STATE_BOSS_PREP) {
            bossPrepTurn(game);
        } else if (game->state == STATE_MENU) {
            if (game->mode == MODE_CLASSIC) {
                game->state = STATE_BET;
            } else {
                floorMenu(game);
            }
        }
    }

    if (game->mode == MODE_ROGUELIKE &&
        game->saveEnabled &&
        !game->playerQuit) {
        deleteSave();
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
