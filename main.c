#include "game.h"
#include "input.h"
#include "rng.h"
#include "version.h"

#include <ctype.h>
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
            "  -s, --seed N    Use N as the RNG seed (default: time-based).\n"
            "  -r, --roguelike Start in roguelike mode (default).\n"
            "  -c, --classic   Start in classic blackjack mode.\n");
}

static GameMode runStartupMenu(void) {
    char input[64];
    int characterIndex;

    while (1) {
        printf("\n"
               " TTTTT TTTTT Y   Y 22222  11\n"
               "   T     T    Y Y     2   11\n"
               "   T     T     Y     2    11\n"
               "   T     T     Y    2     11\n"
               "   T     T     Y   22222  11\n"
               "\n"
               "\033[2mTerminal blackjack roguelike\033[0m\n"
               "\n"
               "tty21 — select mode\n"
               "  [1] Roguelike\n"
               "  [2] Classic blackjack\n"
               "  [q] Quit\n"
               "Choice: ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            exit(0);
        }

        characterIndex = 0;

        while (input[characterIndex] != '\0' &&
               isspace((unsigned char)input[characterIndex])) {
            characterIndex++;
        }

        if (input[characterIndex] == '1') {
            return MODE_ROGUELIKE;
        }

        if (input[characterIndex] == '2') {
            return MODE_CLASSIC;
        }

        if (input[characterIndex] == 'q') {
            exit(0);
        }
    }
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
    int forced;
    GameMode forcedMode;
    GameMode mode;

    seed = 0;
    forced = 0;
    forcedMode = MODE_ROGUELIKE;

    for (argumentIndex = 1; argumentIndex < argc; argumentIndex++) {
        if (strcmp(argv[argumentIndex], "--roguelike") == 0 ||
            strcmp(argv[argumentIndex], "-r") == 0) {
            forced = 1;
            forcedMode = MODE_ROGUELIKE;
            continue;
        }

        if (strcmp(argv[argumentIndex], "--classic") == 0 ||
            strcmp(argv[argumentIndex], "-c") == 0) {
            forced = 1;
            forcedMode = MODE_CLASSIC;
            continue;
        }

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

    if (forced) {
        mode = forcedMode;
    } else {
        mode = runStartupMenu();
    }

    rngInit(seed);

    inputInit();
    initGame(&game, mode);
    runGame(&game);

    return 0;
}
