#include "card.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    Deck deck;
    int cardIndex;

    srand((unsigned int)time(NULL));

    initDeck(&deck);
    shuffleDeck(&deck);

    printf("First 5 cards:\n");

    for (cardIndex = 0; cardIndex < 5; cardIndex++) {
        printCard(&deck.cards[cardIndex]);
    }

    return 0;
}
