#define _DEFAULT_SOURCE
#include "round.h"
#include "round_dealer.h"
#include "round_cards.h"
#include "round_economy.h"
#include "round_rules.h"
#include "round_input.h"
#include "render_table.h"
#include "input.h"
#include "input_map.h"
#include "rng.h"
#include "save.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DECK_START_RESERVE 6
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
    if (game->deck.top >= DECK_SIZE - DECK_BURN_SIZE) return;

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
        if (game->deck.top >= DECK_SIZE - DECK_BURN_SIZE) break;

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
    // Stage 16a peek/insurance/even-money paths have no unit tests yet. Stage 16.6 will add coverage; validate these paths manually meanwhile.
    int playerValue;
    int betAmount;
    int upcardRank;
    int upcardIsAce;
    int upcardIsTen;
    int dealerBJ;
    int playerBJ;

    game->peekActive = 0;

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

    if (twistActive(game, TWIST_SIX_DECK)) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
    } else if (!twistActive(game, TWIST_NO_RESHUFFLE) &&
               game->deck.top >= DECK_SIZE - DECK_BURN_SIZE - DECK_START_RESERVE) {
        initDeck(&game->deck);
        shuffleDeck(&game->deck);
        logPush(game, "Deck reshuffled.", MSG_INFO);
    }

    game->player.bets[game->player.activeHand] = betAmount;
    memset(game->player.usedDouble, 0, sizeof(game->player.usedDouble));
    memset(game->player.usedSplit, 0, sizeof(game->player.usedSplit));
    game->player.insuranceBet = 0;
    game->player.evenMoneyTaken = 0;
    game->player.surrendered = 0;
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
    if (!game->fastMode && isatty(STDOUT_FILENO)) {
        renderTable(game, 1);
        renderPause(game, 150);
    }

    drawCard(game, &game->dealer.hands[0], 0);
    if (!game->fastMode && isatty(STDOUT_FILENO)) {
        renderTable(game, 1);
        renderPause(game, 150);
    }

    drawCard(game, &game->player.hands[0], 1);
    if (!game->fastMode && isatty(STDOUT_FILENO)) {
        renderTable(game, 1);
        renderPause(game, 150);
    }

    drawCard(game, &game->dealer.hands[0], 0);
    if (!game->fastMode && isatty(STDOUT_FILENO)) {
        renderTable(game, 1);
        renderPause(game, 150);
    }
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
                if (game->deck.top >= DECK_SIZE - DECK_BURN_SIZE) break;
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

    game->roundInProgress = 1;
    if (game->mode == MODE_ROGUELIKE && game->saveEnabled) {
        game->rngCalls = rngGetCallCount();
        (void)saveGame(game);
    }

    upcardRank = game->dealer.hands[0].cards[0].rank;
    upcardIsAce = upcardRank == ACE;
    upcardIsTen = upcardRank >= TEN;

    playerValue = calculatePlayerHandValue(game);
    game->roundHadNaturalBj = 0;
    if (game->player.handCount == 1 &&
        game->player.hands[0].count == 2 &&
        playerValue == 21 &&
        !game->player.usedSplit[0]) {
        game->roundHadNaturalBj = 1;
    }
    playerBJ = game->roundHadNaturalBj;

    if (upcardIsAce &&
        !twistActive(game, TWIST_ACES_LOW) &&
        !effectiveDealerAcesCountAsOne(game, game->run.dealerId)) {
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
        game->state = game->dealer.handCount > 1
                          ? STATE_DEALER_TURN : STATE_RESULT;
        return;
    }

    if (playerValue >= 21) {
        game->state = game->dealer.handCount > 1
                          ? STATE_DEALER_TURN : STATE_RESULT;
        return;
    }

    if (dealerForcesDouble(game->run.dealerId) &&
        game->run.floor >= 10) {
        if (game->player.hands[0].count == 2 &&
            (long long)game->player.chips >=
                (long long)game->player.bets[0] * 2) {

            game->player.bets[0] *= 2;
            game->player.usedDouble[0] = 1;
            drawCard(game, &game->player.hands[0], 1);

            if (effectiveDealerMirrorsOnHit(game)) {
                drawCard(game, &game->dealer.hands[0], 0);
            }

            logPush(game, "Devil forces a double down.", MSG_DEALER);
            game->state = STATE_DEALER_TURN;
            return;
        }
    }

    flushInput();
    game->state = STATE_PLAYER_TURN;
}


