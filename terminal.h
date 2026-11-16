#ifndef TERMINAL_H
#define TERMINAL_H

#define MIN_TERMINAL_WIDTH  70
#define MIN_TERMINAL_HEIGHT 42

void terminalGetSize(int *width, int *height);
int terminalDimensionsOk(int width, int height);

#endif
