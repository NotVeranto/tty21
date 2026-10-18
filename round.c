#define _DEFAULT_SOURCE
#include "round.h"
#include "render.h"
#include "input.h"
#include "input_map.h"
#include "rng.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DECK_RESHUFFLE_MARGIN 15
#define FIRST_CARD_RETRY_LIMIT 50

void creditChips(Game *game, int amount) {
    long long total;
    if (amount <= 0 || game->player.chips >= INT_MAX) return;
    total = (long long)game->player.chips + amount;
    game->player.chips = total > INT_MAX ? INT_MAX : (int)total;
}

static void settleInsurance(Game *game) {
    long long value;
    int payout;
    char message[64];
    if (game->player.insuranceBet <= 0) return;
    value = (long long)game->player.insuranceBet * 3;
    payout = value > INT_MAX - game->player.chips
        ? INT_MAX - game->player.chips : (int)value;
    creditChips(game, payout);
    snprintf(message, sizeof(message), "Insurance pays: +%d chips.", payout);
    logPush(game, message, MSG_WIN);
    game->player.insuranceBet = 0;
}

int twistActive(const Game *game, TwistId id) {
    int i;

    if (game->run.twistBlocked &&
        id == game->run.twistId) {
        return 0;
    }

    if (game->run.twistId == id) {
        return 1;
    }

    for (i = 0; i < game->run.extraTwistCount; i++) {
        if (game->run.extraTwists[i] == id) {
            return 1;
        }
    }

    return 0;
}

static TwistId pickHostileTwistExcluding(const Game *game) {
    static const TwistId POOL[] = {
        TWIST_COLD_DECK,
        TWIST_HALF_PAYOUT,
        TWIST_STICKY_HANDS,
        TWIST_DESPERATION,
        TWIST_DEALER_TO_18,
        TWIST_BLOOD_PACT
    };
    TwistId candidates[6];
    int count;
    int i;

    count = 0;

    for (i = 0; i < 6; i++) {
        if (!twistActive(game, POOL[i])) {
            candidates[count] = POOL[i];
            count++;
        }
    }

    if (count == 0) {
        return TWIST_NONE;
    }

    return candidates[rngRange(0, count - 1)];
}

static void applyAntiFarmPressure(Game *game) {
    int r;
    int threshold;

    r = game->run.roundsOnFloor;
    threshold = 0;

    if (r >= 16) {
        threshold = 3 + (r - 16) / 3;

        if (threshold > MAX_EXTRA_TWISTS) {
            threshold = MAX_EXTRA_TWISTS;
        }
    } else if (r >= 11) {
        threshold = 2;
    } else if (r >= 6) {
        threshold = 1;
    }

    while (game->run.extraTwistCount < threshold) {
        TwistId pick;
        char msg[96];

        pick = pickHostileTwistExcluding(game);

        if (pick == TWIST_NONE) {
            break;
        }

        game->run.extraTwists[game->run.extraTwistCount] = pick;
        game->run.extraTwistCount++;

        snprintf(msg,
                 sizeof(msg),
                 "Floor tension rises: %s activates.",
                 getTwistInfo(pick)->name);
        logPush(game, msg, MSG_LOSS);
    }
}

int fatiguePercent(int roundsOnFloor) {
    if (roundsOnFloor <= 5) {
        return 100;
    }

    if (roundsOnFloor <= 8) {
        return 80;
    }

    if (roundsOnFloor <= 11) {
        return 60;
    }

    if (roundsOnFloor <= 14) {
        return 50;
    }

    return 30;
}

static const char *CURSE_NAMES[] = {
    "None",
    "Bleed",
    "Frost",
    "Echo"
};

static void drawCard(Game *game, Hand *hand, int allowCurse) {
    int previousCount;
    CurseId curseId;
    char message[64];

    if (hand->count >= MAX_HAND_SIZE) return;
    if (game->deck.top >= DECK_SIZE) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
        logPush(game, "Deck reshuffled.", MSG_INFO);
    }

    previousCount = hand->count;
    addCardToHand(hand, &game->deck.cards[game->deck.top]);
    game->deck.top++;

    if (allowCurse &&
        game->mode == MODE_ROGUELIKE &&
        hand->count > previousCount &&
        rngRange(0, 99) < 3) {
        curseId = (CurseId)rngRange(1, 3);
        hand->cards[hand->count - 1].cursed = curseId;
        snprintf(message,
                 sizeof(message),
                 "Cursed card: %s.",
                 CURSE_NAMES[curseId]);
        logPush(game, message, MSG_INFO);
    }
}

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
            game->player.hands[game->player.activeHand].cards[0] = replacementCard;
            accepted = 1;
        }

        attemptCount++;
    }

    game->player.hands[game->player.activeHand].value = calculateHandValue(&game->player.hands[game->player.activeHand]);
}

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

static void resetPlayerHandToEleven(Game *game) {
    Card recoveryCard;

    resetHand(&game->player.hands[game->player.activeHand]);
    recoveryCard.suit = HEARTS;
    recoveryCard.rank = ACE;
    recoveryCard.cursed = CURSED_NONE;
    addCardToHand(&game->player.hands[game->player.activeHand], &recoveryCard);
    game->player.hands[game->player.activeHand].value = 11;
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
    int maxBet;
    int validBet;
    int betDivisor;
    KeyEvent event;

    minBet = getMinBet(game, game->run.floor, game->mode);
    maxBet = game->player.chips;
    betDivisor = dealerHandsCount(game->run.dealerId);

    if (game->mode == MODE_ROGUELIKE) {
        int cap = game->player.chips;

        if (game->run.floor >= 12) {
            cap = game->player.chips / 3;
        } else if (game->run.floor >= 8) {
            cap = game->player.chips / 2;
        }

        if (cap >= minBet) {
            maxBet = cap;
        }
    }

    if (game->player.chips < minBet) {
        snprintf(prompt,
                 sizeof(prompt),
                 "Minimum bet is %d. You have %d chips. Run over.",
                 minBet,
                 maxBet);
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
                     maxBet,
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
                        betValue <= maxBet) {
                        if (betDivisor > 1 && betValue % betDivisor != 0) {
                            snprintf(message,
                                     sizeof(message),
                                     "Invalid bet. Enter a multiple of %d between %d and %d.",
                                     betDivisor,
                                     minBet,
                                     maxBet);
                            renderMessage(message, ROW_RESULT, MSG_DEFAULT);
                            validBet = 0;
                        } else {
                            validBet = 1;
                        }
                    } else {
                        snprintf(message,
                                 sizeof(message),
                                 "Invalid bet. Enter an integer between %d and %d.",
                                 minBet,
                                 maxBet);
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
                 maxBet);

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
                         maxBet);
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
                betValue <= maxBet) {
                if (betDivisor > 1 && betValue % betDivisor != 0) {
                    snprintf(message,
                             sizeof(message),
                             "Invalid bet. Enter a multiple of %d between %d and %d.",
                             betDivisor,
                             minBet,
                             maxBet);
                    renderMessage(message, ROW_RESULT, MSG_DEFAULT);
                    validBet = 0;
                } else {
                    validBet = 1;
                }
            } else {
                snprintf(message,
                         sizeof(message),
                         "Invalid bet. Enter an integer between %d and %d.",
                         minBet,
                         maxBet);
                renderMessage(message, ROW_RESULT, MSG_DEFAULT);
            }
        }

        return (int)betValue;
    }
}

