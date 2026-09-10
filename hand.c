#include "hand.h"

#include <stdio.h>

// Calculates the hand value with blackjack ace conversion.
int calculateHandValue(const Hand *hand) {
    int totalValue, aceCount, cardIndex;

    totalValue = 0;
    aceCount = 0;

    for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
        totalValue += getCardValue(&hand->cards[cardIndex]);

        if (hand->cards[cardIndex].rank == ACE) {
            aceCount++;
        }
    }

    while (totalValue > 21 && aceCount > 0) {
        totalValue -= 10;
        aceCount--;
    }

    return totalValue;
}

// Adds a card to the hand and updates the cached value.
void addCardToHand(Hand *hand, const Card *card) {
    if (hand->count < MAX_HAND_SIZE) {
        hand->cards[hand->count] = *card;
        hand->count++;
        hand->value = calculateHandValue(hand);
    }
}

// Clears all cards and resets the cached value.
void resetHand(Hand *hand) {
    hand->count = 0;
    hand->value = 0;
}

// Prints all cards on one line followed by the total value.
void printHand(const Hand *hand) {
    int cardIndex;

    for (cardIndex = 0; cardIndex < hand->count; cardIndex++) {
        printCard(&hand->cards[cardIndex]);
        printf(" ");
    }

    printf("Total value: %d\n", hand->value);
}
