#include "round_simulation.h"

#include "round.h"
#include "round_actions.h"
#include "round_dealer.h"
#include "round_settlement.h"

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
