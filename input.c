#define _POSIX_C_SOURCE 200809L

#include "input.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
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

    signalAction.sa_handler = handleSignal;

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

    do {
        bytesRead = read(STDIN_FILENO, &character, 1);
    } while (bytesRead == -1 && errno == EINTR);

    if (bytesRead == 1) {
        return (int)character;
    }

    return -1;
}
