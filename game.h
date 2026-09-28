#ifndef GAME_H
#define GAME_H

#include "card.h"
#include "hand.h"
#include "dealers.h"
#include "twists.h"

typedef enum {
    STATE_BET,
    STATE_PLAYER_TURN,
    STATE_DEALER_TURN,
    STATE_RESULT,
    STATE_MENU,
    STATE_QUIT
} GameState;

typedef struct {
    Hand hand;
    int chips;
    int bet;
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
    int floor;
    int souls;
    DealerId dealerId;
    DealerId previousDealerId;
    TwistId twistId;
} Game;

void initGame(Game *game);
int getMinBet(int floor);
int calculateDealerHandValue(const Hand *hand, DealerId id);
void startRound(Game *game);
void playerTurn(Game *game);
void dealerTurn(Game *game);
void resolveRound(Game *game);
void advanceFloor(Game *game);
void floorMenu(Game *game);
void printGameState(const Game *game, int hideDealerCard);
int runGame(Game *game);

#endif
