#ifndef RNG_H
#define RNG_H

void rngInit(unsigned int seed);
unsigned int rngNext(void);
int rngRange(int minInclusive, int maxInclusive);

#endif
