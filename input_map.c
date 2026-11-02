#include "input_map.h"

#include <ctype.h>

int moveSelection(int selectedIndex,
                   int itemCount,
                   int skippedMask,
                   int direction) {
    int nextIndex;
    int attemptCount;

    nextIndex = selectedIndex;
    attemptCount = 0;

    while (attemptCount < itemCount) {
        nextIndex += direction;

        if (nextIndex < 0) {
            nextIndex = itemCount - 1;
        } else if (nextIndex >= itemCount) {
            nextIndex = 0;
        }

        if ((skippedMask & (1 << nextIndex)) == 0) {
            return nextIndex;
        }

        attemptCount++;
    }

    return selectedIndex;
}

ActionId mapPlayerKey(char ch) {
    switch (tolower((unsigned char)ch)) {
        case 'h': return ACTION_HIT;
        case 's': return ACTION_STAND;
        case 'd': return ACTION_DOUBLE;
        case 'p': return ACTION_SPLIT;
        case 'k': return ACTION_PEEK;
        case 'u': return ACTION_SURRENDER;
        case 'q': return ACTION_QUIT;
        default: return (ActionId)-1;
    }
}


int actionToChar(ActionId action) {
    if (action == ACTION_HIT) return 'h';
    if (action == ACTION_STAND) return 's';
    if (action == ACTION_DOUBLE) return 'd';
    if (action == ACTION_SPLIT) return 'p';
    if (action == ACTION_PEEK) return 'k';
    if (action == ACTION_SURRENDER) return 'u';
    if (action == ACTION_QUIT) return 'q';
    return -1;
}
