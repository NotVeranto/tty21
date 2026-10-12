#ifndef RUN_H
#define RUN_H

#include "dealers.h"
#include "twists.h"

typedef enum {
    MODE_ROGUELIKE = 0,
    MODE_CLASSIC
} GameMode;

typedef struct Game Game;

#define MAX_EXTRA_TWISTS 4

typedef struct {
    int floor;
    int marks;
    DealerId dealerId;
    DealerId previousDealerId;
    TwistId twistId;
    TwistId previousTwistId;
    TwistId extraTwists[MAX_EXTRA_TWISTS];
    int extraTwistCount;
    int roundsOnFloor;
    int twistBlocked;      /* 1 if current twist is neutralized */
    int twistRerolled;     /* 1 if reroll used this floor */
    int bossRewarded;      /* 1 if current boss floor already paid */
} Run;

void initRun(Run *run);
int getMinBet(const Game *game, int floor, GameMode mode);

#endif
