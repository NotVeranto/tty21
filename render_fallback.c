#include "render.h"
#include "round_cards.h"
#include "round_rules.h"
#include "dealers.h"
#include "twists.h"

#include <stdio.h>

// Prints the fallback game state for non-full-screen output.
void renderGameStateFallback(const Game *game, int hideDealerCard)
void renderGameStateFallback(const Game *game, int hideDealerCard)
{
    const DealerInfo *dealerInfo;
    const TwistInfo *twistInfo;
    int dealerValues[2];
    int dealerCardHidden;
    int displayHandIndex;
    int playerValue;
    int cardIndex;
    int dealerHandIndex;

    dealerInfo = getDealerInfo(game->run.dealerId);
    twistInfo = getTwistInfo(game->run.twistId);

    for (dealerHandIndex = 0;
         dealerHandIndex < game->dealer.handCount;
         dealerHandIndex++) {
        dealerValues[dealerHandIndex] =
            calculateDealerHandValue(
                         game,
                         &game->dealer.hands[dealerHandIndex],
                         game->run.dealerId);
    }
    dealerCardHidden = hideDealerCard &&
                       !game->peekActive &&
                       !twistActive(game, TWIST_SNIPER);

    displayHandIndex = game->player.activeHand;

    if (displayHandIndex >= game->player.handCount) {
        displayHandIndex = game->player.handCount - 1;
    }

    if (displayHandIndex < 0) {
        displayHandIndex = 0;
    }

    playerValue =
        calculatePlayerHandValueForIndex(game, displayHandIndex);

    printf("Round: %d\n", game->roundNumber);
    printf("Dealer (%s): ", dealerInfo->name);

    for (dealerHandIndex = 0;
         dealerHandIndex < game->dealer.handCount;
         dealerHandIndex++) {
        if (dealerHandIndex > 0) {
            printf(" | ");
        }

        for (cardIndex = 0;
             cardIndex < game->dealer.hands[dealerHandIndex].count;
             cardIndex++) {
            if (dealerCardHidden && cardIndex == 0) {
                printf("??");
            } else {
                printCard(&game->dealer.hands[dealerHandIndex].cards[cardIndex]);
            }

            if (cardIndex < game->dealer.hands[dealerHandIndex].count - 1) {
                printf(" ");
            }
        }
    }

    if (dealerCardHidden || dealerHidesTotal(game->run.dealerId)) {
        if (game->dealer.handCount == 1) {
            printf(" | Total value: ??\n");
        } else {
            printf(" | Total value: ?? / ??\n");
        }
    } else {
        if (game->dealer.handCount == 1) {
            printf(" | Total value: %d\n", dealerValues[0]);
        } else {
            printf(" | Total value: %d / %d\n",
                   dealerValues[0],
                   dealerValues[1]);
        }
    }

    if (dealerInfo->rule1 != NULL) {
        printf("Rule: %s\n", dealerInfo->rule1);
    }

    if (twistInfo->id != TWIST_NONE) {
        if (game->run.twistBlocked) {
            printf("Twist: %s (blocked) - %s\n",
                   twistInfo->name,
                   twistInfo->description);
        } else {
            printf("Twist: %s - %s\n",
                   twistInfo->name,
                   twistInfo->description);
        }
    }

    if (game->player.handCount > 1) {
        printf("Player (hand %d/%d): ",
               displayHandIndex + 1,
               game->player.handCount);
    } else {
        printf("Player: ");
    }

    for (cardIndex = 0;
         cardIndex < game->player.hands[displayHandIndex].count;
         cardIndex++) {
        if (cardIndex > 0) {
            printf(" ");
        }

        printCard(&game->player.hands[displayHandIndex].cards[cardIndex]);
    }

    printf(" | Total value: %d\n", playerValue);
    printf("Chips: %d | Bet: %d",
           game->player.chips,
           game->player.bets[displayHandIndex]);

    if (game->mode == MODE_ROGUELIKE &&
        game->loanAmount > 0) {
        printf(" | Loan: %d by F%d",
               game->loanAmount,
               game->loanDueFloor);
    }

    printf("\n");

    {
        int logIndex;
        int sourceIndex;

        for (logIndex = 0; logIndex < LOG_SIZE; logIndex++) {
            sourceIndex = logIndex - (LOG_SIZE - game->log.count);

            if (sourceIndex >= 0) {
                printf("%s\n", game->log.lines[sourceIndex]);
            }
        }
    }
}

