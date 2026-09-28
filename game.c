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
        remainingChips = chipsBeforeLoss / 2;

        if (remainingChips < 1) {
            remainingChips = 1;
        }

        game->player.chips = remainingChips;
        lossAmount = chipsBeforeLoss - remainingChips;
    } else {
        game->player.chips -= lossAmount;
    }

    return lossAmount;
}

// Resets the player to a visible ace with value 11.
static void resetPlayerHandToEleven(Game *game) {
    Card recoveryCard;
    int attemptCount;
    int foundAce;

    resetHand(&game->player.hand);
    attemptCount = 0;
    foundAce = 0;

    while (attemptCount < DECK_SIZE * 2 && !foundAce) {
        if (game->deck.top >= DECK_SIZE) {
            initDeck(&game->deck);
            shuffleDeck(&game->deck);
        }

        recoveryCard = game->deck.cards[game->deck.top];
        game->deck.top++;

        if (recoveryCard.rank == ACE) {
            addCardToHand(&game->player.hand, &recoveryCard);
            foundAce = 1;
        }

        attemptCount++;
    }

    if (!foundAce) {
        recoveryCard.suit = HEARTS;
        recoveryCard.rank = ACE;
        addCardToHand(&game->player.hand, &recoveryCard);
    }

    game->player.hand.value = 11;
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

    minBet = getMinBet(game->floor);

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
    game->floor = 1;
    game->souls = 0;
    game->dealerId = selectDealerForFloor(1, DEALER_CROUPIER);
    game->previousDealerId = game->dealerId;
    game->previousTwistId = TWIST_NONE;
    game->peekActive = 0;
    memset(game->player.perkOwned, 0, sizeof(game->player.perkOwned));
    memset(game->player.perkCharges, 0, sizeof(game->player.perkCharges));
    game->twistId = TWIST_NONE;
}

