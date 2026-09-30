#ifndef INPUT_MAP_H
#define INPUT_MAP_H

typedef enum {
    ACTION_HIT = 0,
    ACTION_STAND,
    ACTION_DOUBLE,
    ACTION_SPLIT,
    ACTION_PEEK,
    ACTION_QUIT
} ActionId;

ActionId mapPlayerKey(char ch);
int moveSelection(int selectedIndex,
                   int itemCount,
                   int skippedMask,
                   int direction);
int actionToChar(ActionId action);

#endif
