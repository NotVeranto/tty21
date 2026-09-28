#include "game.h"
#include "input.h"
#include "rng.h"
#include "version.h"

#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void printUsage(FILE *stream) {
    fprintf(stream,
            "Usage: tty21 [OPTIONS]\n"
            "Terminal blackjack roguelike.\n"
            "\n"
            "Options:\n"
            "  -h, --help      Show this help and exit.\n"
            "  -v, --version   Show version and exit.\n"
            "  -s, --seed N    Use N as the RNG seed (default: time-based).\n");
}

static int parseSeed(const char *text, unsigned int *seed) {
    char *endPointer;
    unsigned long parsedValue;

    if (text[0] == '\0') {
        return -1;
    }

    errno = 0;
    parsedValue = strtoul(text, &endPointer, 10);

    if (errno == ERANGE ||
        endPointer == text ||
        *endPointer != '\0' ||
        parsedValue > UINT_MAX) {
        return -1;
    }

    *seed = (unsigned int)parsedValue;
    return 0;
}

int main(int argc, char **argv) {
    Game game;
    unsigned int seed;
    int argumentIndex;
    int seedResult;
    const char *seedText;

    seed = 0;

    for (argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
        if (strncmp(argv[argumentIndex], "--seed=", 7) == 0) {
            seedText = argv[argumentIndex] + 7;
            seedResult = parseSeed(seedText, &seed);

            if (seedResult != 0) {
                fprintf(stderr, "error: invalid seed '%s'\n", seedText);
                printUsage(stderr);
                return 1;
            }

            continue;
        }

        if (strcmp(argv[argumentIndex], "--seed") == 0 ||
            strcmp(argv[argumentIndex], "-s") == 0) {
            if (argumentIndex + 1 >= argc) {
                fprintf(stderr, "error: missing seed value\n");
                printUsage(stderr);
                return 1;
            }

            argumentIndex++;
            seedText = argv[argumentIndex];
            seedResult = parseSeed(seedText, &seed);

            if (seedResult != 0) {
                fprintf(stderr, "error: invalid seed '%s'\n", seedText);
                printUsage(stderr);
                return 1;
            }

            continue;
        }

        if (strncmp(argv[argumentIndex], "-s", 2) == 0 &&
            argv[argumentIndex][2] != '\0') {
            seedText = argv[argumentIndex] + 2;
            seedResult = parseSeed(seedText, &seed);

            if (seedResult != 0) {
                fprintf(stderr, "error: invalid seed '%s'\n", seedText);
                printUsage(stderr);
                return 1;
            }

            continue;
        }

        if (strcmp(argv[argumentIndex], "-h") == 0 ||
            strcmp(argv[argumentIndex], "--help") == 0) {
            printUsage(stdout);
            return 0;
        }

        if (strcmp(argv[argumentIndex], "-v") == 0 ||
            strcmp(argv[argumentIndex], "--version") == 0) {
            printf("tty21 %s\n", TTY21_VERSION);
            return 0;
        }

        fprintf(stderr, "error: unknown option '%s'\n", argv[argumentIndex]);
        printUsage(stderr);
        return 1;
    }

    setlocale(LC_ALL, "");
    rngInit(seed);

    inputInit();
    initGame(&game);
    runGame(&game);

    return 0;
}
