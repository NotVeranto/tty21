#ifndef CARD_H
#define CARD_H

#define DECK_SIZE 52

typedef enum {
    HEARTS,
    DIAMONDS,
    CLUBS,
    SPADES
} Suit;

typedef enum {
    ACE = 1,
    TWO,
    THREE,
    FOUR,
    FIVE,
    SIX,
    SEVEN,
    EIGHT,
    NINE,
    TEN,
    JACK,
    QUEEN,
    KING
} Rank;

typedef struct {
    Suit suit;
    Rank rank;
} Card;

typedef struct {
    Card cards[DECK_SIZE];
    int top;
} Deck;

void initDeck(Deck *deck);
void shuffleDeck(Deck *deck);
int getCardValue(const Card *card);
void printCard(const Card *card);

#endif
