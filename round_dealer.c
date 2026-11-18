#define _DEFAULT_SOURCE
#include "round_dealer.h"

#include "round.h"
#include "round_cards.h"
#include "round_rules.h"
#include "dealers.h"
#include "round_presentation.h"
#include "input.h"

#include <stdio.h>
#include <unistd.h>

void dealerTurn(Game *game) {
    int targetValue;
    int dealerHandIndex;
    int dealerValue;

    if (game->state == STATE_DEALER_TURN) {
        if (!game->fastMode &&
            isatty(STDOUT_FILENO) &&
            !game->peekActive &&
            !twistActive(game, TWIST_SNIPER)) {
            roundPresentMessage("DEALER REVEALS...", ROUND_MESSAGE_PROMPT, MSG_INFO);
            roundPause(game, 400);
            flushInput();
            roundPresentTable(game, 0);
        }

        targetValue = dealerTargetValue(game->run.dealerId);

        if (twistActive(game, TWIST_DEALER_TO_18) &&
            targetValue < 18) {
            targetValue = 18;
        }

        if (game->mode == MODE_ROGUELIKE &&
            game->run.floor >= 10 &&
            game->run.roundsOnFloor >= 3) {
            if (!game->dealerEnraged) {
                logPush(game,
                        "Dealer is enraged. Draws one higher.",
                        MSG_LOSS);
                game->dealerEnraged = 1;
            }

            targetValue++;
            if (targetValue > 21) {
                targetValue = 21;
            }
        }

        if (game->run.floor >= 10) {
            if (game->run.dealerId == DEALER_CLOCKMAKER && targetValue < 19) {
                targetValue = 19;
            } else if (game->run.dealerId == DEALER_ORACLE && targetValue < 18) {
                targetValue = 18;
            }
        }

        for (dealerHandIndex = 0;
             dealerHandIndex < game->dealer.handCount;
             dealerHandIndex++) {
            dealerValue = calculateDealerHandValue(
                game,
                &game->dealer.hands[dealerHandIndex],
                game->run.dealerId);

            while (dealerValue < targetValue) {
                KeyEvent event;

                int prevCount = game->dealer.hands[dealerHandIndex].count;

                drawCard(game,
                         &game->dealer.hands[dealerHandIndex], 0);
                if (game->dealer.hands[dealerHandIndex].count == prevCount) {
                    break;
                }
                dealerValue = calculateDealerHandValue(
                    game,
                    &game->dealer.hands[dealerHandIndex],
                    game->run.dealerId);
                if (!game->fastMode && isatty(STDOUT_FILENO)) {
                    roundPresentTable(game, 0);
                }

                if (!game->fastMode &&
                    isatty(STDOUT_FILENO) &&
                    inputIsRaw()) {
                    roundPresentMessage("Press any key for next card...",
                                  ROUND_MESSAGE_PROMPT,
                                  MSG_INFO);
                    event = readKey();

                    if (event.type == KEY_EOF) {
                        break;
                    }
                }
            }
        }

        {
            char msg[64];

            for (dealerHandIndex = 0;
                 dealerHandIndex < game->dealer.handCount;
                 dealerHandIndex++) {
                dealerValue = calculateDealerHandValue(
                    game,
                    &game->dealer.hands[dealerHandIndex],
                    game->run.dealerId);

                if (game->dealer.handCount == 1) {
                    if (dealerValue > 21) {
                        snprintf(msg, sizeof(msg),
                                 "Dealer busts with %d.",
                                 dealerValue);
                    } else {
                        snprintf(msg, sizeof(msg),
                                 "Dealer stands on %d.",
                                 dealerValue);
                    }
                } else if (dealerValue > 21) {
                    snprintf(msg, sizeof(msg),
                             "Dealer hand %d: busts with %d.",
                             dealerHandIndex + 1,
                             dealerValue);
                } else {
                    snprintf(msg, sizeof(msg),
                             "Dealer hand %d: stands on %d.",
                             dealerHandIndex + 1,
                             dealerValue);
                }

                if (dealerValue > 21) {
                    logPush(game, msg, MSG_WIN);
                } else {
                    logPush(game, msg, MSG_DEALER);
                }
            }
        }

        game->state = STATE_RESULT;
    }
}
