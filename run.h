#ifndef RUN_H
#define RUN_H

#include "dealers.h"
#include "twists.h"

typedef enum {
    MODE_ROGUELIKE = 0,
    MODE_CLASSIC
} GameMode;

typedef struct Game Game;

typedef struct {
    int floor;
    int marks;
    DealerId dealerId;
    DealerId previousDealerId;
    TwistId twistId;
    TwistId previousTwistId;
} Run;

void initRun(Run *run);
int getMinBet(const Game *game, int floor, GameMode mode);

#endif
