#define _POSIX_C_SOURCE 200809L

#include "input.h"
#include "render.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

static struct termios originalTermios;
static struct termios rawTermios;
static struct sigaction signalAction;
static int rawModeEnabled;
static int exitHandlerRegistered;
static volatile sig_atomic_t wasRawMode;

// Restores the terminal and exits with a signal-specific status.
static void handleSignal(int signalNumber) {
    int exitStatus;

    disableRawMode();
    exitStatus = signalNumber == SIGTERM ? 143 : 130;
    _exit(exitStatus);
}

// Restores cooked mode before the process is suspended by the shell.
static void handleSigTstp(int signalNumber) {
    (void)signalNumber;

    wasRawMode = rawModeEnabled != 0;

    if (rawModeEnabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
        rawModeEnabled = 0;
    }

    signalAction.sa_handler = SIG_DFL;
    sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags = 0;
    sigaction(SIGTSTP, &signalAction, NULL);
    raise(SIGTSTP);
}

// Restores raw mode after the shell resumes the process.
static void handleSigCont(int signalNumber) {
    (void)signalNumber;

    if (wasRawMode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermios);
        rawModeEnabled = 1;
        wasRawMode = 0;
    }

    signalAction.sa_handler = handleSigTstp;
    sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags = 0;
    sigaction(SIGTSTP, &signalAction, NULL);
}

int inputInit(void) {
    struct termios currentTermios;
    struct sigaction oldSigintAction;
    struct sigaction oldSigtermAction;

    if (rawModeEnabled) {
        return 0;
    }

    if (!isatty(STDIN_FILENO)) {
        return -1;
    }

    if (tcgetattr(STDIN_FILENO, &originalTermios) == -1) {
        return -1;
    }

    currentTermios = originalTermios;
    currentTermios.c_lflag &= ~(ICANON | ECHO);
    currentTermios.c_iflag &= ~(IXON | ICRNL);
    currentTermios.c_cc[VMIN] = 1;
    currentTermios.c_cc[VTIME] = 0;
    rawTermios = currentTermios;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermios) == -1) {
        return -1;
    }

    rawModeEnabled = 1;
    wasRawMode = 0;

    if (!exitHandlerRegistered) {
        if (atexit(disableRawMode) != 0) {
            disableRawMode();
            return -1;
        }

        exitHandlerRegistered = 1;
    }

    signalAction.sa_handler = handleSignal;
    sigemptyset(&signalAction.sa_mask);
    signalAction.sa_flags = 0;

    if (sigaction(SIGINT, &signalAction, &oldSigintAction) == -1) {
        disableRawMode();
        return -1;
    }

    if (sigaction(SIGTERM, &signalAction, &oldSigtermAction) == -1) {
        sigaction(SIGINT, &oldSigintAction, NULL);
        disableRawMode();
        return -1;
    }

    signalAction.sa_handler = handleSigTstp;

    if (sigaction(SIGTSTP, &signalAction, NULL) == -1) {
        disableRawMode();
        return -1;
    }

    signalAction.sa_handler = handleSigCont;

    if (sigaction(SIGCONT, &signalAction, NULL) == -1) {
        disableRawMode();
        return -1;
    }

    return 0;
}

void disableRawMode(void) {
    if (rawModeEnabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
        rawModeEnabled = 0;
    }

    renderShowCursor();
}

int inputIsRaw(void) {
    return rawModeEnabled;
}

int getch(void) {
    unsigned char character;
    ssize_t bytesRead;

    do {
        bytesRead = read(STDIN_FILENO, &character, 1);
    } while (bytesRead == -1 && errno == EINTR);

    if (bytesRead == 1) {
        return (int)character;
    }

    return -1;
}

static int waitForInputByte(void) {
    fd_set readSet;
    struct timeval timeout;

    FD_ZERO(&readSet);
    FD_SET(STDIN_FILENO, &readSet);
    timeout = (struct timeval){0, 100000};

    return select(STDIN_FILENO + 1, &readSet, NULL, NULL, &timeout);
}

KeyEvent readKey(void) {
    KeyEvent event;
    int character;
    int selectResult;

    event.type = KEY_NONE;
    event.ch = '\0';
    character = getch();

    if (character == -1) {
        event.type = KEY_EOF;
    } else if (character == '\r' || character == '\n') {
        event.type = KEY_ENTER;
    } else if (character == 27) {
        selectResult = waitForInputByte();

        if (selectResult <= 0) {
            event.type = KEY_ESC;
        } else {
            character = getch();

            if (character != '[') {
                event.type = KEY_ESC;
            } else {
                selectResult = waitForInputByte();

                if (selectResult <= 0) {
                    event.type = KEY_ESC;
                } else {
                    character = getch();

                    if (character == 'A') {
                        event.type = KEY_UP;
                    } else if (character == 'B') {
                        event.type = KEY_DOWN;
                    } else if (character == 'C') {
                        event.type = KEY_RIGHT;
                    } else if (character == 'D') {
                        event.type = KEY_LEFT;
                    } else {
                        event.type = KEY_ESC;
                    }
                }
            }
        }
    } else {
        event.type = KEY_CHAR;
        event.ch = (char)character;
    }

    return event;
}
