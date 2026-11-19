#include "round_simulation.h"

#include "round.h"
#include "round_actions.h"
#include "round_dealer.h"
#include "round_settlement.h"
#include "round_economy.h"

#include <stddef.h>
#include <string.h>

#define SIMULATION_ACTION_LIMIT 256

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

    while (result.rounds < maxRounds && game->state == STATE_BET) {
        minimumBet = getMinBet(game, game->run.floor, game->mode);
        if (game->player.chips < minimumBet) break;

        betAmount = betPolicy(game, betContext);
        if (betAmount == 0) break;
        if (betAmount < minimumBet ||
            betAmount > getMaxBet(game) ||
            betAmount > game->player.chips) {
            return 0;
        }

        if (!simulateRound(game, betAmount, 0, 0,
                           roundPolicy, roundContext, &resultStyle)) {
            return 0;
        }

        result.rounds++;
        if (resultStyle == MSG_WIN) result.wins++;
        else if (resultStyle == MSG_LOSS) result.losses++;
        else if (resultStyle == MSG_PUSH) result.pushes++;

        if (game->player.chips > result.peakChips) {
            result.peakChips = game->player.chips;
        }
    }

    result.finalChips = game->player.chips;
    *stats = result;
    return 1;
}
