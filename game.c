#include "game.h"
#include "render.h"
#include "input.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keeps a safety margin before reshuffling at the start of a round.
#define DECK_RESHUFFLE_MARGIN 15
#define FIRST_CARD_RETRY_LIMIT 50

// Draws the next card and advances the deck position.
static void drawCard(Deck *deck, Hand *hand) {
    if (deck->top >= DECK_SIZE) {
        initDeck(deck);
        shuffleDeck(deck);
    }

    addCardToHand(hand, &deck->cards[deck->top]);
    deck->top++;
}

// Replaces the player's first card while keeping the other dealt cards intact.
static void redrawFirstPlayerCard(Game *game, int requireHighCard) {
    int attemptCount;
    int accepted;
    Card replacementCard;

    attemptCount = 0;
    accepted = 0;

    while (attemptCount < FIRST_CARD_RETRY_LIMIT && !accepted) {
        if (game->deck.top >= DECK_SIZE) {
            initDeck(&game->deck);
            shuffleDeck(&game->deck);
        }

        replacementCard = game->deck.cards[game->deck.top];
        game->deck.top++;

        if ((requireHighCard && replacementCard.rank >= EIGHT) ||
            (!requireHighCard && replacementCard.rank <= FIVE)) {
            game->player.hand.cards[0] = replacementCard;
            accepted = 1;
        }

        attemptCount++;
    }

    game->player.hand.value = calculateHandValue(&game->player.hand);
}

// Applies a loss and activates Survival when the loss would end the run.
static int applyLoss(Game *game, long long lossValue) {
    int chipsBeforeLoss;
    int lossAmount;
    int remainingChips;

    chipsBeforeLoss = game->player.chips;

    if (chipsBeforeLoss <= 0) {
        return 0;
    }

    if (lossValue > chipsBeforeLoss) {
        lossAmount = chipsBeforeLoss;
    } else {
        lossAmount = (int)lossValue;
    }

    if (game->player.perkOwned[PERK_SURVIVAL] &&
        lossValue >= chipsBeforeLoss) {
        // Survival halves the pre-loss chips. With 1 chip this is 0,
        // which ends the run as specified.
        remainingChips = chipsBeforeLoss / 2;

        game->player.chips = remainingChips;
        lossAmount = chipsBeforeLoss - remainingChips;
    } else {
        game->player.chips -= lossAmount;
    }

    return lossAmount;
}

// Resets the player to a synthetic ace with value 11.
static void resetPlayerHandToEleven(Game *game) {
    Card recoveryCard;

    resetHand(&game->player.hand);
    recoveryCard.suit = HEARTS;
    recoveryCard.rank = ACE;
    addCardToHand(&game->player.hand, &recoveryCard);
    game->player.hand.value = 11;
}

static KeyEvent makeKeyEvent(int character) {
    KeyEvent event;

    event.type = KEY_NONE;
    event.ch = '\0';

    if (character == EOF) {
        event.type = KEY_EOF;
    } else if (character == '\r' || character == '\n') {
        event.type = KEY_ENTER;
    } else if (character == 27) {
        event.type = KEY_ESC;
    } else {
        event.type = KEY_CHAR;
        event.ch = (char)character;
    }

    return event;
}

static int moveSelection(int selectedIndex,
                         int itemCount,
                         int skippedMask,
                         int direction) {
    int nextIndex;
    int attemptCount;

    nextIndex = selectedIndex;
    attemptCount = 0;

    while (attemptCount < itemCount) {
        nextIndex += direction;

        if (nextIndex < 0) {
            nextIndex = itemCount - 1;
        } else if (nextIndex >= itemCount) {
            nextIndex = 0;
        }

        if ((skippedMask & (1 << nextIndex)) == 0) {
            return nextIndex;
        }

        attemptCount++;
    }

    return selectedIndex;
}

static int allPerksOwned(const Game *game) {
    int perkIndex;

    for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
        if (!game->player.perkOwned[perkIndex]) {
            return 0;
        }
    }

    return 1;
}

static int canDouble(const Game *game) {
    return game->player.hand.count == 2 &&
           (long long)game->player.chips >=
               (long long)game->player.bet * 2;
}

