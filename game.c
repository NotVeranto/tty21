#include "game.h"
#include "round.h"
#include "round_dealer.h"
#include "round_settlement.h"
#include "round_actions.h"
#include "render.h"
#include "menu.h"
#include "rng.h"
#include "save.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "input.h"
#include "round_economy.h"

void initGame(Game *game, GameMode mode) {
    initDeck(&game->deck);
    shuffleDeck(&game->deck);
    memset(game->player.hands, 0, sizeof(game->player.hands));
    memset(game->player.bets, 0, sizeof(game->player.bets));
    memset(game->player.isSplitAce, 0, sizeof(game->player.isSplitAce));
    game->player.handCount = 1;
    game->player.activeHand = 0;
    resetHand(&game->dealer.hands[0]);
    resetHand(&game->dealer.hands[1]);
    game->dealer.handCount = 1;
    game->player.chips = 100;
    game->state = STATE_BET;
    game->roundNumber = 0;
    game->roundInProgress = 0;
    game->mode = mode;
    initRun(&game->run);

    if (mode == MODE_CLASSIC) {
        game->run.dealerId = DEALER_CROUPIER;
        game->run.previousDealerId = DEALER_CROUPIER;
        game->run.twistId = TWIST_NONE;
        game->run.previousTwistId = TWIST_NONE;
    }

    game->peekActive = 0;
    game->fastMode = 0;
    game->fatigueTier = 0;
    game->dealerEnraged = 0;
    game->winStreak = 0;
    game->lossStreak = 0;
    game->loanAmount = 0;
    game->loanDueFloor = 0;
    game->freePeekReady = 0;
    game->minBetMultiplier = 1;
    game->safetyNetArmed = 0;
    game->tippedThisRound = 0;
    game->lastPayoutBreakdown[0] = '\0';
    game->rngSeed = rngGetSeed();
    game->saveEnabled = 1;
    game->lastAutosavedRound = 0;
    game->playerQuit = 0;
    game->roundHadNaturalBj = 0;
    game->bossRuleDisabled = 0;
    game->bossRulePaid = 0;
    game->totalFloorsReached = 0;
    game->totalBossesDefeated = 0;
    game->totalMarksEarned = 0;
    memset(game->player.perkOwned, 0, sizeof(game->player.perkOwned));
    memset(game->player.perkCharges, 0, sizeof(game->player.perkCharges));
    memset(game->player.usedDouble, 0, sizeof(game->player.usedDouble));
    memset(game->player.usedSplit, 0, sizeof(game->player.usedSplit));
    game->player.surrendered = 0;
    game->player.insuranceBet = 0;
    game->player.evenMoneyTaken = 0;
    game->log.count = 0;

    {
        int lineIndex;

        for (lineIndex = 0; lineIndex < LOG_SIZE; lineIndex++) {
            game->log.lines[lineIndex][0] = '\0';
        }
    }

    if (mode == MODE_ROGUELIKE) {
        generateShopPool(game);
    }
}

int isPerkLockedThisFloor(const Game *game, PerkId perkId) {
    if (perkId < PERK_SECOND_WIND || perkId >= PERK_COUNT) {
        return 0;
    }

    return game->run.perkLockedUsed &&
           game->run.floor >= 10 &&
           game->run.perkLockedId == (int)perkId &&
           !game->player.perkOwned[perkId];
}

void generateShopPool(Game *game) {
    PerkId candidates[PERK_COUNT];
    int candidateCount;
    int i;
    int j;

    candidateCount = 0;
    for (i = 0; i < PERK_COUNT; i++) {
        if (perkIsSold((PerkId)i) &&
            !game->player.perkOwned[i] &&
            !isPerkLockedThisFloor(game, (PerkId)i)) {
            candidates[candidateCount++] = (PerkId)i;
        }
    }

    for (i = candidateCount - 1; i > 0; i--) {
        PerkId tmp;
        j = rngRange(0, i);
        tmp = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = tmp;
    }

    game->run.shopPerkCount =
        candidateCount < SHOP_POOL_SIZE
            ? candidateCount
            : SHOP_POOL_SIZE;

    for (i = 0; i < game->run.shopPerkCount; i++) {
        game->run.shopPerks[i] = candidates[i];
    }
}

