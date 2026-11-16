#ifndef RENDER_LAYOUT_H
#define RENDER_LAYOUT_H

int getCenteredColumn(int contentWidth);
void printBoxBorder(int row, int column, int width);
void printBoxSides(int row, int column, int width);
void renderScreenHeader(const char *title, const char *subtitle, int row);

#endif
