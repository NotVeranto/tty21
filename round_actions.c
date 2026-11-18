#define _DEFAULT_SOURCE
#include "round_actions.h"

#include "round.h"
#include "round_rules.h"
#include "round_cards.h"
#include "round_economy.h"
#include "round_input.h"
#include "round_presentation.h"
#include "input.h"
#include "input_map.h"
#include "rng.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int getTotalExposure(const Game *game) {
    long long exposure = game->player.insuranceBet;
    int handIndex;
    for (handIndex = 0; handIndex < game->player.handCount && handIndex < MAX_PLAYER_HANDS; handIndex++) exposure += game->player.bets[handIndex];
    return exposure > INT_MAX ? INT_MAX : (int)exposure;
}

static int canDouble(const Game *game) {
    if (game->run.floor >= 10 && game->run.dealerId == DEALER_VOID) return 0;
    return game->player.hands[game->player.activeHand].count == 2 &&
           (long long)game->player.chips - getTotalExposure(game) >= game->player.bets[game->player.activeHand];
}

static int canSplit(const Game *game) {
    int handLimit;

    if (game->run.floor >= 10 && game->run.dealerId == DEALER_TWIN) return 0;

    handLimit = MAX_PLAYER_HANDS - 1;
    if (game->player.perkOwned[PERK_SPLIT_MASTERY]) {
        handLimit = MAX_PLAYER_HANDS;
    }

    return game->player.hands[game->player.activeHand].count == 2 &&
           game->player.hands[game->player.activeHand].cards[0].rank ==
               game->player.hands[game->player.activeHand].cards[1].rank &&
           game->player.handCount < handLimit &&
           (long long)game->player.chips - getTotalExposure(game) >= game->player.bets[game->player.activeHand] &&
           !game->player.isSplitAce[game->player.activeHand];
}

static void advanceActiveHand(Game *game) {
    game->player.activeHand++;

    if (game->player.activeHand >= game->player.handCount) {
        game->state = STATE_DEALER_TURN;
    }
}

static void splitHand(Game *game) {
    int src;
    int dst;
    int isAceSplit;

    src = game->player.activeHand;
    dst = game->player.handCount;
    game->player.usedSplit[src] = 1;
    game->player.usedSplit[dst] = 1;

    game->player.hands[dst].cards[0] =
        game->player.hands[src].cards[1];
    game->player.hands[dst].count = 1;
    game->player.hands[dst].value =
        calculateHandValue(&game->player.hands[dst]);

    game->player.hands[src].count = 1;
    game->player.hands[src].value =
        calculateHandValue(&game->player.hands[src]);

    game->player.bets[dst] = game->player.bets[src];
    game->player.isSplitAce[dst] = 0;
    game->player.handCount++;

    isAceSplit = game->player.hands[src].cards[0].rank == ACE;

    drawCard(game, &game->player.hands[src], 1);
    drawCard(game, &game->player.hands[dst], 1);

    if (isAceSplit) {
        game->player.isSplitAce[src] = 1;
        game->player.isSplitAce[dst] = 1;
        advanceActiveHand(game);
        advanceActiveHand(game);
    } else if (calculateHandValue(&game->player.hands[src]) == 21) {
        advanceActiveHand(game);

        if (game->player.activeHand == dst &&
            calculateHandValue(&game->player.hands[dst]) == 21) {
            advanceActiveHand(game);
        }
    }

    {
        char msg[64];

        snprintf(msg, sizeof(msg),
                 "Split. %d hands.",
                 game->player.handCount);
        logPush(game, msg, MSG_ACTION);
    }

    roundPresentTable(game, game->state == STATE_PLAYER_TURN);
}

