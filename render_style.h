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
int renderTerminalSupportsAnsi(int stdoutIsTerminal,
                               const char *term,
                               const char *noColor);
const char *scrAccent(void);
const char *scrSuccess(void);
const char *scrWarning(void);
const char *scrDanger(void);
const char *scrMuted(void);
const char *scrEmphasis(void);
const char *scrCardBorder(void);
const char *scrCardFace(void);
const char *scrCardRedInk(void);
const char *scrCardBlackInk(void);
const char *scrCardBack(void);
const char *scrCardBackInk(void);

#endif
