#include "round_settlement.h"

#include "round_cards.h"
#include "round_economy.h"
#include "round_rules.h"
#include "dealers.h"
#include "rng.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int winStreakBefore;
    int evenMoneyBefore;
    int roundHadBlackjack;
    int currentFatigue;
    int handValue;
    int playerBlackjack;
    int splitBet;
    int dealerValue;
    int dealerHandCount;
    int outcome;
    int compoundingAwarded;
} SettlementContext;

typedef enum {
    HAND_OUTCOME_BUST,
    HAND_OUTCOME_BLACKJACK,
    HAND_OUTCOME_DEALER_BLACKJACK,
    HAND_OUTCOME_PUSH,
    HAND_OUTCOME_WIN,
    HAND_OUTCOME_LOSS
} HandOutcome;

static void captureSettlementContext(const Game *game,
                                     SettlementContext *ctx) {
    ctx->winStreakBefore = game->winStreak;
    ctx->evenMoneyBefore = game->player.evenMoneyTaken;
    ctx->roundHadBlackjack = 0;
    ctx->compoundingAwarded = 0;
}

static HandOutcome classifyHandOutcome(int handValue,
                                       int playerBlackjack,
                                       int dealerBlackjack,
                                       int dealerValue) {
    if (handValue > 21) {
        return HAND_OUTCOME_BUST;
    }
    if (playerBlackjack && !dealerBlackjack) {
        return HAND_OUTCOME_BLACKJACK;
    }
    if (dealerBlackjack && !playerBlackjack) {
        return HAND_OUTCOME_DEALER_BLACKJACK;
    }
    if (handValue == dealerValue) {
        return HAND_OUTCOME_PUSH;
    }
    if (dealerValue > 21 || handValue > dealerValue) {
        return HAND_OUTCOME_WIN;
    }
    return HAND_OUTCOME_LOSS;
}


static int calculateHandPayout(Game *game, SettlementContext *ctx,
                               int playerHandIndex, int dealerHandIndex) {
    long long basePayout = 0;
    long long payoutValue;
    int splitBet = game->player.bets[playerHandIndex] / ctx->dealerHandCount;
    int payout;
    HandOutcome outcome = (HandOutcome)ctx->outcome;
    (void)dealerHandIndex;
    if (outcome == HAND_OUTCOME_BLACKJACK) {
        int numerator = effectiveDealerBlackjackNumerator(game);
        int denominator = effectiveDealerBlackjackDenominator(game);
        if (game->run.floor >= 10 && game->run.dealerId == DEALER_REAPER) {
            numerator = 1; denominator = 1;
        }
        basePayout = ((long long)splitBet * numerator) / denominator;
        if (twistActive(game, TWIST_DESPERATION) && game->player.usedDouble[playerHandIndex]) basePayout *= 3;
        if (twistActive(game, TWIST_HIGH_STAKES)) basePayout *= 2;
        if (twistActive(game, TWIST_BLOOD_PACT)) basePayout *= 2;
        if (game->player.perkOwned[PERK_DOUBLE_DIP] && game->player.usedDouble[playerHandIndex])
            basePayout += splitBet / 2;
        if (game->player.perkOwned[PERK_COMPOUNDING] &&
            !ctx->compoundingAwarded) {
            basePayout += game->winStreak < 10 ? game->winStreak : 10;
            ctx->compoundingAwarded = 1;
        }
    } else if (outcome == HAND_OUTCOME_WIN) {
        basePayout = splitBet;
        if (dealerPaysPureHandBonus(game->run.dealerId) && !game->player.usedSplit[playerHandIndex] && !game->player.usedDouble[playerHandIndex]) basePayout *= 2;
        if (twistActive(game, TWIST_DESPERATION) && game->player.usedDouble[playerHandIndex]) basePayout *= 3;
        if (twistActive(game, TWIST_HIGH_STAKES)) basePayout *= 2;
        if (twistActive(game, TWIST_BLOOD_PACT)) basePayout *= 2;
        if (twistActive(game, TWIST_HALF_PAYOUT)) basePayout /= 2;
        if (twistActive(game, TWIST_JACKPOT) && rngRange(0, 99) < 5) basePayout *= 10;
        if (game->player.perkOwned[PERK_DOUBLE_DIP] && game->player.usedDouble[playerHandIndex])
            basePayout += splitBet / 2;
        if (game->player.perkOwned[PERK_COMPOUNDING] &&
            !ctx->compoundingAwarded) {
            basePayout += game->winStreak < 10 ? game->winStreak : 10;
            ctx->compoundingAwarded = 1;
        }
    } else return 0;
    payoutValue = basePayout;
    if (game->mode == MODE_ROGUELIKE && game->winStreak >= 3) payoutValue *= 2;
    payoutValue = payoutValue * ctx->currentFatigue / 100;
    {
        long long payoutCap = (long long)splitBet * 20LL;
        if (payoutValue > payoutCap) {
            payoutValue = payoutCap;
            logPush(game, "Payout capped!", MSG_INFO);
        }
    }
    if (payoutValue > INT_MAX - game->player.chips) payout = INT_MAX - game->player.chips;
    else payout = (int)payoutValue;
    return payout;
}