static int findPlayerActionIndex(const ActionId *actions,
                                 int actionCount,
                                 char character) {
    ActionId targetAction;
    int actionIndex;

    if (character == 'h' || character == 'H') {
        targetAction = ACTION_HIT;
    } else if (character == 's' || character == 'S') {
        targetAction = ACTION_STAND;
    } else if (character == 'd' || character == 'D') {
        targetAction = ACTION_DOUBLE;
    } else if (character == 'p' || character == 'P') {
        targetAction = ACTION_PEEK;
    } else if (character == 'q' || character == 'Q') {
        targetAction = ACTION_QUIT;
    } else {
        return -1;
    }

    for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
        if (actions[actionIndex] == targetAction) {
            return actionIndex;
        }
    }

    return -1;
}

static int buildPlayerActions(const Game *game, ActionId *actions) {
    int actionCount;

    actionCount = 0;
    actions[actionCount++] = ACTION_HIT;
    actions[actionCount++] = ACTION_STAND;

    if (canDouble(game)) {
        actions[actionCount++] = ACTION_DOUBLE;
    }

    if (game->player.perkOwned[PERK_PEEK] &&
        game->player.perkCharges[PERK_PEEK] > 0) {
        actions[actionCount++] = ACTION_PEEK;
    }

    actions[actionCount++] = ACTION_QUIT;

    return actionCount;
}

static void dispatchPlayerAction(Game *game, ActionId action) {
    if (action == ACTION_HIT) {
        drawCard(&game->deck, &game->player.hand);

        if (game->run.dealerId == DEALER_MIRROR) {
            drawCard(&game->deck, &game->dealer.hand);
        }

        if (calculatePlayerHandValue(game) > 21) {
            game->state = STATE_RESULT;
        }
    } else if (action == ACTION_STAND) {
        if (game->run.twistId == TWIST_STICKY_HANDS &&
            calculatePlayerHandValue(game) < 15) {
            renderMessage("Sticky Hands: cannot stand below 15.",
                          ROW_RESULT,
                          MSG_DEFAULT);
        } else {
            game->state = STATE_DEALER_TURN;
        }
    } else if (action == ACTION_DOUBLE) {
        if (!canDouble(game)) {
            renderMessage("Cannot double now.",
                          ROW_RESULT,
                          MSG_DEFAULT);
        } else {
            game->player.bet *= 2;
            drawCard(&game->deck, &game->player.hand);

            if (game->run.dealerId == DEALER_MIRROR) {
                drawCard(&game->deck, &game->dealer.hand);
            }

            if (calculatePlayerHandValue(game) > 21) {
                game->state = STATE_RESULT;
            } else {
                game->state = STATE_DEALER_TURN;
            }
        }
    } else if (action == ACTION_PEEK) {
        if (!game->player.perkOwned[PERK_PEEK]) {
            renderMessage("You do not own Peek.",
                          ROW_RESULT,
                          MSG_DEFAULT);
        } else if (game->player.perkCharges[PERK_PEEK] <= 0) {
            renderMessage("Peek already used this floor.",
                          ROW_RESULT,
                          MSG_DEFAULT);
        } else if (game->peekActive) {
            renderMessage("Peek already used this round.",
                          ROW_RESULT,
                          MSG_DEFAULT);
        } else {
            game->player.perkCharges[PERK_PEEK]--;
            game->peekActive = 1;
            renderTable(game, 1);
            renderMessage("Peek: hole card revealed.",
                          ROW_RESULT,
                          MSG_DEFAULT);
        }
    } else if (action == ACTION_QUIT) {
        game->state = STATE_QUIT;
    }
}

