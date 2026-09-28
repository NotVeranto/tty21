#ifndef RUN_H
#define RUN_H

#include "dealers.h"
#include "twists.h"

typedef struct {
    int floor;
    int souls;
    DealerId dealerId;
    DealerId previousDealerId;
    TwistId twistId;
    TwistId previousTwistId;
} Run;

void initRun(Run *run);
int getMinBet(int floor);

#endif
