#ifndef INPUT_H
#define INPUT_H

typedef enum {
    KEY_NONE = 0,
    KEY_CHAR,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_ENTER,
    KEY_ESC,
    KEY_EOF
} KeyType;

typedef struct {
    KeyType type;
    char ch;
} KeyEvent;

int inputInit(void);
void disableRawMode(void);
int inputIsRaw(void);
void flushInput(void);
int getch(void);
KeyEvent readKey(void);

#endif
