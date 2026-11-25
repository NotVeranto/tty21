#include "round_cards.h"
#include "round.h"
#include "round_rules.h"

static int countCursedFrost(const Hand *hand) {
    int count;
    int cardIndex;

    count = 0;

    for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
        if (hand->cards[cardIndex].cursed == CURSED_FROST) {
            count++;
        }
    }

    return count;
}

static int calculateHandValueAcesLow(const Hand *hand) {
    int total;
    int i;

    total = 0;

    for (i = 0; i < hand->count; i++) {
        if (hand->cards[i].rank == ACE) {
            total += 1;
        } else {
            total += getCardValue(&hand->cards[i]);
        }
    }

    return total;
}

int calculateDealerHandValue(const Game *game,
                             const Hand *hand,
                             DealerId id) {
    if (twistActive(game, TWIST_ACES_LOW)) {
        return calculateHandValueAcesLow(hand);
    }

    if (effectiveDealerAcesCountAsEleven(game, id)) {
        int total = 0;
        int cardIndex;
        for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
            total += getCardValue(&hand->cards[cardIndex]);
        }
        return total;
    }

    if (!effectiveDealerAcesCountAsOne(game, id)) {
        return calculateHandValue(hand);
    }

    return calculateHandValueAcesLow(hand);
}

int calculatePlayerHandValue(const Game *game) {
    int totalValue;
    int frostCount;

    if (twistActive(game, TWIST_ACES_LOW)) {
        totalValue = calculateHandValueAcesLow(
            &game->player.hands[game->player.activeHand]);
    } else if (!game->player.perkOwned[PERK_ACE_HIGH]) {
        totalValue = game->player.hands[game->player.activeHand].value;
    } else {
        int cardIndex;

        totalValue = 0;

        for (cardIndex = 0;
             cardIndex < game->player.hands[game->player.activeHand].count;
             cardIndex++) {
            totalValue +=
                getCardValue(&game->player.hands[game->player.activeHand]
                                 .cards[cardIndex]);
        }
    }

    frostCount = countCursedFrost(
        &game->player.hands[game->player.activeHand]);
    totalValue -= frostCount * 2;

    if (totalValue < 0) {
        totalValue = 0;
    }

    return totalValue;
}

int calculatePlayerHandValueForIndex(const Game *game,
                                     int handIndex) {
    int totalValue;
    int frostCount;

    if (twistActive(game, TWIST_ACES_LOW)) {
        totalValue = calculateHandValueAcesLow(
            &game->player.hands[handIndex]);
    } else if (!game->player.perkOwned[PERK_ACE_HIGH]) {
        totalValue = game->player.hands[handIndex].value;
    } else {
        int cardIndex;

        totalValue = 0;

        for (cardIndex = 0;
             cardIndex < game->player.hands[handIndex].count;
             cardIndex++) {
            totalValue +=
                getCardValue(&game->player.hands[handIndex]
                                 .cards[cardIndex]);
        }
    }

    frostCount = countCursedFrost(
        &game->player.hands[handIndex]);
    totalValue -= frostCount * 2;

    if (totalValue < 0) {
        totalValue = 0;
    }

    return totalValue;
}
