#include "round_simulation.h"

#include "round.h"
#include "round_actions.h"
#include "round_dealer.h"
#include "round_settlement.h"
#include "rng.h"
#include "round_economy.h"
#include "game.h"

#include <stddef.h>
#include <string.h>

#define SIMULATION_ACTION_LIMIT 256

int simulationBalancedShopPolicy(const Game *game,
                                 int currentMinBet,
                                 void *context) {
    static const PerkId priority[] = {
        PERK_SURVIVAL,
        PERK_EXTRA_LIFE,
        PERK_DEALERS_FAVOR,
        PERK_BANKROLL,
        PERK_COMPOUNDING,
        PERK_DOUBLE_DIP,
        PERK_INSURANCE_PERK,
        PERK_MARKED_CARD,
        PERK_PEEK,
        PERK_SECOND_WIND,
        PERK_SPLIT_MASTERY,
        PERK_SIXTH_SENSE,
        PERK_ACE_HIGH
    };
    char services[7];
    int serviceCount;
    int index;
    int offerIndex;
    int perkOffered;
    int nextPerkCost;
    (void)context;

    if (game == NULL || game->state != STATE_SHOP) return 0;

    for (index = 0; index < (int)(sizeof(priority) / sizeof(priority[0]));
         index++) {
        PerkId perk = priority[index];
        perkOffered = 0;
        for (offerIndex = 0; offerIndex < game->run.shopPerkCount;
             offerIndex++) {
            if (game->run.shopPerks[offerIndex] == perk) {
                perkOffered = 1;
                break;
            }
        }
        if (!perkOffered || game->player.perkOwned[perk] ||
            isPerkLockedThisFloor(game, perk)) continue;
        if (game->run.marks >= perkCost(perk)) return (int)perk + 1;

        nextPerkCost = perkCost(perk);
        serviceCount = getShopServiceOffers(game, services, 7);
        for (offerIndex = 0; offerIndex < serviceCount; offerIndex++) {
            if (services[offerIndex] == 'v' &&
                game->run.marks < nextPerkCost &&
                canPurchaseShopService(game, 'v', currentMinBet) &&
                game->player.chips >= currentMinBet + 10) {
                return -(int)'v';
            }
        }
        continue;
    }

    serviceCount = getShopServiceOffers(game, services, 7);
    for (index = 0; index < serviceCount; index++) {
        if (services[index] == 'n' &&
            game->player.chips < currentMinBet * 2 &&
            canPurchaseShopService(game, 'n', currentMinBet)) {
            return -(int)'n';
        }
    }
    return 0;
}

int simulateRound(Game *game,
                  int betAmount,
                  int insuranceDecision,
                  int evenMoneyDecision,
                  RoundPolicy policy,
                  void *policyContext,
                  MessageStyle *resultStyle) {
    int actionCount;
    int actionIndex;
    int step;
    int actionAvailable;
    ActionId actions[ACTION_COUNT + 1];
    ActionId action;

    if (game == NULL || policy == NULL ||
        !startRoundWithBet(game, betAmount,
                           insuranceDecision, evenMoneyDecision)) {
        return 0;
    }

    for (step = 0; step < SIMULATION_ACTION_LIMIT; step++) {
        if (game->state == STATE_PLAYER_TURN) {
            actionCount = getPlayerActions(game, actions);
            if (actionCount <= 0) return 0;

            action = policy(game, actions, actionCount, policyContext);
            actionAvailable = 0;
            for (actionIndex = 0; actionIndex < actionCount; actionIndex++) {
                if (actions[actionIndex] == action) {
                    actionAvailable = 1;
                    break;
                }
            }
            if (!actionAvailable) return 0;
            applyPlayerAction(game, action);
            if (game->state == STATE_QUIT) return 2;
        } else if (game->state == STATE_DEALER_TURN) {
            dealerTurn(game);
        } else if (game->state == STATE_RESULT) {
            MessageStyle style = resolveRound(game);
            if (resultStyle != NULL) *resultStyle = style;
            return 1;
        } else {
            return 0;
        }
    }

    return 0;
}

