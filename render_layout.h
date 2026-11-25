#ifndef RENDER_LAYOUT_H
#define RENDER_LAYOUT_H

#include <stddef.h>

int getCenteredColumn(int contentWidth);\nint formatPlainScreenHeader(char *buffer, size_t bufferSize,\n                            int terminalWidth, const char *title);
int getCenteredColumnForWidth(int terminalWidth, int contentWidth);
int formatPlainMenuItem(char *buffer, size_t bufferSize, char hotkey,
                        const char *label, int selected, int disabled);
void printBoxBorder(int row, int column, int width);
void printBoxSides(int row, int column, int width);
void renderScreenHeader(const char *title, const char *subtitle, int row);
void renderMenuItemRow(int row, int column, char hotkey, const char *label,
                       int selected, int disabled, int completed);

#endif