static int calculateHandLoss(Game *game, const SettlementContext *ctx,
                             int playerHandIndex, int dealerHandIndex) {
    long long baseLoss;
    long long lossPower;
    int splitBet = game->player.bets[playerHandIndex] / ctx->dealerHandCount;
    int skipLossReduction;
    HandOutcome outcome = (HandOutcome)ctx->outcome;
    (void)dealerHandIndex;
    baseLoss = ((long long)splitBet * effectiveDealerLossMultiplier(game)) / 100;
    if (outcome == HAND_OUTCOME_BUST) {
        skipLossReduction = twistActive(game, TWIST_DESPERATION) && game->player.usedDouble[playerHandIndex];
        if (!skipLossReduction && twistActive(game, TWIST_BUST_INSURANCE)) baseLoss /= 2;
        if (!skipLossReduction && twistActive(game, TWIST_CHARITY)) baseLoss /= 2;
        if (!skipLossReduction && twistActive(game, TWIST_HIGH_STAKES)) baseLoss *= 2;
    } else {
        if (twistActive(game, TWIST_CHARITY)) baseLoss /= 2;
        if (twistActive(game, TWIST_HIGH_STAKES)) baseLoss *= 2;
    }
    if (twistActive(game, TWIST_BLOOD_PACT)) baseLoss *= 2;
    lossPower = dealerPowerLossMultiplier(game->run.dealerId, game->run.floor);
    if (lossPower != 100) baseLoss = baseLoss * lossPower / 100;
    if (outcome != HAND_OUTCOME_LOSS && baseLoss > INT_MAX) baseLoss = INT_MAX;
    if (outcome != HAND_OUTCOME_BUST &&
        game->player.perkOwned[PERK_INSURANCE_PERK] &&
        game->player.perkCharges[PERK_INSURANCE_PERK] > 0 && baseLoss > 0) {
        baseLoss /= 2;
        game->player.perkCharges[PERK_INSURANCE_PERK]--;
        logPush(game, "Insurance used: loss halved.", MSG_INFO);
    }
    if (baseLoss > INT_MAX) baseLoss = INT_MAX;
    return (int)baseLoss;
}

static void applyRoundRewards(Game *game, MessageStyle resultStyle) {
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
        if (reward > 0) {
            game->totalMarksEarned += reward;
        }
        game->totalBossesDefeated++;
        game->run.bossRewarded = 1;
        snprintf(bossMsg, sizeof(bossMsg),
                 "BOSS DEFEATED. +%d marks.", reward);
        logPush(game, bossMsg, MSG_WIN);
    }

}

