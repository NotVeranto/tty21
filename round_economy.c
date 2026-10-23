#include "round_economy.h"

#include <limits.h>
#include <stdio.h>

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

void roundSettleInsurance(Game *game) {
    settleInsurance(game);
}

int applyLoss(Game *game, long long lossValue) {
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

void computeAdvanceForecast(const Game *game,
                            AdvanceForecast *out) {
    out->nextMinBet = getMinBet(game, game->run.floor + 1,
                                game->mode);
    out->fee = 2 * out->nextMinBet;
    out->loanDue = 0;
    if (game->loanAmount > 0 &&
        game->loanDueFloor <= game->run.floor + 1) {
        out->loanDue = game->loanAmount;
    }
    out->postChips = game->player.chips -
                     out->fee - out->loanDue;
    if (out->postChips < 0) out->postChips = 0;
    out->affordable =
        game->player.chips >= out->fee +
                              out->nextMinBet +
                              out->loanDue;
}