static int buildPlayerActions(const Game *game, ActionId *actions) {
    int actionCount;

    actionCount = 0;
    if (game->player.hands[game->player.activeHand].count < MAX_HAND_SIZE)
        actions[actionCount++] = ACTION_HIT;
    actions[actionCount++] = ACTION_STAND;

    if (canDouble(game)) {
        actions[actionCount++] = ACTION_DOUBLE;
    }

    if (canSplit(game)) {
        actions[actionCount++] = ACTION_SPLIT;
    }

    if ((game->player.perkOwned[PERK_PEEK] &&
         game->player.perkCharges[PERK_PEEK] > 0) ||
        game->freePeekReady) {
        actions[actionCount++] = ACTION_PEEK;
    }

    if (game->player.hands[game->player.activeHand].count == 2 &&
        !game->player.usedDouble[game->player.activeHand] &&
        !game->player.usedSplit[game->player.activeHand]) {
        actions[actionCount++] = ACTION_SURRENDER;
    }

    actions[actionCount++] = ACTION_QUIT;

    return actionCount;
}

static void dispatchPlayerAction(Game *game, ActionId action) {
    int value;

    if (action == ACTION_HIT) {
        if (game->player.hands[game->player.activeHand].count >= MAX_HAND_SIZE) {
            logPush(game, "Hand is full.", MSG_DEFAULT);
            roundPresentTable(game, game->state == STATE_PLAYER_TURN);
            return;
        }
        if (game->run.floor >= 10 && game->run.dealerId == DEALER_DEVIL) {
            int toll = 3;
            if (toll > game->player.chips) toll = game->player.chips;
            game->player.chips -= toll;
            {
                char tollMessage[64];
                snprintf(tollMessage, sizeof(tollMessage),
                         "Devil's toll: -%d chips.", toll);
                logPush(game, tollMessage, MSG_LOSS);
            }
        }
        drawCard(game, &game->player.hands[game->player.activeHand], 1);

        if (effectiveDealerMirrorsOnHit(game)) {
            drawCard(game, &game->dealer.hands[0], 0);
        }

        value = calculatePlayerHandValue(game);

        if (value > 21) {
            if (game->player.perkOwned[PERK_SECOND_WIND] &&
                game->player.perkCharges[PERK_SECOND_WIND] > 0) {
                game->player.perkCharges[PERK_SECOND_WIND]--;
                resetPlayerHandToEleven(game);
                logPush(game,
                         "Second Wind! Hand reset to 11.",
                         MSG_WIN);
                roundPresentTable(game, 1);
            } else {
                advanceActiveHand(game);
            }
        } else if (value == 21) {
            advanceActiveHand(game);
        } else {
            char msg[64];

            snprintf(msg, sizeof(msg),
                     "Hit. Total %d.",
                     value);
            logPush(game, msg, MSG_ACTION);
        }

        roundPresentTable(game, game->state == STATE_PLAYER_TURN);
    } else if (action == ACTION_STAND) {
        if (twistActive(game, TWIST_STICKY_HANDS) &&
            calculatePlayerHandValue(game) < 15) {
            logPush(game,
                    "Sticky Hands: cannot stand below 15.",
                    MSG_DEFAULT);
        } else if (game->run.floor >= 10 &&
                   game->run.dealerId == DEALER_BUTCHER &&
                   calculatePlayerHandValue(game) < 15) {
            logPush(game,
                    "Butcher: cannot stand below 15.",
                    MSG_DEFAULT);
        } else if (game->player.hands[game->player.activeHand].count > 0) {
            int hasEcho;
            int cardIndex;

            hasEcho = 0;

            for (cardIndex = 0;
                 cardIndex < game->player.hands[game->player.activeHand].count;
                 cardIndex++) {
                if (game->player.hands[game->player.activeHand]
                        .cards[cardIndex].cursed == CURSED_ECHO) {
                    hasEcho = 1;
                }
            }

            if (hasEcho && rngRange(0, 99) < 10) {
                drawCard(game,
                         &game->player.hands[game->player.activeHand], 1);
                logPush(game,
                        "Echo triggers! Extra card drawn.",
                        MSG_INFO);

                value = calculatePlayerHandValue(game);

                if (value > 21) {
                    if (game->player.perkOwned[PERK_SECOND_WIND] &&
                        game->player.perkCharges[PERK_SECOND_WIND] > 0) {
                        game->player.perkCharges[PERK_SECOND_WIND]--;
                        resetPlayerHandToEleven(game);
                        logPush(game,
                                "Second Wind! Hand reset to 11.",
                                MSG_WIN);
                    } else {
                        advanceActiveHand(game);
                    }
                } else {
                    advanceActiveHand(game);
                }
            } else {
                char msg[64];

                snprintf(msg, sizeof(msg),
                         "Stand on %d.",
                         calculatePlayerHandValue(game));
                logPush(game, msg, MSG_ACTION);
                advanceActiveHand(game);
            }

            roundPresentTable(game, game->state == STATE_PLAYER_TURN);
        }
    } else if (action == ACTION_DOUBLE) {
        if (!canDouble(game)) {
            logPush(game,
                     "Cannot double now.",
                     MSG_DEFAULT);
        } else {
            game->player.bets[game->player.activeHand] *= 2;
            game->player.usedDouble[game->player.activeHand] = 1;
            drawCard(game, &game->player.hands[game->player.activeHand], 1);

            if (effectiveDealerMirrorsOnHit(game)) {
                drawCard(game, &game->dealer.hands[0], 0);
            }

            {
                char msg[64];

                snprintf(msg, sizeof(msg),
                         "Double. Total %d.",
                         calculatePlayerHandValue(game));
                logPush(game, msg, MSG_ACTION);
            }

            advanceActiveHand(game);
            roundPresentTable(game, game->state == STATE_PLAYER_TURN);
        }
    } else if (action == ACTION_SPLIT) {
        if (!canSplit(game)) {
            logPush(game,
                     "Cannot split now.",
                     MSG_DEFAULT);
        } else {
            splitHand(game);
        }
    } else if (action == ACTION_PEEK) {
        if (!game->player.perkOwned[PERK_PEEK] &&
            !game->freePeekReady) {
            logPush(game,
                    "You do not own Peek.",
                    MSG_DEFAULT);
        } else if (game->player.perkOwned[PERK_PEEK] &&
                   game->player.perkCharges[PERK_PEEK] <= 0 &&
                   !game->freePeekReady) {
            logPush(game,
                    "Peek already used this floor.",
                    MSG_DEFAULT);
        } else if (game->peekActive) {
            logPush(game,
                    "Peek already used this round.",
                    MSG_DEFAULT);
        } else {
            if (game->player.perkOwned[PERK_PEEK] &&
                game->player.perkCharges[PERK_PEEK] > 0) {
                game->player.perkCharges[PERK_PEEK]--;
            } else {
                game->player.perkCharges[PERK_PEEK] = 0;
            }

            if (game->freePeekReady) {
                game->freePeekReady = 0;
            }

            game->peekActive = 1;
            logPush(game,
                    "Peek: hole card revealed.",
                    MSG_ACTION);
            roundPresentTable(game, 1);
        }
    } else if (action == ACTION_SURRENDER) {
        int bet = game->player.bets[game->player.activeHand];
        (void)applyLoss(game, bet / 2);
        game->player.surrendered = 1;
        logPush(game, "You surrendered. Half your bet is lost.", MSG_LOSS);
        game->state = STATE_RESULT;
    } else if (action == ACTION_QUIT) {
        game->playerQuit = 1;
        game->state = STATE_QUIT;
    }
}