static void dispatchFloorMenuAction(Game *game,
                                    char actionCharacter,
                                    int currentMinBet,
                                    int nextMinBet) {
    int inputCharacter;

    if ((actionCharacter == 'c' || actionCharacter == 'C') &&
        game->player.chips >= currentMinBet) {
        game->state = STATE_BET;
    } else if ((actionCharacter == 'a' || actionCharacter == 'A') &&
               game->player.chips >= nextMinBet) {
        advanceFloor(game);
        renderMessage("Press any key to continue.",
                      ROW_PROMPT,
                      MSG_DEFAULT);

        if (inputIsRaw()) {
            inputCharacter = getch();
        } else {
            inputCharacter = getchar();

            if (inputCharacter != EOF) {
                while ((inputCharacter = getchar()) != '\n' &&
                       inputCharacter != EOF) {
                }
            }
        }

        if (inputCharacter == -1 || inputCharacter == EOF) {
            game->state = STATE_QUIT;
        } else {
            game->state = STATE_BET;
        }
    } else if (actionCharacter == 's' || actionCharacter == 'S') {
        game->state = STATE_SHOP;
    } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
        game->state = STATE_QUIT;
    }
}

// Bet input stays line-based so fgets can handle multi-character numeric input and backspace.
// Returns -1 on EOF so quitting is distinct from a valid bet.
static int readBet(Game *game) {
    char input[64];
    char prompt[64];
    char message[64];
    char *endPointer;
    long betValue;
    size_t delimiterPosition;
    int minBet;
    int validBet;

    minBet = getMinBet(game->run.floor);

    if (game->player.chips < minBet) {
        snprintf(prompt,
                 sizeof(prompt),
                 "Minimum bet is %d. You have %d chips. Run over.",
                 minBet,
                 game->player.chips);
        renderMessage(prompt, ROW_RESULT, MSG_DEFAULT);
        game->state = STATE_QUIT;
        return -1;
    }

    snprintf(prompt,
             sizeof(prompt),
             "Enter bet (%d-%d): ",
             minBet,
             game->player.chips);

    validBet = 0;

    while (!validBet) {
        renderMessage(prompt, ROW_PROMPT, MSG_DEFAULT);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            if (ferror(stdin) && errno == EINTR) {
                clearerr(stdin);
                continue;
            }

            return -1;
        }

        delimiterPosition = strcspn(input, "\r\n");

        if (delimiterPosition == sizeof(input) - 1) {
            int ch;

            while ((ch = getchar()) != '\n' && ch != EOF) {
            }

            snprintf(message,
                     sizeof(message),
                     "Invalid bet. Enter an integer between %d and %d.",
                     minBet,
                     game->player.chips);
            renderMessage(message, ROW_RESULT, MSG_DEFAULT);
            continue;
        }

        if (input[delimiterPosition] != '\0') {
            input[delimiterPosition] = '\0';
        }

        betValue = strtol(input, &endPointer, 10);

        while (isspace((unsigned char)*endPointer)) {
            endPointer++;
        }

        if (input[0] != '\0' &&
            *endPointer == '\0' &&
            betValue >= minBet &&
            betValue <= game->player.chips) {
            validBet = 1;
        } else {
            snprintf(message,
                     sizeof(message),
                     "Invalid bet. Enter an integer between %d and %d.",
                     minBet,
                     game->player.chips);
            renderMessage(message, ROW_RESULT, MSG_DEFAULT);
        }
    }

    return (int)betValue;
}

void initGame(Game *game) {
    initDeck(&game->deck);
    shuffleDeck(&game->deck);
    resetHand(&game->player.hand);
    resetHand(&game->dealer.hand);
    game->player.chips = 100;
    game->player.bet = 0;
    game->state = STATE_BET;
    game->roundNumber = 0;
    initRun(&game->run);
    game->peekActive = 0;
    memset(game->player.perkOwned, 0, sizeof(game->player.perkOwned));
    memset(game->player.perkCharges, 0, sizeof(game->player.perkCharges));
}


int calculateDealerHandValue(const Hand *hand, DealerId id) {
    int totalValue;
    int cardIndex;

    if (id != DEALER_ACCOUNTANT) {
        return calculateHandValue(hand);
    }

    totalValue = 0;

    for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
        if (hand->cards[cardIndex].rank == ACE) {
            totalValue += 1;
        } else {
            totalValue += getCardValue(&hand->cards[cardIndex]);
        }
    }

    return totalValue;
}

