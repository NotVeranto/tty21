#include "card.h"
#include "hand.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(void) {
    Deck deck;
    Hand hand = {0};
    Card aceCard, kingCard, fiveCard;

    srand((unsigned int)time(NULL));

    initDeck(&deck);
    shuffleDeck(&deck);

    aceCard.suit = SPADES;
    aceCard.rank = ACE;
    kingCard.suit = HEARTS;
    kingCard.rank = KING;
    fiveCard.suit = CLUBS;
    fiveCard.rank = FIVE;

    addCardToHand(&hand, &aceCard);
    addCardToHand(&hand, &kingCard);
    addCardToHand(&hand, &fiveCard);

    printf("Test hand with ace conversion:\n");
    printHand(&hand);

    return 0;
}
