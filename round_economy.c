#include "round_economy.h"
#include "round_rules.h"

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

int applyLossForSettlement(Game *game, long long lossValue) {
    int chipsBeforeLoss;
    int lossAmount;

    chipsBeforeLoss = game->player.chips;
    if (chipsBeforeLoss <= 0) {
        return 0;
    }

    if (lossValue > chipsBeforeLoss) {
        lossAmount = chipsBeforeLoss;
    } else {
        lossAmount = (int)lossValue;
    }

    game->player.chips -= lossAmount;
    return lossAmount;
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
        remainingChips = chipsBeforeLoss / 2;
        game->player.chips = remainingChips;
        lossAmount = chipsBeforeLoss - remainingChips;
    } else {
        game->player.chips -= lossAmount;
    }

    return lossAmount;
}

int applySurrenderLoss(Game *game, int bet) {
    int loss;

    if (bet <= 0) {
        return 0;
    }

    loss = bet / 2 + bet % 2;
    return applyLoss(game, loss);
}

void computeAdvanceForecast(const Game *game,
                            AdvanceForecast *out) {
    Game nextFloor = *game;

    nextFloor.run.floor++;
    nextFloor.run.twistId = TWIST_NONE;
    nextFloor.run.extraTwistCount = 0;
    nextFloor.run.twistBlocked = 0;
    nextFloor.minBetMultiplier = 1;

    out->nextMinBet = getMinBet(&nextFloor, nextFloor.run.floor,
                                game->mode);
    out->fee = 2 * out->nextMinBet;
    out->loanDue = 0;
    if (game->loanAmount > 0 &&
        game->loanDueFloor <= game->run.floor + 1) {
        out->loanDue = game->loanAmount;
    }
    {
        int floorIncome = game->player.perkOwned[PERK_BANKROLL] ? 5 : 0;
        out->postChips = game->player.chips - out->fee -
                         out->loanDue + floorIncome;
    }
    if (out->postChips < 0) out->postChips = 0;
    out->affordable =
        (long long)game->player.chips +
            (game->player.perkOwned[PERK_BANKROLL] ? 5LL : 0LL) >=
        (long long)out->fee + out->nextMinBet + out->loanDue;
}

int getMaxBet(const Game *game) {
    int maxBet;
    int cap;

    maxBet = game->player.chips;
    if (game->mode != MODE_ROGUELIKE) {
        return maxBet;
    }

    cap = game->player.chips;
    if (game->run.floor >= 12) {
        cap /= 3;
    } else if (game->run.floor >= 8) {
        cap /= 2;
    }

    return cap;
}

int getMinBet(const Game *game, int floor, GameMode mode) {
    int base;

    if (mode == MODE_CLASSIC) {
        base = 1;
    } else {
        int halfFloor = floor / 2;

        base = 2 * (1 + halfFloor * halfFloor);
    }

    if (twistActive(game, TWIST_GREED)) {
        base *= 2;
    }

    base *= game->minBetMultiplier;

    return base;
}
