#define _DEFAULT_SOURCE
#include "round.h"
#include "round_cards.h"
#include "round_economy.h"
#include "round_rules.h"
#include "round_input.h"
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


static const char *CURSE_NAMES[] = {
    "None",
    "Bleed",
    "Frost",
    "Echo"
};

void drawCard(Game *game, Hand *hand, int allowCurse) {
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



void resetPlayerHandToEleven(Game *game) {
    Card recoveryCard;

    resetHand(&game->player.hands[game->player.activeHand]);
    recoveryCard.suit = HEARTS;
    recoveryCard.rank = ACE;
    recoveryCard.cursed = CURSED_NONE;
    addCardToHand(&game->player.hands[game->player.activeHand], &recoveryCard);
    game->player.hands[game->player.activeHand].value = 11;
}



// Reads and validates the player's bet.


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
        if (game->state != STATE_QUIT) {
            game->playerQuit = 1;
        }
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

    if (game->run.coldSnapActive) {
        int cardIndex;

        for (cardIndex = 0;
             cardIndex < game->player.hands[0].count;
             cardIndex++) {
            int attempts;
            Card drawn;
            int found;

            attempts = 0;
            found = 0;
            while (attempts < 50 && !found) {
                if (game->deck.top >= DECK_SIZE) {
                    initDeck(&game->deck);
                    shuffleDeck(&game->deck);
                }
                drawn = game->deck.cards[game->deck.top];
                game->deck.top++;
                if (drawn.rank <= SIX) {
                    game->player.hands[0].cards[cardIndex] = drawn;
                    found = 1;
                }
                attempts++;
            }
        }
        game->player.hands[0].value =
            calculateHandValue(&game->player.hands[0]);
        game->run.coldSnapActive = 0;
        logPush(game,
                "Cold snap: hand rewritten to low cards.",
                MSG_INFO);
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
    game->roundHadNaturalBj = 0;
    if (game->player.handCount == 1 &&
        game->player.hands[0].count == 2 &&
        playerValue == 21 &&
        !game->player.usedSplit) {
        game->roundHadNaturalBj = 1;
    }
    playerBJ = game->roundHadNaturalBj;

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
        roundSettleInsurance(game);
        game->peekActive = 1;
        if (playerBJ)
            logPush(game, "Dealer peeks. Both have blackjack.", MSG_PUSH);
        else
            logPush(game, "Dealer peeks. Blackjack.", MSG_LOSS);
        game->state = STATE_RESULT;
        return;
    }

    if (playerValue >= 21) {
        game->state = STATE_RESULT;
        return;
    }

    if (dealerForcesDouble(game->run.dealerId) &&
        game->run.floor >= 10) {
        if (game->player.hands[0].count == 2 &&
            (long long)game->player.chips >=
                (long long)game->player.bets[0] * 2) {

            game->player.bets[0] *= 2;
            game->player.usedDouble = 1;
            drawCard(game, &game->player.hands[0], 1);

            if (effectiveDealerMirrorsOnHit(game)) {
                drawCard(game, &game->dealer.hands[0], 0);
            }

            logPush(game, "Devil forces a double down.", MSG_DEALER);
            game->state = STATE_DEALER_TURN;
            return;
        }
    }

    game->state = STATE_PLAYER_TURN;
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
        numerator = effectiveDealerBlackjackNumerator(game);
        denominator = effectiveDealerBlackjackDenominator(game);

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

        if (effectiveDealerLossMultiplier(game) == 150) {
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
        playerBlackjack = game->roundHadNaturalBj &&
                          game->player.hands[handIndex].count == 2 &&
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
                            effectiveDealerLossMultiplier(game)) / 100;
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
                int bjRatioNumerator = effectiveDealerBlackjackNumerator(game);
                int bjRatioDenominator = effectiveDealerBlackjackDenominator(game);
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
                            effectiveDealerLossMultiplier(game)) / 100;

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
                            effectiveDealerLossMultiplier(game)) / 100;

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
        int rescueChips = getMinBet(game, game->run.floor, game->mode);
        char rescueMessage[64];

        if (rescueChips < 1) rescueChips = 1;
        game->player.chips = rescueChips;
        game->player.perkCharges[PERK_EXTRA_LIFE] = 0;
        game->lossStreak = 0;
        if (game->state == STATE_QUIT)
            game->state = game->mode == MODE_CLASSIC ? STATE_BET : STATE_MENU;
        snprintf(rescueMessage, sizeof(rescueMessage),
                 "Extra Life! Run continues with %d chips.", rescueChips);
        logPush(game, rescueMessage, MSG_WIN);
    }
    if (game->player.chips > 0 &&
        game->player.chips < getMinBet(game, game->run.floor, game->mode) &&
        game->player.perkOwned[PERK_EXTRA_LIFE] &&
        game->player.perkCharges[PERK_EXTRA_LIFE] > 0) {
        int rescueChips = getMinBet(game, game->run.floor, game->mode);
        char rescueMessage[64];

        if (rescueChips < 1) rescueChips = 1;
        game->player.chips = rescueChips;
        game->player.perkCharges[PERK_EXTRA_LIFE] = 0;
        game->lossStreak = 0;
        if (game->state == STATE_QUIT)
            game->state = game->mode == MODE_CLASSIC ? STATE_BET : STATE_MENU;
        snprintf(rescueMessage, sizeof(rescueMessage),
                 "Extra Life! Run continues with %d chips.", rescueChips);
        logPush(game, rescueMessage, MSG_WIN);
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
    game->roundHadNaturalBj = 0;
}
