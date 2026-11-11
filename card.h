#ifndef CARD_H
#define CARD_H

#define DECK_SIZE 52
#define DECK_BURN_SIZE 15

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

typedef enum {
    CURSED_NONE = 0,
    CURSED_BLEED,     // -1 chip while held
    CURSED_FROST,     // -2 to hand value while held
    CURSED_ECHO       // on stand, 10% chance to redraw
} CurseId;

typedef struct {
    Suit suit;
    Rank rank;
    int cursed;   // 0 = normal, 1..N = cursed variant
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
