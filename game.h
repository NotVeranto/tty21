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

/* handCount is 1 for every dealer except Twin (Stage 9.5c-2). */
typedef struct {
    Hand hands[2];
    int handCount;
} Dealer;

typedef enum {
    MSG_DEFAULT,
    MSG_WIN,
    MSG_LOSS,
    MSG_PUSH,
    MSG_INFO,
    MSG_ACTION,
    MSG_DEALER
} MessageStyle;

#define LOG_SIZE 6
#define LOG_LINE_LEN 96

typedef struct {
    char lines[LOG_SIZE][LOG_LINE_LEN];
    MessageStyle styles[LOG_SIZE];
    int count;
} GameLog;

struct Game {
    Deck deck;
    Player player;
    Dealer dealer;
    GameState state;
    int roundNumber;
    Run run;
    GameMode mode;
    int peekActive;
    GameLog log;
};

void initGame(Game *game, GameMode mode);
void advanceFloor(Game *game);
void logPush(Game *game, const char *msg, MessageStyle style);
int runGame(Game *game);

#endif