int canRerollShopPool(const Game *game) {
    int perkIndex;
    int shopIndex;
    int alreadyOffered;

    for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
        if (!perkIsSold((PerkId)perkIndex) ||
            game->player.perkOwned[perkIndex] ||
            isPerkLockedThisFloor(game, (PerkId)perkIndex)) {
            continue;
        }

        alreadyOffered = 0;
        for (shopIndex = 0; shopIndex < game->run.shopPerkCount;
             shopIndex++) {
            if (game->run.shopPerks[shopIndex] == (PerkId)perkIndex) {
                alreadyOffered = 1;
                break;
            }
        }

        if (!alreadyOffered) {
            return 1;
        }
    }

    return 0;
}

int rerollShopPool(Game *game) {
    PerkId candidates[PERK_COUNT];
    int candidateCount;
    int perkIndex;
    int shopIndex;
    int alreadyOffered;
    int i;
    int j;

    candidateCount = 0;
    for (perkIndex = 0; perkIndex < PERK_COUNT; perkIndex++) {
        if (!perkIsSold((PerkId)perkIndex) ||
            game->player.perkOwned[perkIndex] ||
            isPerkLockedThisFloor(game, (PerkId)perkIndex)) {
            continue;
        }

        alreadyOffered = 0;
        for (shopIndex = 0; shopIndex < game->run.shopPerkCount;
             shopIndex++) {
            if (game->run.shopPerks[shopIndex] == (PerkId)perkIndex) {
                alreadyOffered = 1;
                break;
            }
        }

        if (!alreadyOffered) {
            candidates[candidateCount++] = (PerkId)perkIndex;
        }
    }

    if (candidateCount == 0) {
        return 0;
    }

    for (i = candidateCount - 1; i > 0; i--) {
        PerkId temporary;

        j = rngRange(0, i);
        temporary = candidates[i];
        candidates[i] = candidates[j];
        candidates[j] = temporary;
    }

    game->run.shopPerkCount =
        candidateCount < SHOP_POOL_SIZE ? candidateCount : SHOP_POOL_SIZE;
    for (i = 0; i < game->run.shopPerkCount; i++) {
        game->run.shopPerks[i] = candidates[i];
    }

    return 1;
}

void recoverInterruptedRound(Game *game) {
    if (!game->roundInProgress) return;

    (void)applyLoss(game, game->player.bets[0]);
    logPush(game, "You quit mid-round. Bet forfeited.", MSG_LOSS);
    game->roundInProgress = 0;
    game->state = STATE_BET;
}

int purchasePerk(Game *game, int perkIndex) {
    char message[128];

    if (perkIndex < PERK_SECOND_WIND || perkIndex >= PERK_COUNT) {
        return 0;
    }

    if (isPerkLockedThisFloor(game, (PerkId)perkIndex)) {
        logPush(game, "Perk is disabled this floor.", MSG_DEFAULT);
        return 0;
    }

    if (game->player.perkOwned[perkIndex]) {
        logPush(game, "Already owned.", MSG_DEFAULT);
        return 0;
    }

    if (game->run.marks < perkCost((PerkId)perkIndex)) {
        logPush(game, "Not enough marks.", MSG_DEFAULT);
        return 0;
    }

    game->run.marks -= perkCost((PerkId)perkIndex);
    game->player.perkOwned[perkIndex] = 1;

    if (perkIndex == PERK_PEEK ||
        perkIndex == PERK_SECOND_WIND ||
        perkIndex == PERK_INSURANCE_PERK ||
        perkIndex == PERK_DEALERS_FAVOR ||
        perkIndex == PERK_MARKED_CARD ||
        perkIndex == PERK_EXTRA_LIFE) {
        game->player.perkCharges[perkIndex] = 1;
    }

    snprintf(message, sizeof(message), "%s purchased.",
             perkName((PerkId)perkIndex));
    logPush(game, message, MSG_WIN);
    return 1;
}

int canPurchaseShopService(const Game *game,
                            char serviceId,
                            int currentMinBet) {
    if (serviceId == 'v' || serviceId == 't' || serviceId == 'n') {
        int cost = serviceId == 'n' ? 50 : 10;

        return game->player.chips >= cost &&
               (long long)game->player.chips - cost >= currentMinBet &&
               !(serviceId == 'n' && game->safetyNetArmed);
    }

    if (serviceId == 'z') {
        return !game->run.shopPerksRerolled &&
               game->run.marks >= 10 &&
               canRerollShopPool(game);
    }

    if (serviceId == 'l') {
        return game->loanAmount == 0 &&
               game->player.chips >= getMinBet(
                   game, game->run.floor + 1, game->mode);
    }

    if (serviceId == 'r') {
        return game->run.floor >= 4 &&
               !game->run.twistRerolled &&
               game->run.marks >= 30;
    }

    if (serviceId == 'b') {
        return game->run.floor >= 4 &&
               !game->run.twistBlocked &&
               game->run.twistId != TWIST_NONE &&
               game->run.marks >= 40;
    }

    return 0;
}

