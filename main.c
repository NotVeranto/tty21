#include "game.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void printUsage(FILE *stream) {
    fprintf(stream,
            "Usage: tty21 [OPTIONS]\n"
            "Terminal blackjack roguelike.\n"
            "\n"
            "Options:\n"
            "  -h, --help      Show this help and exit.\n"
            "  -v, --version   Show version and exit.\n");
}

int main(int argc, char **argv) {
    Game game;
    int argumentIndex;
    int exitCode;

    exitCode = 0;

    for (argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
        if (strcmp(argv[argumentIndex], "-h") == 0 ||
            strcmp(argv[argumentIndex], "--help") == 0) {
            printUsage(stdout);
        } else if (strcmp(argv[argumentIndex], "-v") == 0 ||
                   strcmp(argv[argumentIndex], "--version") == 0) {
            printf("tty21 %s\n", TTY21_VERSION);
        } else {
            fprintf(stderr, "error: unknown option '%s'\n", argv[argumentIndex]);
            printUsage(stderr);
            exitCode = 1;
        }
    }

    if (exitCode == 0 && argc == 1) {
        srand((unsigned int)time(NULL));

        initGame(&game);
        runGame(&game);
    }

    return exitCode;
}