static int canDouble(const Game *game) {
    if (game->run.floor >= 10 && game->run.dealerId == DEALER_VOID) return 0;
    return game->player.hands[game->player.activeHand].count == 2 &&
           (long long)game->player.chips >=
               (long long)game->player.bets[game->player.activeHand] * 2;
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
           (long long)game->player.chips >=
               2LL * (long long)game->player.bets[game->player.activeHand] &&
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
    game->player.usedSplit = 1;

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

    renderTable(game, game->state == STATE_PLAYER_TURN);
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

    actions[actionCount++] = ACTION_QUIT;

    return actionCount;
}

static void dispatchPlayerAction(Game *game, ActionId action) {
    int value;

    if (action == ACTION_HIT) {
        if (game->player.hands[game->player.activeHand].count >= MAX_HAND_SIZE) {
            logPush(game, "Hand is full.", MSG_DEFAULT);
            renderTable(game, game->state == STATE_PLAYER_TURN);
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

        if (dealerMirrorsOnHit(game->run.dealerId)) {
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
                renderTable(game, 1);
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

        renderTable(game, game->state == STATE_PLAYER_TURN);
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

            renderTable(game, game->state == STATE_PLAYER_TURN);
        }
    } else if (action == ACTION_DOUBLE) {
        if (!canDouble(game)) {
            logPush(game,
                     "Cannot double now.",
                     MSG_DEFAULT);
        } else {
            game->player.bets[game->player.activeHand] *= 2;
            game->player.usedDouble = 1;
            drawCard(game, &game->player.hands[game->player.activeHand], 1);

            if (dealerMirrorsOnHit(game->run.dealerId)) {
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
            renderTable(game, game->state == STATE_PLAYER_TURN);
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
            renderTable(game, 1);
        }
    } else if (action == ACTION_QUIT) {
        game->state = STATE_QUIT;
    }
}

static int countCursedFrost(const Hand *hand) {
    int count;
    int cardIndex;

    count = 0;

    for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
        if (hand->cards[cardIndex].cursed == CURSED_FROST) {
            count++;
        }
    }

    return count;
}

static int calculateHandValueAcesLow(const Hand *hand) {
    int total;
    int i;

    total = 0;

    for (i = 0; i < hand->count; i++) {
        if (hand->cards[i].rank == ACE) {
            total += 1;
        } else {
            total += getCardValue(&hand->cards[i]);
        }
    }

    return total;
}

int calculateDealerHandValue(const Game *game,
                             const Hand *hand,
                             DealerId id) {
    if (twistActive(game, TWIST_ACES_LOW)) {
        return calculateHandValueAcesLow(hand);
    }

    if (!dealerAcesCountAsOne(id)) {
        return calculateHandValue(hand);
    }

    return calculateHandValueAcesLow(hand);
}

int calculatePlayerHandValue(const Game *game) {
    int totalValue;
    int frostCount;

    if (twistActive(game, TWIST_ACES_LOW)) {
        totalValue = calculateHandValueAcesLow(
            &game->player.hands[game->player.activeHand]);
    } else if (!game->player.perkOwned[PERK_ACE_HIGH]) {
        totalValue = game->player.hands[game->player.activeHand].value;
    } else {
        int cardIndex;

        totalValue = 0;

        for (cardIndex = 0;
             cardIndex < game->player.hands[game->player.activeHand].count;
             cardIndex++) {
            totalValue +=
                getCardValue(&game->player.hands[game->player.activeHand]
                                 .cards[cardIndex]);
        }
    }

    frostCount = countCursedFrost(
        &game->player.hands[game->player.activeHand]);
    totalValue -= frostCount * 2;

    if (totalValue < 0) {
        totalValue = 0;
    }

    return totalValue;
}

int calculatePlayerHandValueForIndex(const Game *game,
                                     int handIndex) {
    int totalValue;
    int frostCount;

    if (twistActive(game, TWIST_ACES_LOW)) {
        totalValue = calculateHandValueAcesLow(
            &game->player.hands[handIndex]);
    } else if (!game->player.perkOwned[PERK_ACE_HIGH]) {
        totalValue = game->player.hands[handIndex].value;
    } else {
        int cardIndex;

        totalValue = 0;

        for (cardIndex = 0;
             cardIndex < game->player.hands[handIndex].count;
             cardIndex++) {
            totalValue +=
                getCardValue(&game->player.hands[handIndex]
                                 .cards[cardIndex]);
        }
    }

    frostCount = countCursedFrost(
        &game->player.hands[handIndex]);
    totalValue -= frostCount * 2;

    if (totalValue < 0) {
        totalValue = 0;
    }

    return totalValue;
}

static int readYesNoPrompt(const char *prompt) {
    while (1) {
        int answer;

        renderMessage(prompt, ROW_PROMPT, MSG_DEFAULT);

        if (inputIsRaw()) {
            KeyEvent event;

            event = readKey();
            if (event.type == KEY_ESC || event.type == KEY_EOF) {
                return 0;
            }
            if (event.type != KEY_CHAR) {
                continue;
            }
            answer = (unsigned char)event.ch;
        } else {
            answer = getchar();

            if (answer == EOF) {
                return 0;
            }

            if (answer != '\n' && answer != '\r') {
                while (1) {
                    int character;

                    character = getchar();
                    if (character == '\n' || character == EOF) {
                        break;
                    }
                }
            }
        }

        if (answer == 'y' || answer == 'Y') {
            return 1;
        }
        if (answer == 'n' || answer == 'N') {
            return 0;
        }
    }
}

static void askInsurance(Game *game) {
    int cost;
    char prompt[80];
    char message[80];

    cost = game->player.bets[0] / 2;
    if (cost < 1) {
        cost = 1;
    }

    if (game->player.chips < cost) {
        return;
    }

    snprintf(prompt, sizeof(prompt),
             "Insurance? [y/n] (cost %d chips)", cost);

    if (readYesNoPrompt(prompt)) {
        game->player.chips -= cost;
        game->player.insuranceBet = cost;
        snprintf(message, sizeof(message),
                 "Insurance placed: %d chips.", cost);
        logPush(game, message, MSG_INFO);
    } else {
        game->player.insuranceBet = 0;
    }
}

static void askEvenMoney(Game *game) {
    const char *prompt = "Take even money? [y/n] (pays 1:1)";
    int bet;
    if (!readYesNoPrompt(prompt)) return;
    bet = game->player.bets[0];
    creditChips(game, bet);
    game->player.insuranceBet = 0;
    game->player.evenMoneyTaken = 1;
    game->state = STATE_RESULT;
}

void startRound(Game *game) {
    /*
     * Stage 16a peek/insurance/even-money paths have no unit tests yet.
     * Stage 16.6 will add coverage; validate these paths manually meanwhile.
     */
    int playerValue;
    int betAmount;
    int upcardRank;
    int upcardIsAce;
    int upcardIsTen;
    int dealerBJ;
    int playerBJ;

    game->peekActive = 0;

    if (twistActive(game, TWIST_SIX_DECK)) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    } else if (!twistActive(game, TWIST_NO_RESHUFFLE) &&
               game->deck.top > DECK_SIZE - DECK_RESHUFFLE_MARGIN) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    }

    renderTable(game, 0);

    betAmount = readBet(game);

    if (betAmount < 0 || game->state == STATE_QUIT) {
        game->state = STATE_QUIT;
        return;
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
    game->player.bets[game->player.activeHand] = betAmount;
    game->player.usedSplit = 0;
    game->player.usedDouble = 0;
    game->player.insuranceBet = 0;
    game->player.evenMoneyTaken = 0;
    game->lastPayoutBreakdown[0] = '\0';

    game->roundNumber++;

    if (game->mode == MODE_ROGUELIKE) {
        game->run.roundsOnFloor++;
        applyAntiFarmPressure(game);
    }

    if (game->player.perkOwned[PERK_MARKED_CARD]) {
        game->player.perkCharges[PERK_MARKED_CARD] = 1;
    }

    drawCard(game, &game->player.hands[0], 1);
    drawCard(game, &game->dealer.hands[0], 0);
    drawCard(game, &game->player.hands[0], 1);
    drawCard(game, &game->dealer.hands[0], 0);
    game->dealer.handCount = dealerHandsCount(game->run.dealerId);

    {
        int dealerHandIndex;

        for (dealerHandIndex = 1;
             dealerHandIndex < game->dealer.handCount;
             dealerHandIndex++) {
            drawCard(game,
                     &game->dealer.hands[dealerHandIndex], 0);
            drawCard(game,
                     &game->dealer.hands[dealerHandIndex], 0);
        }
    }

    if (game->tippedThisRound) {
        game->peekActive = 1;
        game->tippedThisRound = 0;
    }

    if (twistActive(game, TWIST_LUCKY_DRAW)) {
        redrawFirstPlayerCard(game, 1);
    } else if (twistActive(game, TWIST_COLD_DECK)) {
        redrawFirstPlayerCard(game, 0);
    }

    {
        char betMsg[64];

        snprintf(betMsg, sizeof(betMsg),
                 "Bet: %d.",
                 betAmount);
        logPush(game, betMsg, MSG_ACTION);
    }

    upcardRank = game->dealer.hands[0].cards[0].rank;
    upcardIsAce = upcardRank == ACE;
    upcardIsTen = upcardRank >= TEN;

    playerValue = calculatePlayerHandValue(game);
    playerBJ = game->player.hands[0].count == 2 &&
               playerValue == 21 &&
               !game->player.usedSplit;

    if (upcardIsAce) {
        if (playerBJ && dealerHandsCount(game->run.dealerId) == 1) {
            askEvenMoney(game);
            if (game->state == STATE_RESULT) {
                return;
            }
        } else {
            askInsurance(game);
        }
    }

    dealerBJ = 0;
    if ((upcardIsAce || upcardIsTen) &&
        game->dealer.hands[0].count == 2) {
        int dealerVal;

        dealerVal = calculateDealerHandValue(
            game,
            &game->dealer.hands[0],
            game->run.dealerId);
        dealerBJ = dealerVal == 21;
    }

    if (dealerBJ) {
        settleInsurance(game);
        game->peekActive = 1;
        if (playerBJ)
            logPush(game, "Dealer peeks. Both have blackjack.", MSG_PUSH);
        else
            logPush(game, "Dealer peeks. Blackjack.", MSG_LOSS);
        game->state = STATE_RESULT;
        return;
    }

    if (dealerForcesDouble(game->run.dealerId)) {
        if (game->player.hands[0].count == 2 &&
            (long long)game->player.chips >=
                (long long)game->player.bets[0] * 2) {

            game->player.bets[0] *= 2;
            game->player.usedDouble = 1;
            drawCard(game, &game->player.hands[0], 1);

            if (dealerMirrorsOnHit(game->run.dealerId)) {
                drawCard(game, &game->dealer.hands[0], 0);
            }

            game->state = STATE_DEALER_TURN;
            return;
        }
    }

    if (playerValue >= 21) {
        game->state = STATE_RESULT;
    } else {
        game->state = STATE_PLAYER_TURN;
    }
}