int purchaseShopService(Game *game,
                        char serviceId,
                        int currentMinBet) {
    char message[128];

    if (!canPurchaseShopService(game, serviceId, currentMinBet)) {
        logPush(game, "Item unavailable.", MSG_DEFAULT);
        return 0;
    }

    if (serviceId == 'v' || serviceId == 'n' || serviceId == 't') {
        int cost = serviceId == 'n' ? 50 : 10;

        game->player.chips -= cost;
        if (serviceId == 'v') {
            game->run.marks += 2;
            game->totalMarksEarned += 2;
            logPush(game, "Converted 10 chips to 2 marks.", MSG_INFO);
        } else if (serviceId == 'n') {
            game->safetyNetArmed = 1;
            logPush(game, "Safety net armed. Next wipe leaves 20 chips.",
                    MSG_INFO);
        } else {
            game->tippedThisRound = 1;
            logPush(game, "Dealer tipped. Hole card visible next round.",
                    MSG_INFO);
        }
        return 1;
    }

    if (serviceId == 'z') {
        if (!rerollShopPool(game)) {
            logPush(game, "No new perks available.", MSG_DEFAULT);
            return 0;
        }
        game->run.marks -= 10;
        game->run.shopPerksRerolled = 1;
        logPush(game, "Shop rerolled. -10 marks.", MSG_INFO);
        return 1;
    }

    if (serviceId == 'l') {
        creditChips(game, 50);
        game->loanAmount = 75;
        game->loanDueFloor = game->run.floor + 3;
        snprintf(message, sizeof(message),
                 "Loan taken: +50 chips, repay 75 by floor %d.",
                 game->loanDueFloor);
        logPush(game, message, MSG_INFO);
        return 1;
    }

    if (serviceId == 'r') {
        game->run.twistId = selectTwistForFloor(game);
        game->run.twistRerolled = 1;
        game->run.marks -= 30;
        logPush(game, "Twist rerolled.", MSG_INFO);
        return 1;
    }

    if (serviceId == 'b') {
        game->run.twistBlocked = 1;
        game->run.marks -= 40;
        logPush(game, "Twist blocked this floor.", MSG_INFO);
        return 1;
    }

    return 0;
}

void logPush(Game *game, const char *msg, MessageStyle style) {
    int i;

    if (game->log.count < LOG_SIZE) {
        snprintf(game->log.lines[game->log.count],
                 LOG_LINE_LEN,
                 "%s",
                 msg);
        game->log.styles[game->log.count] = style;
        game->log.count++;
    } else {
        for (i = 0; i < LOG_SIZE - 1; i++) {
            memcpy(game->log.lines[i],
                   game->log.lines[i + 1],
                   LOG_LINE_LEN);
            game->log.styles[i] = game->log.styles[i + 1];
        }

        snprintf(game->log.lines[LOG_SIZE - 1],
                 LOG_LINE_LEN,
                 "%s",
                 msg);
        game->log.styles[LOG_SIZE - 1] = style;
    }
}


