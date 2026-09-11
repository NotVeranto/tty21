#ifndef GAME_H
#define GAME_H

#include "card.h"
#include "hand.h"

typedef enum {
    STATE_BET,
    STATE_PLAYER_TURN,
    STATE_DEALER_TURN,
    STATE_RESULT,
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
} Game;

void initGame(Game *game);
void startRound(Game *game);
void playerTurn(Game *game);
void dealerTurn(Game *game);
void resolveRound(Game *game);
void printGameState(const Game *game, int hideDealerCard);
int runGame(Game *game);

#endif
