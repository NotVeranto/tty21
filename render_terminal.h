#ifndef RENDER_TERMINAL_H
#define RENDER_TERMINAL_H

#include "terminal.h"

int renderTerminalDimensionsOk(int width, int height);
int renderTerminalSizeOk(int *width, int *height);
void renderTooSmallScreen(int currentWidth, int currentHeight);

#endif
