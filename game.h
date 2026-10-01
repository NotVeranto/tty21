#ifndef GAME_H
#define GAME_H

#include "card.h"
#include "hand.h"
#include "dealers.h"
#include "twists.h"
#include "perks.h"
#include "run.h"

typedef enum {
    STATE_BET,
    STATE_PLAYER_TURN,
    STATE_DEALER_TURN,
    STATE_RESULT,
    STATE_SHOP,
    STATE_MENU,
    STATE_QUIT
} GameState;

#define MAX_PLAYER_HANDS 4

typedef struct {
    Hand hands[MAX_PLAYER_HANDS];
    int bets[MAX_PLAYER_HANDS];
    int handCount;
    int activeHand;
    int isSplitAce[MAX_PLAYER_HANDS];
    int chips;
    int perkOwned[PERK_COUNT];
    int perkCharges[PERK_COUNT];
    int usedSplit;   /* set if the player used split this round */
    int usedDouble;  /* set if the player used double this round */
} Player;

typedef struct {
    Hand hand;
} Dealer;

typedef struct {
    Deck deck;
    Player player;
    Dealer dealer;
    GameState state;
    int roundNumber;
    Run run;
    int peekActive;
} Game;

void initGame(Game *game);
void advanceFloor(Game *game);
int runGame(Game *game);

#endif
