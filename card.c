#include "card.h"

#include <stdio.h>
#include <stdlib.h>

// Fills the deck with all 52 card combinations.
void initDeck(Deck *deck) {
    int suitIndex, rankIndex, cardIndex;

    cardIndex = 0;

    for (suitIndex = HEARTS; suitIndex <= SPADES; suitIndex++) {
        for (rankIndex = ACE; rankIndex <= KING; rankIndex++) {
            deck->cards[cardIndex].suit = (Suit)suitIndex;
            deck->cards[cardIndex].rank = (Rank)rankIndex;
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
        randomIndex = rand() % (cardIndex + 1);

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
    const char *rankNames[] = {
        "", "A", "2", "3", "4", "5", "6",
        "7", "8", "9", "10", "J", "Q", "K"
    };

    const char *suitSymbols[] = {
        "♥", "♦", "♣", "♠"
    };

    printf("%s%s", rankNames[card->rank], suitSymbols[card->suit]);
}
