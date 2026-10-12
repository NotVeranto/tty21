#include "card.h"
#include "rng.h"

#include <stdio.h>

static const char *RANK_NAMES[] = {
    "", "A", "2", "3", "4", "5", "6",
    "7", "8", "9", "10", "J", "Q", "K"
};

static const char *SUIT_SYMBOLS[] = {
    "♥", "♦", "♣", "♠"
};

// Fills the deck with all 52 card combinations.
void initDeck(Deck *deck) {
    int suitIndex, rankIndex, cardIndex;

    cardIndex = 0;

    for (suitIndex = HEARTS; suitIndex <= SPADES; suitIndex++) {
        for (rankIndex = ACE; rankIndex <= KING; rankIndex++) {
            deck->cards[cardIndex].suit = (Suit)suitIndex;
            deck->cards[cardIndex].rank = (Rank)rankIndex;
            deck->cards[cardIndex].cursed = CURSED_NONE;
            cardIndex++;
        }
    }

    deck->top = 0;
}

// Shuffles the deck using the Fisher-Yates algorithm.
void shuffleDeck(Deck *deck) {
    int cardIndex, randomIndex;
    Card temporaryCard;

    for (cardIndex = DECK_SIZE - 1; cardIndex > 0; cardIndex--) {
        randomIndex = rngRange(0, cardIndex);

        temporaryCard = deck->cards[cardIndex];
        deck->cards[cardIndex] = deck->cards[randomIndex];
        deck->cards[randomIndex] = temporaryCard;
    }

    deck->top = 0;
}

// Returns the basic blackjack value of a card.
int getCardValue(const Card *card) {
    if (card->rank == ACE) {
        return 11;
    }

    if (card->rank >= JACK) {
        return 10;
    }

    return card->rank;
}

// Prints a card using its rank and Unicode suit symbol.
void printCard(const Card *card) {
    printf("%s%s", RANK_NAMES[card->rank], SUIT_SYMBOLS[card->suit]);
    if (card->cursed != CURSED_NONE) {
        printf("*");
    }
}
