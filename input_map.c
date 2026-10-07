#include "input_map.h"

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
    if (ch == 'h' || ch == 'H') return ACTION_HIT;
    if (ch == 's' || ch == 'S') return ACTION_STAND;
    if (ch == 'd' || ch == 'D') return ACTION_DOUBLE;
    if (ch == 'p' || ch == 'P') return ACTION_SPLIT;
    if (ch == 'k' || ch == 'K') return ACTION_PEEK;
    if (ch == 'q' || ch == 'Q') return ACTION_QUIT;
    return (ActionId)-1;
}


int actionToChar(ActionId action) {
    if (action == ACTION_HIT) return 'h';
    if (action == ACTION_STAND) return 's';
    if (action == ACTION_DOUBLE) return 'd';
    if (action == ACTION_SPLIT) return 'p';
    if (action == ACTION_PEEK) return 'k';
    if (action == ACTION_QUIT) return 'q';
    return -1;
}
