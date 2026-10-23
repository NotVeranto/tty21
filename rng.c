#define _POSIX_C_SOURCE 200809L

#include "rng.h"

#include <stdlib.h>
#include <time.h>

static unsigned int rngSeed;
static unsigned long rngCalls;

void rngInit(unsigned int seed, int seedIsValid) {
    if (!seedIsValid) {
        seed = (unsigned int)time(NULL);
    }

    rngSeed = seed;
    rngCalls = 0;
    srand(rngSeed);
}

unsigned int rngNext(void) {
    rngCalls++;
    return (unsigned int)rand();
}

int rngRange(int minInclusive, int maxInclusive) {
    int minimum;
    int maximum;
    unsigned long long range;
    unsigned long long limit;
    unsigned int randomValue;

    minimum = minInclusive;
    maximum = maxInclusive;

    if (minimum > maximum) {
        minimum = maxInclusive;
        maximum = minInclusive;
    }

    range = (unsigned long long)((long long)maximum - minimum + 1);
    limit = (unsigned long long)RAND_MAX;
    limit -= limit % range;

    do {
        randomValue = rngNext();
    } while ((unsigned long long)randomValue >= limit);

    return minimum + (int)((unsigned long long)randomValue % range);
}


unsigned int rngGetSeed(void) {
    return rngSeed;
}

unsigned long rngGetCallCount(void) {
    return rngCalls;
}
