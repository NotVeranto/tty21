#ifndef RNG_H
#define RNG_H

void rngInit(unsigned int seed, int seedIsValid);
unsigned int rngNext(void);
unsigned int rngGetSeed(void);
unsigned long rngGetCallCount(void);
int rngRange(int minInclusive, int maxInclusive);

#endif
