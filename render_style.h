#ifndef RENDER_STYLE_H
#define RENDER_STYLE_H

#define SCR_CURSOR "\033[%d;%dH"

const char *scrReset(void);
const char *scrBold(void);
const char *scrDim(void);
const char *scrCyan(void);
void renderInitColors(void);

#endif
