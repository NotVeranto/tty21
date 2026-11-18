#ifndef RENDER_STYLE_H
#define RENDER_STYLE_H

const char *scrReset(void);
const char *scrGreen(void);
const char *scrYellow(void);
const char *scrMagenta(void);
const char *scrBold(void);
const char *scrDim(void);
const char *scrCyan(void);
const char *scrRed(void);
void renderInitColors(void);
void renderHideCursor(void);
void renderShowCursor(void);
int renderAnsiEnabled(void);

#endif
