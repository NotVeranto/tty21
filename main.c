#include "game.h"

#include <stdlib.h>
#include <time.h>

int main(void) {
    Game game;

    srand((unsigned int)time(NULL));

    initGame(&game);
    runGame(&game);

    return 0;
}