int calculatePlayerHandValue(const Game *game) {
    int totalValue;
    int cardIndex;

    if (!game->player.perkOwned[PERK_ACE_HIGH]) {
        return game->player.hand.value;
    }

    totalValue = 0;

    for (cardIndex = 0; cardIndex < game->player.hand.count; cardIndex++) {
        totalValue += getCardValue(&game->player.hand.cards[cardIndex]);
    }

    return totalValue;
}

void startRound(Game *game) {
    int rawMode;
    int playerValue;

    game->peekActive = 0;

    if (game->run.twistId == TWIST_SIX_DECK) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    } else if (game->run.twistId != TWIST_NO_RESHUFFLE &&
               game->deck.top > DECK_SIZE - DECK_RESHUFFLE_MARGIN) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    }

    resetHand(&game->player.hand);
    resetHand(&game->dealer.hand);
    renderTable(game, 0);

    rawMode = inputIsRaw();

    if (rawMode) {
        disableRawMode();
    }

    game->player.bet = readBet(game);

    if (rawMode && game->state != STATE_QUIT) {
        inputInit();
    }

    if (game->player.bet < 0 || game->state == STATE_QUIT) {
        game->state = STATE_QUIT;
    } else {
        game->roundNumber++;

        drawCard(&game->deck, &game->player.hand);
        drawCard(&game->deck, &game->dealer.hand);
        drawCard(&game->deck, &game->player.hand);
        drawCard(&game->deck, &game->dealer.hand);

        if (game->run.twistId == TWIST_LUCKY_DRAW) {
            redrawFirstPlayerCard(game, 1);
        } else if (game->run.twistId == TWIST_COLD_DECK) {
            redrawFirstPlayerCard(game, 0);
        }

        playerValue = calculatePlayerHandValue(game);

        if (playerValue >= 21) {
            game->state = STATE_RESULT;
        } else {
            game->state = STATE_PLAYER_TURN;
        }
    }
}

void playerTurn(Game *game) {
    ActionId actions[8];
    KeyEvent event;
    int actionCount;
    int actionIndex;
    int matchedIndex;
    int inputCharacter;

    actionIndex = 0;

    if (inputIsRaw()) {
        while (game->state == STATE_PLAYER_TURN) {
            actionCount = buildPlayerActions(game, actions);

            if (actionIndex >= actionCount) {
                actionIndex = actionCount - 1;
            }

            renderTable(game, 1);
            renderActionBar(actions,
                            actionCount,
                            actionIndex,
                            ROW_PROMPT);

            event = readKey();

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
            } else if (event.type == KEY_UP ||
                       event.type == KEY_DOWN) {
            } else if (event.type == KEY_CHAR) {
                matchedIndex = findPlayerActionIndex(actions,
                                                     actionCount,
                                                     event.ch);

                if (matchedIndex >= 0) {
                    actionIndex = matchedIndex;
                }

                if (event.ch == 'h' || event.ch == 'H') {
                    dispatchPlayerAction(game, ACTION_HIT);
                } else if (event.ch == 's' || event.ch == 'S') {
                    dispatchPlayerAction(game, ACTION_STAND);
                } else if (event.ch == 'd' || event.ch == 'D') {
                    dispatchPlayerAction(game, ACTION_DOUBLE);
                } else if (event.ch == 'p' || event.ch == 'P') {
                    dispatchPlayerAction(game, ACTION_PEEK);
                } else if (event.ch == 'q' || event.ch == 'Q') {
                    dispatchPlayerAction(game, ACTION_QUIT);
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                game->state = STATE_QUIT;
            }
        }
    } else {
        while (game->state == STATE_PLAYER_TURN) {
            actionCount = buildPlayerActions(game, actions);

            if (actionIndex >= actionCount) {
                actionIndex = actionCount - 1;
            }

            renderTable(game, 1);
            renderMessage("Hit or stand? [h/s]: ", ROW_PROMPT, MSG_DEFAULT);
            inputCharacter = getchar();

            if (inputCharacter == EOF) {
                game->state = STATE_QUIT;
            } else {
                event = makeKeyEvent(inputCharacter);

                while ((inputCharacter = getchar()) != '\n' &&
                       inputCharacter != EOF) {
                }

                if (event.type == KEY_CHAR) {
                    matchedIndex = findPlayerActionIndex(actions,
                                                         actionCount,
                                                         event.ch);

                    if (matchedIndex >= 0) {
                        actionIndex = matchedIndex;
                    }

                    if (event.ch == 'h' || event.ch == 'H') {
                        dispatchPlayerAction(game, ACTION_HIT);
                    } else if (event.ch == 's' || event.ch == 'S') {
                        dispatchPlayerAction(game, ACTION_STAND);
                    } else if (event.ch == 'd' || event.ch == 'D') {
                        dispatchPlayerAction(game, ACTION_DOUBLE);
                    } else if (event.ch == 'p' || event.ch == 'P') {
                        dispatchPlayerAction(game, ACTION_PEEK);
                    } else if (event.ch == 'q' || event.ch == 'Q') {
                        dispatchPlayerAction(game, ACTION_QUIT);
                    } else {
                        renderMessage("Invalid action. Enter h, s, d, p, or q.",
                                      ROW_PROMPT,
                                      MSG_DEFAULT);
                    }
                } else {
                    renderMessage("Invalid action. Enter h, s, d, p, or q.",
                                  ROW_PROMPT,
                                  MSG_DEFAULT);
                }
            }
        }
    }
}

