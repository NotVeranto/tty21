#ifndef HAND_H
#define HAND_H

#include "card.h"

#define MAX_HAND_SIZE 12

typedef struct {
    Card cards[MAX_HAND_SIZE];
    int count;
    int value;
} Hand;

void addCardToHand(Hand *hand, const Card *card);
int calculateHandValue(const Hand *hand);
void resetHand(Hand *hand);
void printHand(const Hand *hand);

#endif
