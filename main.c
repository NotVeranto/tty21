#include "card.h"
#include "hand.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    Deck deck;
    Hand hand = {0};

    srand((unsigned int)time(NULL));

    initDeck(&deck);
    shuffleDeck(&deck);

    addCardToHand(&hand, &deck.cards[0]);
    addCardToHand(&hand, &deck.cards[1]);
    addCardToHand(&hand, &deck.cards[2]);

    printf("Test hand:\n");
    printHand(&hand);

    return 0;
}