void dealerTurn(Game *game) {
    int dealerValue;
    int targetValue;

    if (game->state == STATE_DEALER_TURN) {
        targetValue = game->run.dealerId == DEALER_CLOCKMAKER ? 18 : 17;
        dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                               game->run.dealerId);

        while (dealerValue < targetValue) {
            drawCard(&game->deck, &game->dealer.hand);
            dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                                   game->run.dealerId);
        }

        game->state = STATE_RESULT;
    }
}

void resolveRound(Game *game) {
    int playerValue;
    int dealerValue;
    int playerBlackjack;
    int dealerBlackjack;
    int payout;
    int lossAmount;
    long long payoutValue;
    long long baseLoss;
    MessageStyle resultStyle;
    char resultMessage[128];

    playerValue = calculatePlayerHandValue(game);
    dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                           game->run.dealerId);
    playerBlackjack = game->player.hand.count == 2 && playerValue == 21;
    dealerBlackjack = game->dealer.hand.count == 2 && dealerValue == 21;
    payout = 0;
    lossAmount = game->player.bet;
    payoutValue = 0;
    baseLoss = game->player.bet;

    if (game->run.dealerId == DEALER_BUTCHER) {
        baseLoss = ((long long)game->player.bet * 150) / 100;
    }

    if (game->run.twistId == TWIST_CHARITY) {
        baseLoss /= 2;
    }

    if (playerValue > 21 &&
        game->player.perkOwned[PERK_SECOND_WIND] &&
        game->player.perkCharges[PERK_SECOND_WIND] > 0) {
        game->player.perkCharges[PERK_SECOND_WIND]--;
        resetPlayerHandToEleven(game);
        game->state = STATE_PLAYER_TURN;
        renderTable(game, 1);
        renderMessage("Second Wind! Hand reset to 11.",
                      ROW_RESULT,
                      MSG_INFO);
        return;
    }

    if (playerValue > 21) {
        lossAmount = applyLoss(game, baseLoss);
        resultStyle = MSG_LOSS;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Player busts. You lose %d chips.",
                 lossAmount);
    } else if (playerBlackjack && !dealerBlackjack) {
        if (game->run.dealerId == DEALER_VOID) {
            payoutValue = game->player.bet;
        } else {
            payoutValue = ((long long)game->player.bet * 3) / 2;
        }

        if (payoutValue > INT_MAX - game->player.chips) {
            payout = INT_MAX - game->player.chips;
        } else {
            payout = (int)payoutValue;
        }

        game->player.chips += payout;
        resultStyle = MSG_WIN;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Blackjack! You win %d chips.",
                 payout);
    } else if (dealerBlackjack && !playerBlackjack) {
        lossAmount = applyLoss(game, baseLoss);
        resultStyle = MSG_LOSS;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Dealer has blackjack. You lose %d chips.",
                 lossAmount);
    } else if (playerValue == dealerValue) {
        resultStyle = MSG_PUSH;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "Push. Your bet is returned.");
    } else if (dealerValue > 21 || playerValue > dealerValue) {
        if (game->run.twistId == TWIST_HALF_PAYOUT) {
            payoutValue = game->player.bet / 2;
        } else {
            payoutValue = game->player.bet;
        }

        if (payoutValue > INT_MAX - game->player.chips) {
            payout = INT_MAX - game->player.chips;
        } else {
            payout = (int)payoutValue;
        }

        game->player.chips += payout;
        resultStyle = MSG_WIN;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "You win %d chips.",
                 payout);
    } else {
        lossAmount = applyLoss(game, baseLoss);
        resultStyle = MSG_LOSS;
        snprintf(resultMessage,
                 sizeof(resultMessage),
                 "You lose %d chips.",
                 lossAmount);
    }

    renderTable(game, 0);
    renderMessage(resultMessage, ROW_RESULT, resultStyle);
    snprintf(resultMessage,
             sizeof(resultMessage),
             "Chips: %d",
             game->player.chips);
    renderMessage(resultMessage, ROW_CHIPS, MSG_DEFAULT);

    if (game->player.chips == 0) {
        game->state = STATE_QUIT;
    } else {
        game->state = STATE_MENU;
    }
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
    game->run.souls += earned;
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

    resetHand(&game->player.hand);
    resetHand(&game->dealer.hand);

    snprintf(message,
             sizeof(message),
             "Floor %d. Souls: %d. Minimum bet: %d.",
             game->run.floor,
             game->run.souls,
             getMinBet(game->run.floor));
    renderMessage(message, ROW_RESULT, MSG_DEFAULT);
}