void advanceFloor(Game *game) {
    int bonus;
    int earned;
    char message[128];

    if (game->mode == MODE_CLASSIC) {
        return;
    }

    bonus = (game->player.chips - 50) / 25;

    if (bonus < 0) {
        bonus = 0;
    }
    if (bonus > 5) {
        bonus = 5;
    }

    earned = 4 + game->run.floor + bonus;
    game->run.marks += earned;
    if (earned > 0) {
        game->totalMarksEarned += earned;
    }
    game->totalFloorsReached++;
    game->run.previousDealerId = game->run.dealerId;
    game->run.previousTwistId = game->run.twistId;
    game->run.floor++;

    if (game->loanAmount > 0 &&
        game->run.floor >= game->loanDueFloor) {
        if (game->player.chips >= game->loanAmount) {
            game->player.chips -= game->loanAmount;
            logPush(game, "Loan repaid: -75 chips.", MSG_INFO);
            game->loanAmount = 0;
            game->loanDueFloor = 0;
        } else {
            logPush(game, "Loan defaulted. Run over.", MSG_LOSS);
            game->state = STATE_QUIT;
            return;
        }
    }

    game->run.twistRerolled = 0;
    game->run.bossRewarded = 0;
    game->run.twistBlocked = 0;
    game->safetyNetArmed = 0;
    game->run.roundsOnFloor = 0;
    game->run.extraTwistCount = 0;
    game->fatigueTier = 0;
    game->dealerEnraged = 0;
    game->run.dealerId = selectDealerForFloor(game->run.floor,
                                          game->run.previousDealerId);
    game->run.shopPerksRerolled = 0;
    generateShopPool(game);

    if (game->run.floor >= 5 &&
        game->run.floor % 5 == 0 &&
        dealerIsBoss(game->run.dealerId)) {
        snprintf(message,
                 sizeof(message),
                 "BOSS FLOOR. %s awaits.",
                 getDealerInfo(game->run.dealerId)->name);
        logPush(game, message, MSG_LOSS);
    }

    // Restore a perk locked on a previous floor before charge refills.
    if (isPerkLockedThisFloor(game, (PerkId)game->run.perkLockedId)) {
        game->player.perkOwned[game->run.perkLockedId] = 1;
        game->run.perkLockedId = -1;
    }

    if (game->player.perkOwned[PERK_PEEK]) {
        game->player.perkCharges[PERK_PEEK] = 1;
    }

    if (game->player.perkOwned[PERK_INSURANCE_PERK]) {
        game->player.perkCharges[PERK_INSURANCE_PERK] = 1;
    }

    if (game->player.perkOwned[PERK_DEALERS_FAVOR]) {
        game->player.perkCharges[PERK_DEALERS_FAVOR] = 1;
    }

    {
        int attempts;
        game->run.twistId = selectTwistForFloor(game);
        if (!twistIsForcedHostile(game)) {
            attempts = 0;
            while (game->player.perkOwned[PERK_SIXTH_SENSE] &&
                   game->run.floor >= 4 &&
                   getTwistInfo(game->run.twistId)->category == TWIST_CAT_HOSTILE &&
                   attempts < 10) {
                game->run.twistId = selectTwistForFloor(game);
                attempts++;
            }
            if (game->player.perkOwned[PERK_SIXTH_SENSE] &&
                game->run.floor >= 4 &&
                getTwistInfo(game->run.twistId)->category == TWIST_CAT_HOSTILE) {
                static const TwistId FAVORITES[] = {TWIST_LUCKY_DRAW, TWIST_CHARITY,
                    TWIST_SNIPER, TWIST_JACKPOT, TWIST_BUST_INSURANCE, TWIST_GREED};
                static const TwistId NEUTRALS[] = {TWIST_SIX_DECK, TWIST_NO_RESHUFFLE,
                    TWIST_ACES_LOW, TWIST_HIGH_STAKES};
                int i, replaced = 0;
                for (i = 0; i < (int)(sizeof(FAVORITES)/sizeof(FAVORITES[0])); i++) {
                    if (FAVORITES[i] != game->run.previousTwistId) {
                        game->run.twistId = FAVORITES[i]; replaced = 1; break;
                    }
                }
                if (!replaced) {
                    for (i = 0; i < (int)(sizeof(NEUTRALS)/sizeof(NEUTRALS[0])); i++) {
                        if (NEUTRALS[i] != game->run.previousTwistId) {
                            game->run.twistId = NEUTRALS[i]; break;
                        }
                    }
                }
            }
        }
    }

    game->freePeekReady = 0;
    game->minBetMultiplier = 1;

    if (rngRange(0, 99) < 25) {
        int eventId;

        eventId = rngRange(0, 3);

        if (eventId == 0) {
            game->run.marks += 25;
            game->totalMarksEarned += 25;
            logPush(game,
                    "Floor event: Mark Cache (+25 marks).",
                    MSG_INFO);
        } else if (eventId == 1) {
            game->player.perkCharges[PERK_PEEK] = 1;

            if (game->player.perkOwned[PERK_PEEK]) {
                logPush(game,
                        "Floor event: Free Peek (charge refilled).",
                        MSG_INFO);
            } else {
                game->freePeekReady = 1;
                logPush(game,
                        "Floor event: Free Peek (one free peek this floor).",
                        MSG_INFO);
            }
        } else if (eventId == 2) {
            game->run.twistBlocked = 1;
            logPush(game,
                    "Floor event: Dealer Lapse (twist forgotten).",
                    MSG_INFO);
        } else {
            int newMinBet;

            game->minBetMultiplier = 2;
            logPush(game,
                    "Floor event: Tight Table (minimum bet doubled).",
                    MSG_INFO);
            newMinBet = getMinBet(game, game->run.floor, game->mode);
            if (game->player.chips < newMinBet &&
                game->player.chips > 0) {
                game->minBetMultiplier = 1;
                logPush(game,
                        "Tight Table skipped: cannot cover.",
                        MSG_INFO);
            }
        }
    }

    if (game->player.perkOwned[PERK_BANKROLL]) {
        creditChips(game, 5);
        logPush(game, "Bankroll: +5 chips.", MSG_WIN);
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

    // Trigger run-limited floor hazards before the floor summary.
    if (!game->run.coldSnapUsed &&
        game->run.floor >= 8 &&
        rngRange(0, 99) < 25) {
        game->run.coldSnapUsed = 1;
        game->run.coldSnapActive = 1;
        logPush(game,
                "The deck runs cold. Next hand is low.",
                MSG_LOSS);
    }

    if (!game->run.perkLockedUsed &&
        game->run.floor >= 10) {
        PerkId candidates[PERK_COUNT];
        int count = 0;
        int i;
        int lockRoll;

        for (i = 0; i < PERK_COUNT; i++) {
            if (game->player.perkOwned[i]) {
                candidates[count++] = (PerkId)i;
            }
        }

        lockRoll = rngRange(0, 99);
        if (count > 0 && lockRoll < 25) {
            PerkId locked = candidates[rngRange(0, count - 1)];
            char lockedMsg[128];

            game->run.perkLockedUsed = 1;
            game->player.perkOwned[locked] = 0;
            game->run.perkLockedId = (int)locked;
            snprintf(lockedMsg, sizeof(lockedMsg),
                     "%s disabled this floor.",
                     perkName(locked));
            logPush(game, lockedMsg, MSG_LOSS);
        }
    }

    snprintf(message,
             sizeof(message),
             "Floor %d. Marks: %d. Minimum bet: %d.",
             game->run.floor,
             game->run.marks,
             getMinBet(game, game->run.floor, game->mode));
    logPush(game, message, MSG_DEFAULT);

    renderFloorTransition(game);
    renderPause(game, 1200);
    flushInput();

    if (dealerIsBoss(game->run.dealerId)) {
        game->state = STATE_BOSS_PREP;
        game->bossRuleDisabled = 0;
        game->bossRulePaid = game->run.floor * 10;
    } else {
        game->state = STATE_BET;
    }
}

int advanceToNextFloor(Game *game) {
    AdvanceForecast forecast;
    char feeMsg[64];

    if (game->mode != MODE_ROGUELIKE) {
        return 0;
    }

    computeAdvanceForecast(game, &forecast);
    if (!forecast.affordable) {
        return 0;
    }

    game->player.chips -= forecast.fee;
    snprintf(feeMsg, sizeof(feeMsg),
             "Advance cost: -%d chips.", forecast.fee);
    logPush(game, feeMsg, MSG_LOSS);
    advanceFloor(game);

    if (game->state != STATE_QUIT &&
        game->state != STATE_BOSS_PREP) {
        game->state = STATE_BET;
    }

    return 1;
}

void bossPrepTurn(Game *game) {
    KeyEvent event;
    int answer;
    renderBossPrep(game);
    answer = EOF;
    if (inputIsRaw()) {
        event = readKey();
        if (event.type == KEY_CHAR) answer = tolower((unsigned char)event.ch);
    } else {
        answer = getchar();
        if (answer != EOF) {
            answer = tolower((unsigned char)answer);
            while (answer != '\n' && getchar() != '\n' && !feof(stdin)) {
            }
        }
    }
    if (answer == 'y' && game->run.marks >= game->bossRulePaid) {
        game->run.marks -= game->bossRulePaid;
        game->bossRuleDisabled = 1;
    }
    game->state = STATE_BET;
}

int runGame(Game *game) {
    char finalMsg[64];

    while (game->state != STATE_QUIT) {
        if (game->mode == MODE_ROGUELIKE &&
            game->saveEnabled &&
            (game->state == STATE_BET ||
             game->state == STATE_MENU)) {
            game->rngCalls = rngGetCallCount();
            if (saveGame(game) == 0) {
                game->lastAutosavedRound = game->roundNumber;
            }
        }
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
        } else if (game->state == STATE_BOSS_PREP) {
            bossPrepTurn(game);
        } else if (game->state == STATE_MENU) {
            if (game->mode == MODE_CLASSIC) {
                game->state = STATE_BET;
            } else {
                floorMenu(game);
            }
        }
    }

    if (game->mode == MODE_ROGUELIKE &&
        game->saveEnabled &&
        !game->playerQuit) {
        deleteSave();
    }

    snprintf(finalMsg,
             sizeof(finalMsg),
             "Game over. Final chips: %d.",
             game->player.chips);
    logPush(game, finalMsg, MSG_DEFAULT);
    renderTable(game, 0);
    renderShowCursor();

    return 0;
}