void playerTurn(Game *game) {
    ActionId actions[8];
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

            renderTable(game, 1);
            renderActionBar(actions,
                            actionCount,
                            actionIndex,
                            ROW_PROMPT);

            event = readKey();

            if (event.type == KEY_CHAR && event.ch == '?') {
                renderHelpOverlay(game);
                (void)readKey();
                renderTable(game, 1);
                renderActionBar(actions,
                                actionCount,
                                actionIndex,
                                ROW_PROMPT);
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
            } else if (event.type == KEY_UP ||
                       event.type == KEY_DOWN) {
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
                    }

                    dispatchPlayerAction(game, action);
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
            renderMessage("Choose action: [h/s/d/p/k/q]: ",
                          ROW_PROMPT,
                          MSG_DEFAULT);
            inputCharacter = getchar();

            if (inputCharacter == EOF) {
                game->state = STATE_QUIT;
            } else {
                event = makeKeyEvent(inputCharacter);

                while ((inputCharacter = getchar()) != '\n' &&
                       inputCharacter != EOF) {
                }

                if (event.type == KEY_CHAR && event.ch == '?') {
                    renderHelpOverlay(game);
                    renderTable(game, 1);
                    renderActionBar(actions,
                                    actionCount,
                                    actionIndex,
                                    ROW_PROMPT);
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
                        }

                        dispatchPlayerAction(game, action);
                    } else {
                        renderMessage("Invalid action. Enter h, s, d, p, k, or q.",
                                      ROW_PROMPT,
                                      MSG_DEFAULT);
                    }
                } else {
                    renderMessage("Invalid action. Enter h, s, d, p, k, or q.",
                                  ROW_PROMPT,
                                  MSG_DEFAULT);
                }
            }
        }
    }
}