void floorMenu(Game *game) {
    const char *menuItems[4];
    char continueItem[128];
    char advanceItem[128];
    char shopItem[128];
    char input[64];
    KeyEvent event;
    int nextMinBet;
    int disabledMask;
    int selectedIndex;
    int actionCharacter;
    int characterIndex;
    int hasCharacter;
    int currentMinBet;
    int inputCharacter;
    int rawMode;

    currentMinBet = getMinBet(game->run.floor);

    if (game->player.chips < currentMinBet) {
        renderMessage("You cannot afford the minimum bet. Run over.",
                      ROW_RESULT,
                      MSG_LOSS);
        game->state = STATE_QUIT;
        return;
    }

    nextMinBet = getMinBet(game->run.floor + 1);
    disabledMask = 0;

    if (game->player.chips < nextMinBet) {
        disabledMask |= 1 << 1;
    }

    snprintf(continueItem,
             sizeof(continueItem),
             "[C]ontinue playing on floor %d",
             game->run.floor);
    snprintf(advanceItem,
             sizeof(advanceItem),
             "[A]dvance to floor %d (earn souls)",
             game->run.floor + 1);
    snprintf(shopItem,
             sizeof(shopItem),
             "[S]hop (%d souls)",
             game->run.souls);

    menuItems[0] = continueItem;
    menuItems[1] = advanceItem;
    menuItems[2] = shopItem;
    menuItems[3] = "[Q]uit run";

    selectedIndex = 0;
    rawMode = inputIsRaw();

    if (!rawMode) {
        renderMenu("What's next?",
                   menuItems,
                   4,
                   disabledMask,
                   selectedIndex,
                   ROW_MENU_START);
    }

    while (game->state == STATE_MENU) {
        if (rawMode) {
            renderMenu("What's next?",
                       menuItems,
                       4,
                       disabledMask,
                       selectedIndex,
                       ROW_MENU_START);

            event = readKey();

            if (event.type == KEY_UP) {
                selectedIndex = moveSelection(selectedIndex,
                                              4,
                                              disabledMask,
                                              -1);
            } else if (event.type == KEY_DOWN) {
                selectedIndex = moveSelection(selectedIndex,
                                              4,
                                              disabledMask,
                                              1);
            } else if (event.type == KEY_LEFT ||
                       event.type == KEY_RIGHT) {
            } else if (event.type == KEY_ENTER) {
                actionCharacter = "casq"[selectedIndex];
                dispatchFloorMenuAction(game,
                                        (char)actionCharacter,
                                        currentMinBet,
                                        nextMinBet);
            } else if (event.type == KEY_CHAR) {
                actionCharacter = event.ch;

                if (actionCharacter == 'c' || actionCharacter == 'C') {
                    selectedIndex = 0;
                } else if (actionCharacter == 'a' || actionCharacter == 'A') {
                    selectedIndex = 1;
                } else if (actionCharacter == 's' || actionCharacter == 'S') {
                    selectedIndex = 2;
                } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
                    selectedIndex = 3;
                }

                dispatchFloorMenuAction(game,
                                        (char)actionCharacter,
                                        currentMinBet,
                                        nextMinBet);
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                game->state = STATE_QUIT;
            }
        } else {
            actionCharacter = -1;

            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (strchr(input, '\n') == NULL) {
                    while ((inputCharacter = getchar()) != '\n' &&
                           inputCharacter != EOF) {
                    }
                }

                characterIndex = 0;
                hasCharacter = 0;

                while (input[characterIndex] != '\0') {
                    if (!isspace((unsigned char)input[characterIndex])) {
                        actionCharacter = input[characterIndex];
                        hasCharacter = 1;
                        break;
                    }

                    characterIndex++;
                }

                if (!hasCharacter) {
                    actionCharacter = -1;
                }
            }

            if (actionCharacter == -1) {
                game->state = STATE_QUIT;
            } else {
                dispatchFloorMenuAction(game,
                                        (char)actionCharacter,
                                        currentMinBet,
                                        nextMinBet);
            }
        }
    }
}

