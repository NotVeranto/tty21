#define _POSIX_C_SOURCE 200809L

#include "input.h"

#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

static struct termios originalTermios;
static int rawModeEnabled;
static int exitHandlerRegistered;

// Restores the terminal and exits with a signal-specific status.
static void handleSignal(int signalNumber) {
    int exitStatus;

    disableRawMode();
    exitStatus = signalNumber == SIGTERM ? 143 : 130;
    _exit(exitStatus);
}

int inputInit(void) {
    struct termios rawTermios;
    struct sigaction signalAction;
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

    rawTermios = originalTermios;
    rawTermios.c_lflag &= ~(ICANON | ECHO);
    rawTermios.c_iflag &= ~(IXON | ICRNL);
    rawTermios.c_cc[VMIN] = 1;
    rawTermios.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &rawTermios) == -1) {
        return -1;
    }

    rawModeEnabled = 1;

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

    return 0;
}

void disableRawMode(void) {
    if (rawModeEnabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios);
        rawModeEnabled = 0;
    }
}

int inputIsRaw(void) {
    return rawModeEnabled;
}

int getch(void) {
    unsigned char character;
    ssize_t bytesRead;

    bytesRead = read(STDIN_FILENO, &character, 1);

    if (bytesRead == 1) {
        return (int)character;
    }

    return -1;
}