void dealerTurn(Game *game) {
    int targetValue;
    int dealerHandIndex;
    int dealerValue;

    if (game->state == STATE_DEALER_TURN) {
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

                drawCard(game,
                         &game->dealer.hands[dealerHandIndex], 0);
                dealerValue = calculateDealerHandValue(
                    game,
                    &game->dealer.hands[dealerHandIndex],
                    game->run.dealerId);
                renderTable(game, 0);

                if (game->fastMode) {
                    usleep(180000);
                } else if (inputIsRaw()) {
                    renderMessage("Press any key for next card...",
                                  ROW_PROMPT,
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

static void appendResultMessage(char *buffer,
                                size_t bufferSize,
                                const char *message,
                                int *truncated) {
    size_t currentLength;
    const char *separator;
    int written;

    if (*truncated) {
        return;
    }

    currentLength = strlen(buffer);
    separator = currentLength > 0 ? "   " : "";
    written = snprintf(buffer + currentLength,
                       bufferSize - currentLength,
                       "%s%s",
                       separator,
                       message);

    if (written < 0) {
        *truncated = 1;
    } else if ((size_t)written >= bufferSize - currentLength) {
        if (bufferSize >= 4) {
            buffer[bufferSize - 4] = '.';
            buffer[bufferSize - 3] = '.';
            buffer[bufferSize - 2] = '.';
            buffer[bufferSize - 1] = '\0';
        }

        *truncated = 1;
    }
}

static void appendModifierToken(char *out, size_t outSize,
                                 const char *token) {
    size_t length;

    length = strlen(out);
    if (length + 2 + strlen(token) >= outSize) {
        return;
    }

    snprintf(out + length,
             outSize - length,
             "%s%s",
             length > 0 ? ", " : "",
             token);
}

static void buildModifierList(const Game *game,
                              int winStreakBefore,
                              int roundHadBlackjack,
                              int isWin,
                              char *out, size_t outSize) {
    int numerator;
    int denominator;
    int powerMultiplier;
    int compoundingBonus;
    char token[32];

    if (outSize == 0) {
        return;
    }

    out[0] = '\0';

#define APPEND_MODIFIER(tokenValue) \
    do { \
        if (strlen(out) + 2 + strlen(tokenValue) >= outSize) { \
            goto modifier_list_done; \
        } \
        appendModifierToken(out, outSize, tokenValue); \
    } while (0)

    if (roundHadBlackjack && isWin) {
        numerator = dealerBlackjackNumerator(game->run.dealerId);
        denominator = dealerBlackjackDenominator(game->run.dealerId);

        if (numerator == 3 && denominator == 2) {
            APPEND_MODIFIER("BJ 3:2");
        } else if (numerator == 1 && denominator == 1) {
            APPEND_MODIFIER("BJ 1:1");
        } else {
            APPEND_MODIFIER("BJ");
        }
    }

    if (twistActive(game, TWIST_DESPERATION) &&
        game->player.usedDouble && isWin) {
        APPEND_MODIFIER("Desp 3x");
    }

    if (twistActive(game, TWIST_HIGH_STAKES)) {
        APPEND_MODIFIER("High 2x");
    }

    if (twistActive(game, TWIST_BLOOD_PACT)) {
        APPEND_MODIFIER("Blood 2x");
    }

    if (twistActive(game, TWIST_HALF_PAYOUT) && isWin) {
        APPEND_MODIFIER("Half 0.5");
    }

    if (twistActive(game, TWIST_CHARITY) && !isWin) {
        APPEND_MODIFIER("Charity 0.5");
    }

    if (twistActive(game, TWIST_BUST_INSURANCE) && !isWin) {
        APPEND_MODIFIER("BustIns 0.5");
    }

    if (game->mode == MODE_ROGUELIKE &&
        winStreakBefore >= 3 && isWin) {
        APPEND_MODIFIER("Streak 2x");
    }

    if (!isWin) {
        powerMultiplier = dealerPowerLossMultiplier(game->run.dealerId,
                                                     game->run.floor);
        if (powerMultiplier == 125) {
            APPEND_MODIFIER("Power x1.25");
        } else if (powerMultiplier == 150) {
            APPEND_MODIFIER("Power x1.5");
        } else if (powerMultiplier == 200) {
            APPEND_MODIFIER("Power x2.0");
        }

        if (dealerLossMultiplierPercent(game->run.dealerId) == 150) {
            APPEND_MODIFIER("Dealer 1.5");
        }
    }

    if (game->player.perkOwned[PERK_DOUBLE_DIP] &&
        game->player.usedDouble &&
        !game->player.usedSplit && isWin) {
        APPEND_MODIFIER("Dip +B/2");
    }

    if (game->player.perkOwned[PERK_COMPOUNDING] &&
        winStreakBefore > 0 && isWin) {
        compoundingBonus = winStreakBefore < 10 ? winStreakBefore : 10;
        snprintf(token, sizeof(token), "Comp +%d", compoundingBonus);
        APPEND_MODIFIER(token);
    }

modifier_list_done:
    if (out[0] == '\0') {
        snprintf(out, outSize, "(none)");
    }

#undef APPEND_MODIFIER
}

void resolveRound(Game *game) {
    int dealerValue;
    int dealerBlackjack;
    int dealerHandCount;
    int dealerValues[2];
    int dealerBlackjacks[2];
    int handIndex;
    int handValue;
    int handBet;
    int splitBet;
    int payout;
    int lossAmount;
    int playerBlackjack;
    int messageLength;
    int messageTruncated;
    int dealerHandIndex;
    int anyDealerBlackjack;
    int skipLossReduction;
    int greedAwarded;
    int handWon;
    int currentFatigue;
    int winStreakBefore;
    int roundHadBlackjack;
    int evenMoneyBefore;
    long long payoutValue;
    long long basePayout;
    long long baseLoss;
    long long payoutTotal;
    long long lossTotal;
    long long handPayoutTotal;
    long long handLossTotal;
    MessageStyle resultStyle;
    char resultMessage[256];
    char handMessage[64];
    char *payoutBreakdownText;

    winStreakBefore = game->winStreak;
    roundHadBlackjack = 0;
    evenMoneyBefore = game->player.evenMoneyTaken;
    payoutBreakdownText = game->lastPayoutBreakdown;
    game->lastPayoutBreakdown[0] = '\0';

    dealerHandCount = game->dealer.handCount;

    for (dealerHandIndex = 0;
         dealerHandIndex < dealerHandCount;
         dealerHandIndex++) {
        dealerValues[dealerHandIndex] =
            calculateDealerHandValue(game,
                                     &game->dealer.hands[dealerHandIndex],
                                     game->run.dealerId);
        dealerBlackjacks[dealerHandIndex] =
            game->dealer.hands[dealerHandIndex].count == 2 &&
            dealerValues[dealerHandIndex] == 21;
    }

    {
        int bleedCount;
        int playerCardIndex;

        bleedCount = 0;

        for (handIndex = 0;
             handIndex < game->player.handCount;
             handIndex++) {
            for (playerCardIndex = 0;
                 playerCardIndex < game->player.hands[handIndex].count;
                 playerCardIndex++) {
                if (game->player.hands[handIndex]
                        .cards[playerCardIndex].cursed == CURSED_BLEED) {
                    bleedCount++;
                }
            }
        }

        if (bleedCount > 0) {
            int bleedLoss;

            bleedLoss = bleedCount;

            if (bleedLoss > game->player.chips) {
                bleedLoss = game->player.chips;
            }

            game->player.chips -= bleedLoss;

            if (game->player.chips < 0) {
                game->player.chips = 0;
            }

            {
                char bleedMessage[64];

                snprintf(bleedMessage,
                         sizeof(bleedMessage),
                         "Bleed: -%d chips.",
                         bleedCount);
                logPush(game, bleedMessage, MSG_LOSS);
            }
        }
    }

    payoutTotal = 0;
    lossTotal = 0;
    greedAwarded = 0;
    if (game->mode == MODE_ROGUELIKE) {
        currentFatigue = fatiguePercent(game->run.roundsOnFloor);

        if (game->fatigueTier == 0) {
            game->fatigueTier = currentFatigue;
        } else if (currentFatigue < game->fatigueTier) {
            char fatigueMessage[64];

            snprintf(fatigueMessage,
                     sizeof(fatigueMessage),
                     "Payout reduced to %d%% (floor fatigue).",
                     currentFatigue);
            logPush(game, fatigueMessage, MSG_LOSS);
            game->fatigueTier = currentFatigue;
        }
    } else {
        currentFatigue = 100;
    }

    resultMessage[0] = '\0';
    messageTruncated = 0;

    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        handValue = calculatePlayerHandValueForIndex(game, handIndex);
        handBet = game->player.bets[handIndex];
        playerBlackjack = game->player.hands[handIndex].count == 2 &&
                          handValue == 21 &&
                          !game->player.usedSplit;
        splitBet = handBet / dealerHandCount;
        handPayoutTotal = 0;
        handLossTotal = 0;
        anyDealerBlackjack = 0;

        if (game->player.evenMoneyTaken) {
            payoutTotal += handBet;
            handPayoutTotal = handBet;
            handWon = 1;
            snprintf(resultMessage, sizeof(resultMessage),
                     "Even money taken: +%d chips.", handBet);
            continue;
        }

        for (dealerHandIndex = 0;
             dealerHandIndex < dealerHandCount;
             dealerHandIndex++) {
            dealerValue = dealerValues[dealerHandIndex];
            dealerBlackjack = dealerBlackjacks[dealerHandIndex];

            if (dealerBlackjack) {
                anyDealerBlackjack = 1;
            }

            payout = 0;
            lossAmount = 0;
            payoutValue = 0;
            basePayout = 0;
            baseLoss = 0;
            skipLossReduction = 0;
            handWon = 0;

            if (handValue > 21) {
                baseLoss = ((long long)splitBet *
                            dealerLossMultiplierPercent(game->run.dealerId)) / 100;
                skipLossReduction =
                    twistActive(game, TWIST_DESPERATION) &&
                    game->player.usedDouble;

                if (!skipLossReduction &&
                    twistActive(game, TWIST_BUST_INSURANCE)) {
                    baseLoss /= 2;
                }

                if (!skipLossReduction &&
                    twistActive(game, TWIST_CHARITY)) {
                    baseLoss /= 2;
                }

                if (!skipLossReduction &&
                    twistActive(game, TWIST_HIGH_STAKES)) {
                    baseLoss *= 2;
                }

                if (twistActive(game, TWIST_BLOOD_PACT)) {
                    baseLoss *= 2;
                }

                {
                    long long lossPower = dealerPowerLossMultiplier(
                        game->run.dealerId, game->run.floor);
                    if (lossPower != 100) {
                        baseLoss = baseLoss * lossPower / 100;
                    }
                }

                if (baseLoss > INT_MAX) {
                    baseLoss = INT_MAX;
                }

                if (game->player.perkOwned[PERK_DEALERS_FAVOR] &&
                    game->player.perkCharges[PERK_DEALERS_FAVOR] > 0) {
                    game->player.perkCharges[PERK_DEALERS_FAVOR]--;
                    game->winStreak = 0;

                    if (dealerHandCount == 1 &&
                        game->player.handCount == 1) {
                        snprintf(resultMessage,
                                 sizeof(resultMessage),
                                 "Dealer's Favor: bust pushed.");
                    }
                } else {
                    if (game->player.perkOwned[PERK_INSURANCE_PERK] &&
                        game->player.perkCharges[PERK_INSURANCE_PERK] > 0 &&
                        baseLoss > 0) {
                        baseLoss /= 2;
                        game->player.perkCharges[PERK_INSURANCE_PERK]--;
                        logPush(game,
                                "Insurance used: loss halved.",
                                MSG_INFO);
                    }

                    lossAmount = applyLoss(game, baseLoss);
                    lossTotal += lossAmount;
                    handLossTotal += lossAmount;

                    if (dealerHandCount == 1) {
                        if (game->player.handCount == 1) {
                            snprintf(resultMessage,
                                     sizeof(resultMessage),
                                     "Player busts. You lose %d chips. Chips: %d.",
                                     lossAmount,
                                     game->player.chips);
                        } else {
                            messageLength = snprintf(handMessage,
                                                     sizeof(handMessage),
                                                     "Hand %d: bust (-%d)",
                                                     handIndex + 1,
                                                     lossAmount);

                            if (messageLength < 0) {
                                snprintf(handMessage,
                                         sizeof(handMessage),
                                         "Hand %d: bust",
                                         handIndex + 1);
                            }

                            appendResultMessage(resultMessage,
                                                sizeof(resultMessage),
                                                handMessage,
                                                &messageTruncated);
                        }
                    }
                }
            } else if (playerBlackjack && !dealerBlackjack) {
                int bjRatioNumerator = dealerBlackjackNumerator(game->run.dealerId);
                int bjRatioDenominator = dealerBlackjackDenominator(game->run.dealerId);
                roundHadBlackjack = 1;
                if (game->run.floor >= 10 && game->run.dealerId == DEALER_REAPER) {
                    bjRatioNumerator = 1;
                    bjRatioDenominator = 1;
                }
                basePayout = ((long long)splitBet * bjRatioNumerator) /
                             bjRatioDenominator;

                if (twistActive(game, TWIST_DESPERATION) &&
                    game->player.usedDouble) {
                    basePayout *= 3;
                }

                if (twistActive(game, TWIST_HIGH_STAKES)) {
                    basePayout *= 2;
                }

                if (twistActive(game, TWIST_BLOOD_PACT)) {
                    basePayout *= 2;
                }

                if (game->player.perkOwned[PERK_DOUBLE_DIP] &&
                    game->player.usedDouble &&
                    !game->player.usedSplit) {
                    basePayout += splitBet / 2;
                }

                if (game->player.perkOwned[PERK_COMPOUNDING]) {
                    basePayout += game->winStreak < 10 ?
                                  game->winStreak : 10;
                }

                payoutValue = basePayout;

                if (game->mode == MODE_ROGUELIKE &&
                    game->winStreak >= 3) {
                    /* Apply the streak bonus before floor fatigue. */
                    payoutValue *= 2;
                }

                payoutValue = payoutValue *
                              currentFatigue / 100;

                if (payoutValue > INT_MAX - game->player.chips) {
                    payout = INT_MAX - game->player.chips;
                } else {
                    payout = (int)payoutValue;
                }

                creditChips(game, payout);
                payoutTotal += payout;
                handPayoutTotal += payout;
                handWon = 1;

                if (dealerHandCount == 1) {
                    if (game->player.handCount == 1) {
                        snprintf(resultMessage,
                                 sizeof(resultMessage),
                                 "Blackjack! You win %d chips. Chips: %d.",
                                 payout,
                                 game->player.chips);
                    } else {
                        messageLength = snprintf(handMessage,
                                                 sizeof(handMessage),
                                                 "Hand %d: blackjack (+%d)",
                                                 handIndex + 1,
                                                 payout);

                        if (messageLength < 0) {
                            snprintf(handMessage,
                                     sizeof(handMessage),
                                     "Hand %d: blackjack",
                                     handIndex + 1);
                        }

                        appendResultMessage(resultMessage,
                                            sizeof(resultMessage),
                                            handMessage,
                                            &messageTruncated);
                    }
                }
            } else if (dealerBlackjack && !playerBlackjack) {
                baseLoss = ((long long)splitBet *
                            dealerLossMultiplierPercent(game->run.dealerId)) / 100;

                if (twistActive(game, TWIST_CHARITY)) {
                    baseLoss /= 2;
                }

                if (twistActive(game, TWIST_HIGH_STAKES)) {
                    baseLoss *= 2;
                }

                if (twistActive(game, TWIST_BLOOD_PACT)) {
                    baseLoss *= 2;
                }

                {
                    long long lossPower = dealerPowerLossMultiplier(
                        game->run.dealerId, game->run.floor);
                    if (lossPower != 100) {
                        baseLoss = baseLoss * lossPower / 100;
                    }
                }

                if (baseLoss > INT_MAX) {
                    baseLoss = INT_MAX;
                }

                if (game->player.perkOwned[PERK_INSURANCE_PERK] &&
                    game->player.perkCharges[PERK_INSURANCE_PERK] > 0 &&
                    baseLoss > 0) {
                    baseLoss /= 2;
                    game->player.perkCharges[PERK_INSURANCE_PERK]--;
                    logPush(game,
                            "Insurance used: loss halved.",
                            MSG_INFO);
                }

                lossAmount = applyLoss(game, baseLoss);
                lossTotal += lossAmount;
                handLossTotal += lossAmount;

                game->winStreak = 0;

                if (dealerHandCount == 1) {
                    if (game->player.handCount == 1) {
                        snprintf(resultMessage,
                                 sizeof(resultMessage),
                                 "Dealer has blackjack. You lose %d chips. Chips: %d.",
                                 lossAmount,
                                 game->player.chips);
                    } else {
                        messageLength = snprintf(handMessage,
                                                 sizeof(handMessage),
                                                 "Hand %d: dealer BJ (-%d)",
                                                 handIndex + 1,
                                                 lossAmount);

                        if (messageLength < 0) {
                            snprintf(handMessage,
                                     sizeof(handMessage),
                                     "Hand %d: dealer BJ",
                                     handIndex + 1);
                        }

                        appendResultMessage(resultMessage,
                                            sizeof(resultMessage),
                                            handMessage,
                                            &messageTruncated);
                    }
                }
            } else if (handValue == dealerValue) {
                game->winStreak = 0;

                if (dealerHandCount == 1) {
                    if (game->player.handCount == 1) {
                        snprintf(resultMessage,
                                 sizeof(resultMessage),
                                 "Push. Your bet is returned. Chips: %d.",
                                 game->player.chips);
                    } else {
                        messageLength = snprintf(handMessage,
                                                 sizeof(handMessage),
                                                 "Hand %d: push",
                                                 handIndex + 1);

                        if (messageLength < 0) {
                            snprintf(handMessage,
                                     sizeof(handMessage),
                                     "Hand %d: push",
                                     handIndex + 1);
                        }

                        appendResultMessage(resultMessage,
                                            sizeof(resultMessage),
                                            handMessage,
                                            &messageTruncated);
                    }
                }
            } else if (dealerValue > 21 || handValue > dealerValue) {
                basePayout = splitBet;

                if (dealerPaysPureHandBonus(game->run.dealerId) &&
                    !game->player.usedSplit &&
                    !game->player.usedDouble) {
                    basePayout *= 2;
                }

                if (twistActive(game, TWIST_DESPERATION) &&
                    game->player.usedDouble) {
                    basePayout *= 3;
                }

                if (twistActive(game, TWIST_HIGH_STAKES)) {
                    basePayout *= 2;
                }

                if (twistActive(game, TWIST_BLOOD_PACT)) {
                    basePayout *= 2;
                }

                if (twistActive(game, TWIST_HALF_PAYOUT)) {
                    basePayout /= 2;
                }

                if (twistActive(game, TWIST_JACKPOT) &&
                    rngRange(0, 99) < 5) {
                    basePayout *= 10;
                }

                if (game->player.perkOwned[PERK_DOUBLE_DIP] &&
                    game->player.usedDouble &&
                    !game->player.usedSplit) {
                    basePayout += splitBet / 2;
                }

                if (game->player.perkOwned[PERK_COMPOUNDING]) {
                    basePayout += game->winStreak < 10 ?
                                  game->winStreak : 10;
                }

                payoutValue = basePayout;

                if (game->mode == MODE_ROGUELIKE &&
                    game->winStreak >= 3) {
                    /* Apply the streak bonus before floor fatigue. */
                    payoutValue *= 2;
                }

                payoutValue = payoutValue *
                              currentFatigue / 100;

                if (payoutValue > INT_MAX - game->player.chips) {
                    payout = INT_MAX - game->player.chips;
                } else {
                    payout = (int)payoutValue;
                }

                creditChips(game, payout);
                payoutTotal += payout;
                handPayoutTotal += payout;
                handWon = 1;

                if (dealerHandCount == 1 && game->player.handCount == 1) {
                    snprintf(resultMessage, sizeof(resultMessage),
                             "You win %d chips. Chips: %d.",
                             payout, game->player.chips);
                }
            } else {
                baseLoss = ((long long)splitBet *
                            dealerLossMultiplierPercent(game->run.dealerId)) / 100;

                if (twistActive(game, TWIST_CHARITY)) {
                    baseLoss /= 2;
                }

                if (twistActive(game, TWIST_HIGH_STAKES)) {
                    baseLoss *= 2;
                }

                if (twistActive(game, TWIST_BLOOD_PACT)) {
                    baseLoss *= 2;
                }

                {
                    long long lossPower = dealerPowerLossMultiplier(
                        game->run.dealerId, game->run.floor);
                    if (lossPower != 100) {
                        baseLoss = baseLoss * lossPower / 100;
                    }
                }

                if (game->player.perkOwned[PERK_INSURANCE_PERK] &&
                    game->player.perkCharges[PERK_INSURANCE_PERK] > 0 &&
                    baseLoss > 0) {
                    baseLoss /= 2;
                    game->player.perkCharges[PERK_INSURANCE_PERK]--;
                    logPush(game,
                            "Insurance used: loss halved.",
                            MSG_INFO);
                }

                if (baseLoss > INT_MAX) {
                    baseLoss = INT_MAX;
                }

                lossAmount = applyLoss(game, baseLoss);
                lossTotal += lossAmount;
                handLossTotal += lossAmount;
                game->winStreak = 0;

                if (dealerHandCount == 1 && game->player.handCount == 1) {
                    snprintf(resultMessage, sizeof(resultMessage),
                             "You lose %d chips. Chips: %d.",
                             lossAmount, game->player.chips);
                }
            }

            if (handWon &&
                twistActive(game, TWIST_GREED) &&
                !greedAwarded) {
                game->run.marks += 1;
                greedAwarded = 1;
            }
        }

        if (dealerHandCount > 1 && !game->player.evenMoneyTaken) {
            if (handValue > 21) {
                messageLength = snprintf(handMessage,
                                         sizeof(handMessage),
                                         "Hand %d: bust (-%lld)",
                                         handIndex + 1,
                                         handLossTotal);
            } else if (handPayoutTotal > handLossTotal) {
                if (playerBlackjack && !anyDealerBlackjack) {
                    messageLength = snprintf(handMessage,
                                             sizeof(handMessage),
                                             "Hand %d: blackjack (+%lld)",
                                             handIndex + 1,
                                             handPayoutTotal);
                } else {
                    messageLength = snprintf(handMessage,
                                             sizeof(handMessage),
                                             "Hand %d: win (+%lld)",
                                             handIndex + 1,
                                             handPayoutTotal);
                }
            } else if (handLossTotal > handPayoutTotal) {
                messageLength = snprintf(handMessage,
                                         sizeof(handMessage),
                                         "Hand %d: loss (-%lld)",
                                         handIndex + 1,
                                         handLossTotal);
            } else {
                messageLength = snprintf(handMessage,
                                         sizeof(handMessage),
                                         "Hand %d: push",
                                         handIndex + 1);
            }

            if (messageLength < 0) {
                snprintf(handMessage,
                         sizeof(handMessage),
                         "Hand %d: result",
                         handIndex + 1);
            }

            appendResultMessage(resultMessage,
                                sizeof(resultMessage),
                                handMessage,
                                &messageTruncated);
        }
    }

    if (payoutTotal > lossTotal) {
        resultStyle = MSG_WIN;
    } else if (payoutTotal < lossTotal) {
        resultStyle = MSG_LOSS;
    } else {
        resultStyle = MSG_PUSH;
    }

    if (game->mode == MODE_ROGUELIKE) {
        if (resultStyle == MSG_WIN) {
            game->winStreak++;
            game->lossStreak = 0;

            if (game->winStreak == 3) {
                logPush(game,
                        "Streak 3! Wins now pay double.",
                        MSG_WIN);
            }
        } else if (resultStyle == MSG_LOSS) {
            game->lossStreak++;
            game->winStreak = 0;
        }
    }

    if (resultStyle == MSG_WIN &&
        dealerIsBoss(game->run.dealerId) &&
        !game->run.bossRewarded) {
        long long rewardValue;
        long long availableMarks;
        int reward;
        char bossMsg[96];

        rewardValue = 10LL + (long long)game->run.floor * 2LL;
        if (rewardValue > INT_MAX) {
            rewardValue = INT_MAX;
        }

        availableMarks = (long long)INT_MAX - game->run.marks;
        if (rewardValue > availableMarks) {
            rewardValue = availableMarks;
        }

        if (rewardValue < 0) {
            rewardValue = 0;
        }

        reward = (int)rewardValue;
        game->run.marks += reward;
        game->run.bossRewarded = 1;
        snprintf(bossMsg, sizeof(bossMsg),
                 "BOSS DEFEATED. +%d marks.", reward);
        logPush(game, bossMsg, MSG_WIN);
    }

    logPush(game, resultMessage, resultStyle);

    if (game->mode == MODE_ROGUELIKE &&
        resultStyle == MSG_LOSS) {
        static const char *TAUNTS[] = {
            "The house always wins.",
            "Better luck next time.",
            "You'll be back."
        };
        int tauntIndex;

        tauntIndex = (game->run.floor + game->roundNumber) % 3;
        logPush(game, TAUNTS[tauntIndex], MSG_DEALER);
    }

    if (game->mode == MODE_ROGUELIKE &&
        game->lossStreak >= 5) {
        logPush(game,
                "Five losses in a row. The pit boss ends your run.",
                MSG_LOSS);
        game->state = STATE_QUIT;
    }

    if (game->safetyNetArmed &&
        game->player.chips == 0) {
        game->player.chips = 20;
        game->safetyNetArmed = 0;
        logPush(game,
                "Safety net triggered. 20 chips remain.",
                MSG_WIN);
    }

    if (game->player.chips == 0 &&
        game->player.perkOwned[PERK_EXTRA_LIFE] &&
        game->player.perkCharges[PERK_EXTRA_LIFE] > 0) {
        game->player.chips = 1;
        game->player.perkCharges[PERK_EXTRA_LIFE] = 0;
        if (game->state == STATE_QUIT)
            game->state = game->mode == MODE_CLASSIC ? STATE_BET : STATE_MENU;
        logPush(game, "Extra Life! Run continues with 1 chip.", MSG_WIN);
    }
    if (game->player.chips > 0 &&
        game->player.chips < getMinBet(game, game->run.floor, game->mode) &&
        game->player.perkOwned[PERK_EXTRA_LIFE] &&
        game->player.perkCharges[PERK_EXTRA_LIFE] > 0) {
        game->player.chips = getMinBet(game, game->run.floor, game->mode);
        game->player.perkCharges[PERK_EXTRA_LIFE] = 0;
        if (game->state == STATE_QUIT)
            game->state = game->mode == MODE_CLASSIC ? STATE_BET : STATE_MENU;
        logPush(game, "Extra Life! Chips restored to the minimum bet.", MSG_WIN);
    }

    game->tippedThisRound = 0;
    game->player.insuranceBet = 0;
    game->player.evenMoneyTaken = 0;

    renderTable(game, 0);

    {
        char breakdown[256];

        if (evenMoneyBefore) {
            snprintf(breakdown, sizeof(breakdown),
                     "Even money | bet %d paid 1:1",
                     game->player.bets[0]);
        } else if (payoutTotal > 0 && lossTotal > 0) {
            snprintf(breakdown, sizeof(breakdown),
                     "Won %lld, lost %lld",
                     payoutTotal, lossTotal);
        } else if (payoutTotal > 0) {
            char modifiers[160];
            buildModifierList(game, winStreakBefore,
                              roundHadBlackjack, 1,
                              modifiers, sizeof(modifiers));
            snprintf(breakdown, sizeof(breakdown),
                     "Won %lld | modifiers: %s | fatigue %d%%",
                     payoutTotal, modifiers, currentFatigue);
        } else if (lossTotal > 0) {
            char modifiers[160];
            buildModifierList(game, winStreakBefore,
                              0, 0,
                              modifiers, sizeof(modifiers));
            snprintf(breakdown, sizeof(breakdown),
                     "Lost %lld | modifiers: %s",
                     lossTotal, modifiers);
        } else {
            snprintf(breakdown, sizeof(breakdown),
                     "Push | bet returned");
        }

        snprintf(payoutBreakdownText,
                 sizeof(breakdown),
                 "%s", breakdown);
    }

    if (game->state == STATE_QUIT ||
        game->player.chips == 0) {
        game->state = STATE_QUIT;
    } else if (game->mode == MODE_CLASSIC) {
        game->state = STATE_BET;
    } else {
        game->state = STATE_MENU;
    }
}
