#include "game.h"
#include "round.h"
#include "render.h"
#include "input.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Keeps a safety margin before reshuffling at the start of a round.
#define DECK_RESHUFFLE_MARGIN 15
#define FIRST_CARD_RETRY_LIMIT 50

// Draws the next card and advances the deck position.

// Replaces the player's first card while keeping the other dealt cards intact.

// Applies a loss and activates Survival when the loss would end the run.

// Resets the player to a synthetic ace with value 11.

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

// Reads and validates the player's bet.
static int readBet(Game *game) {
    char input[64];
    char prompt[64];
    char message[64];
    char rawInput[9];
    char *endPointer;
    long betValue;
    size_t delimiterPosition;
    size_t rawLength;
    int minBet;
    int validBet;
    KeyEvent event;

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

    if (inputIsRaw()) {
        rawLength = 0;
        rawInput[0] = '\0';

        while (1) {
            snprintf(prompt,
                     sizeof(prompt),
                     "Enter bet (%d-%d): %s",
                     minBet,
                     game->player.chips,
                     rawInput);
            renderMessage(prompt, ROW_PROMPT, MSG_DEFAULT);

            event = readKey();

            if (event.type == KEY_CHAR) {
                if (event.ch >= '0' &&
                    event.ch <= '9') {
                    if (rawLength < 8) {
                        rawInput[rawLength] = event.ch;
                        rawLength++;
                        rawInput[rawLength] = '\0';
                    }
                } else if (event.ch == 0x7F || event.ch == 0x08) {
                    if (rawLength > 0) {
                        rawLength--;
                        rawInput[rawLength] = '\0';
                    }
                }
            } else if (event.type == KEY_ENTER) {
                if (rawLength == 0) {
                    renderMessage("Enter a bet.",
                                  ROW_RESULT,
                                  MSG_DEFAULT);
                } else {
                    betValue = strtol(rawInput, &endPointer, 10);

                    if (*endPointer == '\0' &&
                        betValue >= minBet &&
                        betValue <= game->player.chips) {
                        validBet = 1;
                    } else {
                        snprintf(message,
                                 sizeof(message),
                                 "Invalid bet. Enter an integer between %d and %d.",
                                 minBet,
                                 game->player.chips);
                        renderMessage(message,
                                      ROW_RESULT,
                                      MSG_DEFAULT);
                        validBet = 0;
                    }

                    if (validBet) {
                        return (int)betValue;
                    }
                }
            } else if (event.type == KEY_ESC ||
                       event.type == KEY_EOF) {
                return -1;
            }
        }
    } else {
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
}

void initGame(Game *game) {
    initDeck(&game->deck);
    shuffleDeck(&game->deck);
    memset(game->player.hands, 0, sizeof(game->player.hands));
    memset(game->player.bets, 0, sizeof(game->player.bets));
    memset(game->player.isSplitAce, 0, sizeof(game->player.isSplitAce));
    game->player.handCount = 1;
    game->player.activeHand = 0;
    resetHand(&game->dealer.hand);
    game->player.chips = 100;
    game->state = STATE_BET;
    game->roundNumber = 0;
    initRun(&game->run);
    game->peekActive = 0;
    memset(game->player.perkOwned, 0, sizeof(game->player.perkOwned));
    memset(game->player.perkCharges, 0, sizeof(game->player.perkCharges));
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

// Appends one hand result without overflowing the result buffer.

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
    resetHand(&game->dealer.hand);

    snprintf(message,
             sizeof(message),
             "Floor %d. Marks: %d. Minimum bet: %d.",
             game->run.floor,
             game->run.marks,
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
             "[A]dvance to floor %d (earn marks)",
             game->run.floor + 1);
    snprintf(shopItem,
             sizeof(shopItem),
             "[S]hop (%d marks)",
             game->run.marks);

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
    int displayHandIndex;
    int playerValue;
    int cardIndex;

    dealerInfo = getDealerInfo(game->run.dealerId);
    twistInfo = getTwistInfo(game->run.twistId);
    dealerValue = calculateDealerHandValue(&game->dealer.hand,
                                           game->run.dealerId);
    dealerCardHidden = hideDealerCard &&
                       !game->peekActive &&
                       game->run.twistId != TWIST_SNIPER;

    displayHandIndex = game->player.activeHand;

    if (displayHandIndex >= game->player.handCount) {
        displayHandIndex = game->player.handCount - 1;
    }

    if (displayHandIndex < 0) {
        displayHandIndex = 0;
    }

    if (game->player.activeHand < game->player.handCount) {
        playerValue = calculatePlayerHandValue(game);
    } else {
        playerValue = game->player.hands[displayHandIndex].value;
    }

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

    if (game->player.handCount > 1) {
        printf("Player (hand %d/%d): ",
               displayHandIndex + 1,
               game->player.handCount);
    } else {
        printf("Player: ");
    }

    for (cardIndex = 0;
         cardIndex < game->player.hands[displayHandIndex].count;
         cardIndex++) {
        if (cardIndex > 0) {
            printf(" ");
        }

        printCard(&game->player.hands[displayHandIndex].cards[cardIndex]);
    }

    printf(" | Total value: %d\n", playerValue);
    printf("Chips: %d | Bet: %d\n",
           game->player.chips,
           game->player.bets[displayHandIndex]);
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
                } else if (game->run.marks < perkCost((PerkId)selectedIndex)) {
                    pendingMessage = "Not enough marks.";
                } else {
                    game->run.marks -= perkCost((PerkId)selectedIndex);
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
                    } else if (game->run.marks <
                               perkCost((PerkId)selectedIndex)) {
                        pendingMessage = "Not enough marks.";
                    } else {
                        game->run.marks -=
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
                } else if (game->run.marks <
                           perkCost((PerkId)selectedIndex)) {
                    pendingMessage = "Not enough marks.";
                } else {
                    game->run.marks -= perkCost((PerkId)selectedIndex);
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
