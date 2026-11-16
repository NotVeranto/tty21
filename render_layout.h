#ifndef RENDER_LAYOUT_H
#define RENDER_LAYOUT_H

#include "game.h"

int getCenteredColumn(int contentWidth);
int getDynamicTableWidth(const Game *game);
void printBoxBorder(int row, int column, int width);
void printBoxSides(int row, int column, int width);
void renderScreenHeader(const char *title, const char *subtitle, int row);

#endif