void printGameState(const Game *game, int hideDealerCard) {
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    int dealerValue;
    int dealerCardHidden;
    int cardIndex;

    dealerInfo = getDealerInfo(game->run.dealerId);
    twistInfo = getTwistInfo(game->run.twistId);
    dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                           game->run.dealerId);
    dealerCardHidden = hideDealerCard &&
                       !game->peekActive &&
                       game->run.twistId != TWIST_SNIPER;

    printf("Round: %d\n", game->roundNumber);
    printf("Dealer (%s): ", dealerInfo->name);

    for (cardIndex = 0; cardIndex < game->dealer.hand.count; cardIndex++) {
        if (dealerCardHidden && cardIndex == 0) {
            printf("??");
        } else {
            printCard(&game->dealer.hand.cards[cardIndex]);
        }

        if (cardIndex < game->dealer.hand.count - 1) {
            printf(" ");
        }
    }

    if (dealerCardHidden || game->run.dealerId == DEALER_REAPER) {
        printf(" | Total value: ??\n");
    } else {
        printf(" | Total value: %d\n", dealerValue);
    }

    if (dealerInfo->rule1 != NULL) {
        printf("Rule: %s\n", dealerInfo->rule1);
    }

    if (twistInfo->id != TWIST_NONE) {
        printf("Twist: %s — %s\n",
               twistInfo->name,
               twistInfo->description);
    }

    printf("Player: ");

    for (cardIndex = 0; cardIndex < game->player.hand.count; cardIndex++) {
        if (cardIndex > 0) {
            printf(" ");
        }

        printCard(&game->player.hand.cards[cardIndex]);
    }

    printf(" | Total value: %d\n",
           calculatePlayerHandValue(game));
    printf("Chips: %d | Bet: %d\n",
           game->player.chips,
           game->player.bet);
}