static void applyRoundRecovery(Game *game) {
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

static int anyHandUsed(const Game *game, const int flags[MAX_PLAYER_HANDS]) {
    int handIndex;
    for (handIndex = 0; handIndex < game->player.handCount && handIndex < MAX_PLAYER_HANDS; handIndex++) {
        if (flags[handIndex]) return 1;
    }
    return 0;
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
    int modifierListFull;
    char token[32];

    if (outSize == 0) {
        return;
    }

    out[0] = '\0';

    modifierListFull = 0;

#define APPEND_MODIFIER(tokenValue) \
    do { \
        if (!modifierListFull) { \
            if (strlen(out) + 2 + strlen(tokenValue) >= outSize) { \
                modifierListFull = 1; \
            } else { \
                appendModifierToken(out, outSize, tokenValue); \
            } \
        } \
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
        anyHandUsed(game, game->player.usedDouble) && isWin) {
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
        anyHandUsed(game, game->player.usedDouble) &&
        !anyHandUsed(game, game->player.usedSplit) && isWin) {
        APPEND_MODIFIER("Dip +B/2");
    }

    if (game->player.perkOwned[PERK_COMPOUNDING] &&
        winStreakBefore > 0 && isWin) {
        compoundingBonus = winStreakBefore < 10 ? winStreakBefore : 10;
        snprintf(token, sizeof(token), "Comp +%d", compoundingBonus);
        APPEND_MODIFIER(token);
    }

    if (out[0] == '\0') {
        snprintf(out, outSize, "(none)");
    }

#undef APPEND_MODIFIER
}

MessageStyle resolveRound(Game *game) {
    SettlementContext ctx;
    HandOutcome outcome;
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
    int greedAwarded;
    int dealerFavorPush;
    int handWon;
    int currentFatigue;
    long long baseLoss;
    long long payoutTotal;
    long long lossTotal;
    long long handPayoutTotal;
    long long handLossTotal;
    MessageStyle resultStyle;
    char resultMessage[256];
    char handMessage[64];
    char *payoutBreakdownText;

    if (game->player.surrendered) {
        applyRoundRewards(game, MSG_LOSS);
        applyRoundRecovery(game);
        game->tippedThisRound = 0;
        game->player.insuranceBet = 0;
        game->player.evenMoneyTaken = 0;
        snprintf(game->lastPayoutBreakdown,
                 sizeof(game->lastPayoutBreakdown),
                 "Surrender | half bet lost");
        game->player.surrendered = 0;
        game->roundHadNaturalBj = 0;
        game->roundInProgress = 0;
        if (game->player.chips == 0) {
            game->state = STATE_QUIT;
        } else if (game->mode == MODE_CLASSIC) {
            game->state = STATE_BET;
        } else {
            game->state = STATE_MENU;
        }
        return MSG_DEFAULT;
    }

    captureSettlementContext(game, &ctx);
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

    ctx.currentFatigue = currentFatigue;
    ctx.dealerHandCount = dealerHandCount;
    resultMessage[0] = '\0';
    messageTruncated = 0;

    for (handIndex = 0;
         handIndex < game->player.handCount;
         handIndex++) {
        if (game->player.hands[handIndex].count < 1) {
            continue;
        }
        handValue = calculatePlayerHandValueForIndex(game, handIndex);
        handBet = game->player.bets[handIndex];
        playerBlackjack = game->roundHadNaturalBj &&
                          game->player.hands[handIndex].count == 2 &&
                          handValue == 21 &&
                          !game->player.usedSplit[handIndex];
        splitBet = handBet / dealerHandCount;
        ctx.handValue = handValue;
        ctx.playerBlackjack = playerBlackjack;
        ctx.splitBet = splitBet;
        handPayoutTotal = 0;
        handLossTotal = 0;
        anyDealerBlackjack = 0;
        dealerFavorPush = handValue > 21 &&
                          game->player.perkOwned[PERK_DEALERS_FAVOR] &&
                          game->player.perkCharges[PERK_DEALERS_FAVOR] > 0;
        if (dealerFavorPush) {
            game->player.perkCharges[PERK_DEALERS_FAVOR]--;
            if (dealerHandCount == 1) {
                if (game->player.handCount == 1) {
                    snprintf(resultMessage, sizeof(resultMessage),
                             "Dealer's Favor: bust pushed.");
                } else {
                    messageLength = snprintf(handMessage,
                                             sizeof(handMessage),
                                             "Hand %d: push",
                                             handIndex + 1);
                    appendResultMessage(resultMessage,
                                        sizeof(resultMessage),
                                        handMessage,
                                        &messageTruncated);
                }
            }
        }

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
            if (dealerFavorPush) {
                continue;
            }
            dealerValue = dealerValues[dealerHandIndex];
            dealerBlackjack = dealerBlackjacks[dealerHandIndex];

            if (dealerBlackjack) {
                anyDealerBlackjack = 1;
            }

            payout = 0;
            lossAmount = 0;
            baseLoss = 0;
            handWon = 0;

            outcome = classifyHandOutcome(handValue, playerBlackjack,
                                          dealerBlackjack, dealerValue);
            ctx.outcome = (int)outcome;
            ctx.dealerValue = dealerValue;
            if (outcome == HAND_OUTCOME_BUST) {
                baseLoss = calculateHandLoss(game, &ctx, handIndex, dealerHandIndex);

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
            } else if (outcome == HAND_OUTCOME_BLACKJACK) {
                ctx.roundHadBlackjack = 1;
                payout = calculateHandPayout(game, &ctx, handIndex, dealerHandIndex);
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
            } else if (outcome == HAND_OUTCOME_DEALER_BLACKJACK) {
                baseLoss = calculateHandLoss(game, &ctx, handIndex, dealerHandIndex);
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
            } else if (outcome == HAND_OUTCOME_PUSH) {

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
            } else if (outcome == HAND_OUTCOME_WIN) {
                payout = calculateHandPayout(game, &ctx, handIndex, dealerHandIndex);
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
                baseLoss = calculateHandLoss(game, &ctx, handIndex, dealerHandIndex);
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
                game->totalMarksEarned++;
                greedAwarded = 1;
            }
        }

        if (dealerHandCount > 1 && !game->player.evenMoneyTaken) {
            if (handValue > 21 && dealerFavorPush) {
                messageLength = snprintf(handMessage,
                                         sizeof(handMessage),
                                         "Hand %d: push",
                                         handIndex + 1);
            } else if (handValue > 21) {
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

    applyRoundRewards(game, resultStyle);

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

    applyRoundRecovery(game);

    game->tippedThisRound = 0;
    game->player.insuranceBet = 0;
    game->player.evenMoneyTaken = 0;

    {
        char breakdown[256];

        if (ctx.evenMoneyBefore) {
            snprintf(breakdown, sizeof(breakdown),
                     "Even money | bet %d paid 1:1",
                     game->player.bets[0]);
        } else if (payoutTotal > 0 && lossTotal > 0) {
            snprintf(breakdown, sizeof(breakdown),
                     "Won %lld, lost %lld",
                     payoutTotal, lossTotal);
        } else if (payoutTotal > 0) {
            char modifiers[160];
            buildModifierList(game, ctx.winStreakBefore,
                              ctx.roundHadBlackjack, 1,
                              modifiers, sizeof(modifiers));
            snprintf(breakdown, sizeof(breakdown),
                     "Won %lld | modifiers: %s | fatigue %d%%",
                     payoutTotal, modifiers, currentFatigue);
        } else if (lossTotal > 0) {
            char modifiers[160];
            buildModifierList(game, ctx.winStreakBefore,
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
                 sizeof(game->lastPayoutBreakdown),
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
    game->roundInProgress = 0;
    return resultStyle;
}
