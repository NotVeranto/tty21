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

typedef struct {
    Hand hand;
    int chips;
    int bet;
    int perkOwned[PERK_COUNT];
    int perkCharges[PERK_COUNT];
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
int calculateDealerHandValue(const Hand *hand, DealerId id);
int calculatePlayerHandValue(const Game *game);
void startRound(Game *game);
void playerTurn(Game *game);
void dealerTurn(Game *game);
void resolveRound(Game *game);
void advanceFloor(Game *game);
void floorMenu(Game *game);
void runShop(Game *game);
void printGameState(const Game *game, int hideDealerCard);
int runGame(Game *game);

#endif