void runShop(Game *game) {
    char input[64];
    KeyEvent event;
    int actionCharacter;
    int inputCharacter;
    int perkIndex;
    int selectedIndex;
    int ownedMask;
    int rawMode;
    int characterIndex;

    const char *pendingMessage = NULL;

    selectedIndex = 0;
    rawMode = inputIsRaw();

    while (game->state == STATE_SHOP) {
        ownedMask = 0;

        for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
            if (game->player.perkOwned[perkIndex]) {
                ownedMask |= 1 << perkIndex;
            }
        }

        renderShop(game, selectedIndex);

        if (pendingMessage != NULL) {
            renderMessage(pendingMessage, ROW_RESULT, MSG_DEFAULT);
        }

        if (rawMode) {
            event = readKey();

            if (event.type == KEY_UP) {
                selectedIndex = moveSelection(selectedIndex,
                                              PERK_COUNT,
                                              ownedMask,
                                              -1);
                pendingMessage = NULL;
            } else if (event.type == KEY_DOWN) {
                selectedIndex = moveSelection(selectedIndex,
                                              PERK_COUNT,
                                              ownedMask,
                                              1);
                pendingMessage = NULL;
            } else if (event.type == KEY_LEFT ||
                       event.type == KEY_RIGHT) {
                pendingMessage = NULL;
            } else if (event.type == KEY_ENTER) {
                if (allPerksOwned(game)) {
                    pendingMessage = NULL;
                } else if (game->player.perkOwned[selectedIndex]) {
                    pendingMessage = "Already owned.";
                } else if (game->run.souls < perkCost((PerkId)selectedIndex)) {
                    pendingMessage = "Not enough souls.";
                } else {
                    game->run.souls -= perkCost((PerkId)selectedIndex);
                    game->player.perkOwned[selectedIndex] = 1;

                    if (selectedIndex == PERK_PEEK ||
                        selectedIndex == PERK_SECOND_WIND) {
                        game->player.perkCharges[selectedIndex] = 1;
                    }

                    pendingMessage = NULL;
                }
            } else if (event.type == KEY_CHAR) {
                actionCharacter = event.ch;

                if (actionCharacter >= '1' &&
                    actionCharacter <= '5') {
                    selectedIndex = actionCharacter - '1';

                    if (game->player.perkOwned[selectedIndex]) {
                        pendingMessage = "Already owned.";
                    } else if (game->run.souls <
                               perkCost((PerkId)selectedIndex)) {
                        pendingMessage = "Not enough souls.";
                    } else {
                        game->run.souls -=
                            perkCost((PerkId)selectedIndex);
                        game->player.perkOwned[selectedIndex] = 1;

                        if (selectedIndex == PERK_PEEK ||
                            selectedIndex == PERK_SECOND_WIND) {
                            game->player.perkCharges[selectedIndex] = 1;
                        }

                        pendingMessage = NULL;
                    }
                } else if (actionCharacter == 'q' ||
                           actionCharacter == 'Q') {
                    pendingMessage = NULL;
                    game->state = STATE_MENU;
                } else {
                    pendingMessage = NULL;
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                pendingMessage = NULL;
                game->state = STATE_MENU;
            }
        } else {
            actionCharacter = -1;

            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (strchr(input, '\n') == NULL) {
                    while ((inputCharacter = getchar()) != '\n' &&
                           inputCharacter != EOF) {
                    }
                }

                characterIndex = 0;

                while (input[characterIndex] != '\0') {
                    if (!isspace((unsigned char)input[characterIndex])) {
                        actionCharacter = input[characterIndex];
                        break;
                    }

                    characterIndex++;
                }
            }

            if (actionCharacter == -1) {
                pendingMessage = NULL;
                game->state = STATE_MENU;
            } else if (actionCharacter == 'q' ||
                       actionCharacter == 'Q' ||
                       actionCharacter == 27) {
                pendingMessage = NULL;
                game->state = STATE_MENU;
            } else if (actionCharacter >= '1' &&
                       actionCharacter <= '5') {
                selectedIndex = actionCharacter - '1';

                if (game->player.perkOwned[selectedIndex]) {
                    pendingMessage = "Already owned.";
                } else if (game->run.souls <
                           perkCost((PerkId)selectedIndex)) {
                    pendingMessage = "Not enough souls.";
                } else {
                    game->run.souls -= perkCost((PerkId)selectedIndex);
                    game->player.perkOwned[selectedIndex] = 1;

                    if (selectedIndex == PERK_PEEK ||
                        selectedIndex == PERK_SECOND_WIND) {
                        game->player.perkCharges[selectedIndex] = 1;
                    }

                    pendingMessage = NULL;
                }
            } else {
                pendingMessage = NULL;
            }
        }
    }
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