int simulateClassicRun(Game *game,
                       int maxRounds,
                       SimulationBetPolicy betPolicy,
                       void *betContext,
                       RoundPolicy roundPolicy,
                       void *roundContext,
                       SimulationRunStats *stats) {
    SimulationRunStats result;
    int betAmount;
    int minimumBet;
    MessageStyle resultStyle;

    if (game == NULL || stats == NULL || betPolicy == NULL ||
        roundPolicy == NULL || game->mode != MODE_CLASSIC ||
        game->state != STATE_BET || maxRounds <= 0) {
        return 0;
    }

    memset(&result, 0, sizeof(result));
    result.startingChips = game->player.chips;
    result.peakChips = game->player.chips;
    result.stopReason = SIMULATION_STOP_MAX_ROUNDS;

    while (result.rounds < maxRounds && game->state == STATE_BET) {
        minimumBet = getMinBet(game, game->run.floor, game->mode);
        if (game->player.chips < minimumBet ||
            getMaxBet(game) < minimumBet) {
            result.stopReason = SIMULATION_STOP_INSUFFICIENT_BANKROLL;
            break;
        }

        betAmount = betPolicy(game, betContext);
        if (betAmount == 0) {
            result.stopReason = SIMULATION_STOP_POLICY;
            break;
        }
        if (betAmount < minimumBet ||
            betAmount > getMaxBet(game) ||
            betAmount > game->player.chips) {
            return 0;
        }

        {
            int roundResult = simulateRound(
                game, betAmount, 0, 0, roundPolicy, roundContext,
                &resultStyle);
            if (roundResult == 0) return 0;
            if (roundResult == 2) {
                result.stopReason = SIMULATION_STOP_GAME_QUIT;
                break;
            }
        }

        result.rounds++;
        if (resultStyle == MSG_WIN) result.wins++;
        else if (resultStyle == MSG_LOSS) result.losses++;
        else if (resultStyle == MSG_PUSH) result.pushes++;

        if (game->player.chips > result.peakChips) {
            result.peakChips = game->player.chips;
        }
    }

    if (game->state == STATE_QUIT) {
        result.stopReason = SIMULATION_STOP_GAME_QUIT;
    }
    result.finalChips = game->player.chips;
    *stats = result;
    return 1;
}

int simulateRoguelikeRun(Game *game,
                         int maxRounds,
                         SimulationBetPolicy betPolicy,
                         void *betContext,
                         RoundPolicy roundPolicy,
                         void *roundContext,
                         SimulationFloorPolicy floorPolicy,
                         void *floorContext,
                         SimulationBossPrepPolicy bossPrepPolicy,
                         void *bossPrepContext,
                         SimulationShopPolicy shopPolicy,
                         void *shopContext,
                         SimulationRunStats *stats) {
    SimulationRunStats result;
    MessageStyle resultStyle;
    int betAmount;
    int minimumBet;
    int shopAction;
    int shopStep;
    char floorAction;
    FloorMenuActionResult menuResult;

    if (game == NULL || stats == NULL || betPolicy == NULL ||
        roundPolicy == NULL || floorPolicy == NULL ||
        game->mode != MODE_ROGUELIKE || game->state != STATE_BET ||
        maxRounds <= 0) {
        return 0;
    }

    memset(&result, 0, sizeof(result));
    result.startingChips = game->player.chips;
    result.peakChips = game->player.chips;
    result.floorsReached = game->totalFloorsReached;
    result.bossesDefeated = game->totalBossesDefeated;
    result.marksEarned = game->totalMarksEarned;
    result.stopReason = SIMULATION_STOP_MAX_ROUNDS;

    while (1) {
        if (game->state == STATE_BOSS_PREP) {
            int disableFirstRule = bossPrepPolicy != NULL
                ? bossPrepPolicy(game, bossPrepContext) : 0;
            completeBossPrep(game, disableFirstRule != 0);
        }

        if (game->state == STATE_SHOP) {
            if (shopPolicy != NULL) {
                for (shopStep = 0; shopStep < PERK_COUNT + 8; shopStep++) {
                    shopAction = shopPolicy(
                        game, getMinBet(game, game->run.floor, game->mode),
                        shopContext);
                    if (shopAction == 0) break;
                    if (shopAction > 0) {
                        if (shopAction > PERK_COUNT ||
                            !purchasePerk(game, shopAction - 1)) break;
                    } else {
                        char serviceId = (char)(-shopAction);
                        if (!purchaseShopService(
                                game, serviceId,
                                getMinBet(game, game->run.floor, game->mode))) {
                            break;
                        }
                    }
                }
            }
            (void)handleShopMenuAction(game, 'q');
            continue;
        }

        if (game->state == STATE_MENU) {
            floorAction = floorPolicy(game, floorContext);
            menuResult = handleFloorMenuAction(
                game, floorAction,
                getMinBet(game, game->run.floor, game->mode));
            if (menuResult != FLOOR_MENU_HANDLED) {
                if (floorAction == 'q' || floorAction == 'Q' ||
                    floorAction == 'a' || floorAction == 'A' ||
                    floorAction == 'c' || floorAction == 'C') {
                    result.stopReason = game->state == STATE_QUIT
                        ? SIMULATION_STOP_GAME_QUIT : SIMULATION_STOP_POLICY;
                    break;
                }
                return 0;
            }
            if (game->state == STATE_QUIT) {
                result.stopReason = SIMULATION_STOP_GAME_QUIT;
                break;
            }
            continue;
        }

        if (game->state != STATE_BET) return 0;
        if (result.rounds >= maxRounds) {
            result.stopReason = SIMULATION_STOP_MAX_ROUNDS;
            break;
        }
        minimumBet = getMinBet(game, game->run.floor, game->mode);
        if (game->player.chips < minimumBet ||
            getMaxBet(game) < minimumBet) {
            result.stopReason = SIMULATION_STOP_INSUFFICIENT_BANKROLL;
            break;
        }

        betAmount = betPolicy(game, betContext);
        if (betAmount == 0) {
            result.stopReason = SIMULATION_STOP_POLICY;
            break;
        }
        if (betAmount < minimumBet ||
            betAmount > getMaxBet(game) ||
            betAmount > game->player.chips) {
            return 0;
        }

        {
            int roundResult = simulateRound(
                game, betAmount, 0, 0, roundPolicy, roundContext,
                &resultStyle);
            if (roundResult == 0) return 0;
            if (roundResult == 2) {
                result.stopReason = SIMULATION_STOP_GAME_QUIT;
                break;
            }
        }

        result.rounds++;
        if (resultStyle == MSG_WIN) result.wins++;
        else if (resultStyle == MSG_LOSS) result.losses++;
        else if (resultStyle == MSG_PUSH) result.pushes++;
        if (game->player.chips > result.peakChips) {
            result.peakChips = game->player.chips;
        }
        if (game->state == STATE_QUIT) {
            result.stopReason = SIMULATION_STOP_GAME_QUIT;
            break;
        }
    }

    result.finalChips = game->player.chips;
    result.floorsReached = game->totalFloorsReached;
    result.bossesDefeated = game->totalBossesDefeated;
    result.marksEarned = game->totalMarksEarned;
    *stats = result;
    return 1;
}