int getMinBet(int floor) {
    if (floor <= 1) {
        return 1;
    }

    if (floor <= 3) {
        return 5;
    }

    if (floor <= 6) {
        return 10;
    }

    if (floor <= 9) {
        return 25;
    }

    return 50;
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

    if (game->twistId == TWIST_SIX_DECK) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    } else if (game->twistId != TWIST_NO_RESHUFFLE &&
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

        if (game->twistId == TWIST_LUCKY_DRAW) {
            redrawFirstPlayerCard(game, 1);
        } else if (game->twistId == TWIST_COLD_DECK) {
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
    int inputCharacter;
    int actionCharacter;

    if (inputIsRaw()) {
        renderTable(game, 1);

        while (game->state == STATE_PLAYER_TURN) {
            renderMessage("Hit or stand? [h/s]: ", ROW_PROMPT, MSG_DEFAULT);
            actionCharacter = getch();

            if (actionCharacter == -1) {
                game->state = STATE_QUIT;
                break;
            }

            if (actionCharacter == 'h' || actionCharacter == 'H') {
                drawCard(&game->deck, &game->player.hand);

                if (game->dealerId == DEALER_MIRROR) {
                    drawCard(&game->deck, &game->dealer.hand);
                }

                if (calculatePlayerHandValue(game) > 21) {
                    game->state = STATE_RESULT;
                }

                renderTable(game, game->state == STATE_PLAYER_TURN);
            } else if (actionCharacter == 's' || actionCharacter == 'S') {
                if (game->twistId == TWIST_STICKY_HANDS &&
                    calculatePlayerHandValue(game) < 15) {
                    renderMessage("Sticky Hands: cannot stand below 15.",
                                  ROW_RESULT,
                                  MSG_DEFAULT);
                } else {
                    game->state = STATE_DEALER_TURN;
                    renderTable(game, 0);
                }
            } else if (actionCharacter == 'p' || actionCharacter == 'P') {
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
            } else if (actionCharacter == 'q' || actionCharacter == 'Q') {
                game->state = STATE_QUIT;
            }
        }
    } else {
        while (game->state == STATE_PLAYER_TURN) {
            renderTable(game, 1);
            renderMessage("Hit or stand? [h/s]: ", ROW_PROMPT, MSG_DEFAULT);
            actionCharacter = getchar();

            if (actionCharacter == EOF) {
                game->state = STATE_QUIT;
                break;
            }

            while ((inputCharacter = getchar()) != '\n' &&
                   inputCharacter != EOF) {
            }

            if (actionCharacter == 'h' || actionCharacter == 'H') {
                drawCard(&game->deck, &game->player.hand);

                if (game->dealerId == DEALER_MIRROR) {
                    drawCard(&game->deck, &game->dealer.hand);
                }

                if (calculatePlayerHandValue(game) > 21) {
                    game->state = STATE_RESULT;
                }
            } else if (actionCharacter == 's' || actionCharacter == 'S') {
                if (game->twistId == TWIST_STICKY_HANDS &&
                    calculatePlayerHandValue(game) < 15) {
                    renderMessage("Sticky Hands: cannot stand below 15.",
                                  ROW_RESULT,
                                  MSG_DEFAULT);
                } else {
                    game->state = STATE_DEALER_TURN;
                }
            } else if (actionCharacter == 'p' || actionCharacter == 'P') {
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
            } else {
                renderMessage("Invalid action. Enter h to hit, s to stand, or p to peek.",
                              ROW_PROMPT,
                              MSG_DEFAULT);
            }
        }
    }
}

void dealerTurn(Game *game) {
    int dealerValue;
    int targetValue;

    if (game->state == STATE_DEALER_TURN) {
        targetValue = game->dealerId == DEALER_CLOCKMAKER ? 18 : 17;
        dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                               game->dealerId);

        while (dealerValue < targetValue) {
            drawCard(&game->deck, &game->dealer.hand);
            dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                                   game->dealerId);
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
                                           game->dealerId);
    playerBlackjack = game->player.hand.count == 2 && playerValue == 21;
    dealerBlackjack = game->dealer.hand.count == 2 && dealerValue == 21;
    payout = 0;
    lossAmount = game->player.bet;
    payoutValue = 0;
    baseLoss = game->player.bet;

    if (game->dealerId == DEALER_BUTCHER) {
        baseLoss = ((long long)game->player.bet * 150) / 100;
    }

    if (game->twistId == TWIST_CHARITY) {
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
        if (game->dealerId == DEALER_VOID) {
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
        if (game->twistId == TWIST_HALF_PAYOUT) {
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

    earned = 10 + game->floor * 3 + bonus;
    game->souls += earned;
    game->previousDealerId = game->dealerId;
    game->previousTwistId = game->twistId;
    game->floor++;
    game->dealerId = selectDealerForFloor(game->floor,
                                          game->previousDealerId);

    if (game->player.perkOwned[PERK_PEEK]) {
        game->player.perkCharges[PERK_PEEK] = 1;
    }

    {
        int attempts;

        attempts = 0;

        do {
            game->twistId = selectTwistForFloor(game->floor,
                                                game->previousTwistId);
            attempts++;
        } while (game->player.perkOwned[PERK_SIXTH_SENSE] &&
                 game->floor >= 4 &&
                 getTwistInfo(game->twistId)->category == TWIST_CAT_HOSTILE &&
                 attempts < 10);
    }

    resetHand(&game->player.hand);
    resetHand(&game->dealer.hand);

    snprintf(message,
             sizeof(message),
             "Floor %d. Souls: %d. Minimum bet: %d.",
             game->floor,
             game->souls,
             getMinBet(game->floor));
    renderMessage(message, ROW_RESULT, MSG_DEFAULT);
}

void floorMenu(Game *game) {
    const char *menuItems[4];
    char continueItem[128];
    char advanceItem[128];
    char shopItem[128];
    char input[64];
    int nextMinBet;
    int disabledMask;
    int actionCharacter;
    int characterIndex;
    int hasCharacter;
    int currentMinBet;
    int inputCharacter;

    nextMinBet = getMinBet(game->floor + 1);
    currentMinBet = getMinBet(game->floor);
    disabledMask = 0;

    if (game->player.chips < currentMinBet) {
        disabledMask |= 1;
    }

    if (game->player.chips < nextMinBet) {
        disabledMask |= 1 << 1;
    }

    snprintf(continueItem,
             sizeof(continueItem),
             "[C]ontinue playing on floor %d",
             game->floor);
    snprintf(advanceItem,
             sizeof(advanceItem),
             "[A]dvance to floor %d (earn souls)",
             game->floor + 1);
    snprintf(shopItem,
             sizeof(shopItem),
             "[S]hop (%d souls)",
             game->souls);

    menuItems[0] = continueItem;
    menuItems[1] = advanceItem;
    menuItems[2] = shopItem;
    menuItems[3] = "[Q]uit run";

    renderMenu("What's next?", menuItems, 4, disabledMask, ROW_MENU_START);

    if (game->player.chips < currentMinBet) {
        snprintf(continueItem,
                 sizeof(continueItem),
                 "Cannot afford minimum bet on floor %d.",
                 game->floor);
        renderMessage(continueItem, ROW_RESULT, MSG_DEFAULT);
    }

    while (game->state == STATE_MENU) {
        actionCharacter = -1;

        if (inputIsRaw()) {
            actionCharacter = getch();
        } else if (fgets(input, sizeof(input), stdin) != NULL) {
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
        } else if ((actionCharacter == 'c' || actionCharacter == 'C') &&
                   game->player.chips >= currentMinBet) {
            game->state = STATE_BET;
        } else if ((actionCharacter == 'a' || actionCharacter == 'A') &&
                   game->player.chips >= nextMinBet) {
            advanceFloor(game);
            renderMessage("Press any key to continue.",
                          ROW_PROMPT,
                          MSG_DEFAULT);

            if (inputIsRaw()) {
                actionCharacter = getch();
            } else {
                actionCharacter = getchar();

                if (actionCharacter != EOF) {
                    while ((inputCharacter = getchar()) != '\n' &&
                           inputCharacter != EOF) {
                    }
                }
            }

            if (actionCharacter == -1 || actionCharacter == EOF) {
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
}

void printGameState(const Game *game, int hideDealerCard) {
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    int dealerValue;
    int dealerCardHidden;
    int cardIndex;

    dealerInfo = getDealerInfo(game->dealerId);
    twistInfo = getTwistInfo(game->twistId);
    dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                           game->dealerId);
    dealerCardHidden = hideDealerCard &&
                       !game->peekActive &&
                       game->twistId != TWIST_SNIPER;

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

    if (dealerCardHidden || game->dealerId == DEALER_REAPER) {
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
    int actionCharacter;
    int inputCharacter;
    int perkIndex;
    const char *message;

    while (game->state == STATE_SHOP) {
        message = NULL;
        renderShop(game);

        if (inputIsRaw()) {
            actionCharacter = getch();
        } else if (fgets(input, sizeof(input), stdin) != NULL) {
            if (strchr(input, '\n') == NULL) {
                while ((inputCharacter = getchar()) != '\n' &&
                       inputCharacter != EOF) {
                }
            }

            actionCharacter = -1;
            inputCharacter = 0;

            while (input[inputCharacter] != '\0') {
                if (!isspace((unsigned char)input[inputCharacter])) {
                    actionCharacter = input[inputCharacter];
                    break;
                }

                inputCharacter++;
            }
        } else {
            actionCharacter = -1;
        }

        if (actionCharacter == -1) {
            game->state = STATE_MENU;
        } else if (actionCharacter == 'q' ||
                   actionCharacter == 'Q' ||
                   actionCharacter == 27) {
            game->state = STATE_MENU;
        } else if (actionCharacter >= '1' &&
                   actionCharacter <= '5') {
            perkIndex = actionCharacter - '1';

            if (game->player.perkOwned[perkIndex]) {
                message = "Already owned.";
            } else if (game->souls < perkCost((PerkId)perkIndex)) {
                message = "Not enough souls.";
            } else {
                game->souls -= perkCost((PerkId)perkIndex);
                game->player.perkOwned[perkIndex] = 1;

                if (perkIndex == PERK_PEEK ||
                    perkIndex == PERK_SECOND_WIND) {
                    game->player.perkCharges[perkIndex] = 1;
                }
            }

            renderShop(game);

            if (message != NULL) {
                renderMessage(message, ROW_RESULT, MSG_DEFAULT);
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