void playerTurn(Game *game) {
    ActionId actions[ACTION_COUNT + 1];
    KeyEvent event;
    int actionCount;
    int actionIndex;
    int matchedIndex;
    int inputCharacter;
    ActionId action;

    actionIndex = 0;

    if (inputIsRaw()) {
        while (game->state == STATE_PLAYER_TURN) {
            actionCount = buildPlayerActions(game, actions);

            if (actionIndex >= actionCount) {
                actionIndex = actionCount - 1;
            }

            roundPresentTable(game, 1);
            roundPresentActionBar(actions,
                            actionCount,
                            actionIndex, game->mode == MODE_ROGUELIKE);

            event = readKey();

            if (event.type == KEY_CHAR && event.ch == '?') {
                roundPresentHelp(game);
                (void)readKey();
                roundPresentTable(game, 1);
                roundPresentActionBar(actions,
                                actionCount,
                                actionIndex,
                                ROUND_MESSAGE_PROMPT,
                                game->mode == MODE_ROGUELIKE);
                continue;
            }

            if (event.type == KEY_LEFT) {
                actionIndex = moveSelection(actionIndex,
                                            actionCount,
                                            0,
                                            -1);
            } else if (event.type == KEY_RIGHT) {
                actionIndex = moveSelection(actionIndex,
                                            actionCount,
                                            0,
                                            1);
            } else if (event.type == KEY_ENTER) {
                dispatchPlayerAction(game, actions[actionIndex]);
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_UP || event.type == KEY_DOWN ||
                       event.type == KEY_NONE) {
                continue;
            } else if (event.type == KEY_CHAR) {
                action = mapPlayerKey(event.ch);
                matchedIndex = -1;

                if ((int)action >= 0) {
                    {
                        int scanIndex;
                        for (scanIndex = 0; scanIndex < actionCount; scanIndex++) {
                            if (actions[scanIndex] == action) {
                                matchedIndex = scanIndex;
                            }
                        }
                    }

                    if (matchedIndex >= 0) {
                        actionIndex = matchedIndex;
                        dispatchPlayerAction(game, action);
                    }
                }
            } else if (event.type == KEY_EOF) {
                game->playerQuit = 1;
                game->state = STATE_QUIT;
            }
        }
    } else {
        while (game->state == STATE_PLAYER_TURN) {
            actionCount = buildPlayerActions(game, actions);

            if (actionIndex >= actionCount) {
                actionIndex = actionCount - 1;
            }

            roundPresentTable(game, 1);
            roundPresentMessage("Choose action: [h/s/d/p/k/u/q]: ",
                          ROUND_MESSAGE_PROMPT,
                          MSG_DEFAULT);
            inputCharacter = getchar();

            if (inputCharacter == EOF) {
                game->playerQuit = 1;
                game->state = STATE_QUIT;
            } else {
                event = makeKeyEvent(inputCharacter);

                while ((inputCharacter = getchar()) != '\n' &&
                       inputCharacter != EOF) {
                }

                if (event.type == KEY_CHAR && event.ch == '?') {
                    roundPresentHelp(game);
                    roundPresentTable(game, 1);
                    roundPresentActionBar(actions,
                                    actionCount,
                                    actionIndex,
                                    ROUND_MESSAGE_PROMPT,
                                    game->mode == MODE_ROGUELIKE);
                    continue;
                }

                if (event.type == KEY_CHAR) {
                    action = mapPlayerKey(event.ch);
                    matchedIndex = -1;

                    if ((int)action >= 0) {
                        {
                            int scanIndex;
                            for (scanIndex = 0; scanIndex < actionCount; scanIndex++) {
                                if (actions[scanIndex] == action) {
                                    matchedIndex = scanIndex;
                                }
                            }
                        }

                        if (matchedIndex >= 0) {
                            actionIndex = matchedIndex;
                            dispatchPlayerAction(game, action);
                        }
                    } else {
                        roundPresentMessage("Invalid action. Enter h, s, d, p, k, or q.",
                                      ROUND_MESSAGE_PROMPT,
                                      MSG_DEFAULT);
                    }
                } else {
                    roundPresentMessage("Invalid action. Enter h, s, d, p, k, or q.",
                                  ROUND_MESSAGE_PROMPT,
                                  MSG_DEFAULT);
                }
            }
        }
    }
}