int simulateRoguelikeBatch(const unsigned int *seeds,
                           int seedCount,
                           int maxRoundsPerRun,
                           SimulationBetPolicy betPolicy,
                           void *betContext,
                           RoundPolicy roundPolicy,
                           void *roundContext,
                           SimulationFloorPolicy floorPolicy,
                           void *floorContext,
                           SimulationBossPrepPolicy bossPrepPolicy,
                           void *bossPrepContext,
                           SimulationShopPolicy shopPolicy,
                           void *shopContext,
                           SimulationBatchStats *stats) {
    SimulationBatchStats result;
    SimulationRunStats runStats;
    Game game;
    int index;

    if (seeds == NULL || seedCount <= 0 || maxRoundsPerRun <= 0 ||
        betPolicy == NULL || roundPolicy == NULL || floorPolicy == NULL ||
        stats == NULL) {
        return 0;
    }

    memset(&result, 0, sizeof(result));
    for (index = 0; index < seedCount; index++) {
        rngInit(seeds[index], 1);
        initGame(&game, MODE_ROGUELIKE);
        game.fastMode = 1;
        game.saveEnabled = 0;

        if (!simulateRoguelikeRun(
                &game, maxRoundsPerRun, betPolicy, betContext,
                roundPolicy, roundContext, floorPolicy, floorContext,
                bossPrepPolicy, bossPrepContext,
                shopPolicy, shopContext, &runStats)) {
            return 0;
        }

        result.runsCompleted++;
        result.totalRounds += runStats.rounds;
        result.totalWins += runStats.wins;
        result.totalLosses += runStats.losses;
        result.totalPushes += runStats.pushes;
        result.totalStartingChips += runStats.startingChips;
        result.totalFinalChips += runStats.finalChips;
        result.totalPeakChips += runStats.peakChips;
        if (index == 0 || runStats.finalChips < result.minFinalChips) {
            result.minFinalChips = runStats.finalChips;
        }
        if (index == 0 || runStats.finalChips > result.maxFinalChips) {
            result.maxFinalChips = runStats.finalChips;
        }
        if (runStats.finalChips > runStats.startingChips) {
            result.profitableRuns++;
        }
        if (runStats.finalChips <= 0) {
            result.bankruptRuns++;
        }
        result.totalFloorsReached += runStats.floorsReached;
        result.totalBossesDefeated += runStats.bossesDefeated;
        result.totalMarksEarned += runStats.marksEarned;
        if (runStats.stopReason >= 0 &&
            runStats.stopReason < SIMULATION_STOP_REASON_COUNT) {
            result.stopReasonCounts[runStats.stopReason]++;
        }

        if (index == 0 || runStats.floorsReached <
                          result.shallowestFloorReached) {
            result.shallowestFloorReached = runStats.floorsReached;
        }
        if (runStats.floorsReached > result.deepestFloorReached) {
            result.deepestFloorReached = runStats.floorsReached;
        }
    }

    *stats = result;
    return 1;
}
